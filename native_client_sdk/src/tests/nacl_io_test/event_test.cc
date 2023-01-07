/* Copyright 2013 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/time.h>

#include "gtest/gtest.h"

#include "nacl_io/event_emitter.h"
#include "nacl_io/event_listener.h"
#include "nacl_io/event_listener.h"
#include "nacl_io/event_listener.h"
#include "nacl_io/kernel_intercept.h"
#include "nacl_io/kernel_proxy.h"
#include "nacl_io/kernel_wrap.h"
#include "nacl_io/pipe/pipe_node.h"
#include "nacl_io/stream/stream_fs.h"

#include "ppapi_simple/ps.h"

using namespace nacl_io;
using namespace sdk_util;

class EventListenerTester : public EventListener {
 public:
  EventListenerTester() : EventListener(), events_(0) {};

  virtual void ReceiveEvents(EventEmitter* emitter, uint32_t events) {
    events_ |= events;
  }

  uint32_t Events() { return events_; }

  void Clear() { events_ = 0; }

  uint32_t events_;
};

TEST(EmitterBasic, SingleThread) {
  EventListenerTester listener_a;
  EventListenerTester listener_b;
  EventEmitter emitter;

  emitter.RegisterListener(&listener_a, POLLIN | POLLOUT | POLLERR);
  emitter.RegisterListener(&listener_b, POLLIN | POLLOUT | POLLERR);

  EXPECT_EQ(0, emitter.GetEventStatus());
  EXPECT_EQ(0, listener_a.Events());

  {
    AUTO_LOCK(emitter.GetLock())
    emitter.RaiseEvents_Locked(POLLIN);
  }
  EXPECT_EQ(POLLIN, listener_a.Events());

  listener_a.Clear();

  {
    AUTO_LOCK(emitter.GetLock())
    emitter.RaiseEvents_Locked(POLLOUT);
  }
  EXPECT_EQ(POLLOUT, listener_a.Events());
  EXPECT_EQ(POLLIN | POLLOUT, listener_b.Events());
}

class EmitterTest : public ::testing::Test {
 public:
  void SetUp() {
    pthread_cond_init(&multi_cond_, NULL);
    waiting_ = 0;
    signaled_ = 0;
  }

  void TearDown() { pthread_cond_destroy(&multi_cond_); }

  pthread_t CreateThread() {
    pthread_t id;
    EXPECT_EQ(0, pthread_create(&id, NULL, ThreadThunk, this));
    return id;
  }

  static void* ThreadThunk(void* ptr) {
    return static_cast<EmitterTest*>(ptr)->ThreadEntry();
  }

  void* ThreadEntry() {
    EventListenerLock listener(&emitter_);

    pthread_cond_signal(&multi_cond_);
    waiting_++;
    EXPECT_EQ(0, listener.WaitOnEvent(POLLIN, -1));
    emitter_.ClearEvents_Locked(POLLIN);
    AUTO_LOCK(signaled_lock_);
    signaled_++;
    return NULL;
  }

  int GetSignaledCount() {
    AUTO_LOCK(signaled_lock_);
    return signaled_;
  }

 protected:
  pthread_cond_t multi_cond_;
  EventEmitter emitter_;
  int waiting_;

 private:
  int signaled_;
  sdk_util::SimpleLock signaled_lock_;
};

// Temporarily disabled since it seems to be causing lockup in whe
// KernelWrapTests later on.
// TODO(sbc): renable once we fix http://crbug.com/378596
const int NUM_THREADS = 10;
TEST_F(EmitterTest, DISABLED_MultiThread) {
  pthread_t threads[NUM_THREADS];

  for (int a = 0; a < NUM_THREADS; a++)
    threads[a] = CreateThread();

  {
    AUTO_LOCK(emitter_.GetLock());

    // Wait for all threads to wait
    while (waiting_ < NUM_THREADS)
      pthread_cond_wait(&multi_cond_, emitter_.GetLock().mutex());

    ASSERT_EQ(0, GetSignaledCount());

    emitter_.RaiseEvents_Locked(POLLIN);
  }

  // sleep for 50 milliseconds
  struct timespec sleeptime = {0, 50 * 1000 * 1000};
  nanosleep(&sleeptime, NULL);

  EXPECT_EQ(1, GetSignaledCount());

  {
    AUTO_LOCK(emitter_.GetLock());
    emitter_.RaiseEvents_Locked(POLLIN);
  }

  nanosleep(&sleeptime, NULL);
  EXPECT_EQ(2, GetSignaledCount());

  // Clean up remaining threads.
  while (GetSignaledCount() < waiting_) {
    AUTO_LOCK(emitter_.GetLock());
    emitter_.RaiseEvents_Locked(POLLIN);
  }

  for (int a = 0; a < NUM_THREADS; a++)
    pthread_join(threads[a], NULL);
}

TEST(EventListenerPollTest, WaitForAny) {
  ScopedEventEmitter emitter1(new EventEmitter());
  ScopedEventEmitter emitter2(new EventEmitter());
  ScopedEventEmitter emitter3(new EventEmitter());
  EventListenerPoll listener;
  EventRequest requests[3] = {
      {emitter1, 0, 0}, {emitter2, 0, 0}, {emitter3, 0, 0}, };
  Error error =
      listener.WaitOnAny(requests, sizeof(requests) / sizeof(requests[0]), 1);
  ASSERT_EQ(ETIMEDOUT, error);
}

TEST(PipeTest, Listener) {
  const char hello[] = "Hello World.";
  char tmp[64] = "Goodbye";

  PipeEventEmitter pipe(32);

  // Expect to time out on input.
  {
    EventListenerLock locker(&pipe);
    EXPECT_EQ(ETIMEDOUT, locker.WaitOnEvent(POLLIN, 0));
  }

  // Output should be ready to go.
  {
    EventListenerLock locker(&pipe);
    EXPECT_EQ(0, locker.WaitOnEvent(POLLOUT, 0));
    int out_bytes = 0;
    EXPECT_EQ(0, pipe.Write_Locked(hello, sizeof(hello), &out_bytes));
    EXPECT_EQ(sizeof(hello), out_bytes);
  }

  // We should now be able to poll
  {
    EventListenerLock locker(&pipe);
    EXPECT_EQ(0, locker.WaitOnEvent(POLLIN, 0));
    int out_bytes = -1;
    EXPECT_EQ(0, pipe.Read_Locked(tmp, sizeof(tmp), &out_bytes));
    EXPECT_EQ(sizeof(hello), out_bytes);
  }

  // Verify we can read it correctly.
  EXPECT_EQ(0, strcmp(hello, tmp));
}

class StreamFsForTesting : public StreamFs {
 public:
  StreamFsForTesting() {}
};

TEST(PipeNodeTest, Basic) {
  ScopedFilesystem fs(new StreamFsForTesting());

  PipeNode* pipe_node = new PipeNode(fs.get());
  ScopedRef<PipeNode> pipe(pipe_node);

  EXPECT_EQ(POLLOUT, pipe_node->GetEventStatus());
}

const int MAX_FDS = 32;
class SelectPollTest : public ::testing::Test {
 public:
  void SetUp() {
    kp = new KernelProxy();
    kp->Init(NULL);
    EXPECT_EQ(0, kp->umount("/"));
    EXPECT_EQ(0, kp->mount("", "/", "memfs", 0, NULL));

    memset(&tv, 0, sizeof(tv));
  }

  void TearDown() { delete kp; }

  void SetFDs(int* fds, int cnt) {
    FD_ZERO(&rd_set);
    FD_ZERO(&wr_set);
    FD_ZERO(&ex_set);

    for (int index = 0; index < cnt; index++) {
      EXPECT_NE(-1, fds[index]);
      FD_SET(fds[index], &rd_set);
      FD_SET(fds[index], &wr_set);
      FD_SET(fds[index], &ex_set);

      pollfds[index].fd = fds[index];
      pollfds[index].events = POLLIN | POLLOUT;
      pollfds[index].revents = -1;
    }
  }

  void CloseFDs(int* fds, int cnt) {
    for (int index = 0; index < cnt; index++)
      kp->close(fds[index]);
  }

 protected:
  KernelProxy* kp;

  timeval tv;
  fd_set rd_set;
  fd_set wr_set;
  fd_set ex_set;
  struct pollfd pollfds[MAX_FDS];
};

TEST_F(SelectPollTest, PollMemPipe) {
  int fds[2];

  // Both FDs for regular files should be read/write but not exception.
  fds[0] = kp->open("/test.txt", O_CREAT | O_WRONLY, 0777);
  fds[1] = kp->open("/test.txt", O_RDONLY, 0);
  ASSERT_GT(fds[0], -1);
  ASSERT_GT(fds[1], -1);

  SetFDs(fds, 2);

  ASSERT_EQ(2, kp->poll(pollfds, 2, 0));
  ASSERT_EQ(POLLIN | POLLOUT, pollfds[0].revents);
  ASSERT_EQ(POLLIN | POLLOUT, pollfds[1].revents);
  CloseFDs(fds, 2);

  // The write FD should select for write-only, read FD should not select
  ASSERT_EQ(0, kp->pipe(fds));
  SetFDs(fds, 2);

  ASSERT_EQ(2, kp->poll(pollfds, 2, 0));
  // TODO(bradnelson) fix poll based on open mode
  // EXPECT_EQ(0, pollfds[0].revents);
  // Bug 291018
  ASSERT_EQ(POLLOUT, pollfds[1].revents);

  CloseFDs(fds, 2);
}

TEST_F(SelectPollTest, SelectMemPipe) {
  int fds[2];

  // Both FDs for regular files should be read/write but not exception.
  fds[0] = kp->open("/test.txt", O_CREAT | O_WRONLY, 0777);
  fds[1] = kp->open("/test.txt", O_RDONLY, 0);
  ASSERT_GT(fds[0], -1);
  ASSERT_GT(fds[1], -1);
  SetFDs(fds, 2);

  ASSERT_EQ(4, kp->select(fds[1] + 1, &rd_set, &wr_set, &ex_set, &tv));
  EXPECT_NE(0, FD_ISSET(fds[0], &rd_set));
  EXPECT_NE(0, FD_ISSET(fds[1], &rd_set));
  EXPECT_NE(0, FD_ISSET(fds[0], &wr_set));
  EXPECT_NE(0, FD_ISSET(fds[1], &wr_set));
  EXPECT_EQ(0, FD_ISSET(fds[0], &ex_set));
  EXPECT_EQ(0, FD_ISSET(fds[1], &ex_set));

  CloseFDs(fds, 2);

  // The write FD should select for write-only, read FD should not select
  ASSERT_EQ(0, kp->pipe(fds));
  SetFDs(fds, 2);

  ASSERT_EQ(2, kp->select(fds[1] + 1, &rd_set, &wr_set, &ex_set, &tv));
  EXPECT_EQ(0, FD_ISSET(fds[0], &rd_set));
  EXPECT_EQ(0, FD_ISSET(fds[1], &rd_set));
  // TODO(bradnelson) fix poll based on open mode
  // EXPECT_EQ(0, FD_ISSET(fds[0], &wr_set));
  // Bug 291018
  EXPECT_NE(0, FD_ISSET(fds[1], &wr_set));
  EXPECT_EQ(0, FD_ISSET(fds[0], &ex_set));
  EXPECT_EQ(0, FD_ISSET(fds[1], &ex_set));
}

/**
 * Test that calling select() only writes the initial parts of the fd_sets
 * passed in.
 * We had an issue when select() was calling FD_ZERO() on the incoming fd_sets
 * which was causing corruption in ssh which always allocates the fd_sets to be
 * hold 'nfds' worth of descriptors.
 */
TEST_F(SelectPollTest, SelectPartialFdset) {
  int fds[2];

  // Both FDs for regular files should be read/write but not exception.
  fds[0] = kp->open("/test.txt", O_CREAT | O_WRONLY, 0777);
  fds[1] = kp->open("/test.txt", O_RDONLY, 0);
  ASSERT_GT(fds[0], -1);
  ASSERT_GT(fds[1], -1);
  ASSERT_LT(fds[1], 8);
  SetFDs(fds, 2);

  // Fill in all the remaining bytes in the fd_sets a constant value
  static const char guard_value = 0xab;
  for (int i = 1; i < sizeof(fd_set); i++) {
    ((char*)&rd_set)[i] = guard_value;
    ((char*)&wr_set)[i] = guard_value;
    ((char*)&ex_set)[i] = guard_value;
  }

  ASSERT_EQ(4, kp->select(fds[1] + 1, &rd_set, &wr_set, &ex_set, &tv));

  // Verify guard values were not touched.
  for (int i = 1; i < sizeof(fd_set); i++) {
    ASSERT_EQ(guard_value, ((char*)&rd_set)[i]);
    ASSERT_EQ(guard_value, ((char*)&wr_set)[i]);
    ASSERT_EQ(guard_value, ((char*)&ex_set)[i]);
  }
}
