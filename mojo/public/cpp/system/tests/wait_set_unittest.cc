// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "mojo/public/cpp/system/wait_set.h"

#include <set>
#include <string_view>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/platform_thread.h"
#include "base/threading/simple_thread.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "mojo/public/cpp/system/wait.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace {

using WaitSetTest = testing::Test;

void WriteMessage(const ScopedMessagePipeHandle& handle,
                  const std::string_view& message) {
  MojoResult rv = WriteMessageRaw(handle.get(), message.data(),
                                  static_cast<uint32_t>(message.size()),
                                  nullptr, 0, MOJO_WRITE_MESSAGE_FLAG_NONE);
  CHECK_EQ(MOJO_RESULT_OK, rv);
}

std::string ReadMessage(const ScopedMessagePipeHandle& handle) {
  std::vector<uint8_t> bytes;
  MojoResult rv = ReadMessageRaw(handle.get(), &bytes, nullptr,
                                 MOJO_READ_MESSAGE_FLAG_NONE);
  CHECK_EQ(MOJO_RESULT_OK, rv);
  return std::string(bytes.begin(), bytes.end());
}

class ThreadedRunner : public base::SimpleThread {
 public:
  explicit ThreadedRunner(base::OnceClosure callback)
      : SimpleThread("ThreadedRunner"), callback_(std::move(callback)) {}

  ThreadedRunner(const ThreadedRunner&) = delete;
  ThreadedRunner& operator=(const ThreadedRunner&) = delete;

  ~ThreadedRunner() override { Join(); }

  void Run() override { std::move(callback_).Run(); }

 private:
  base::OnceClosure callback_;
};

TEST_F(WaitSetTest, Satisfied) {
  WaitSet wait_set;
  MessagePipe p;

  const char kTestMessage1[] = "hello wake up";

  // Watch only one handle and write to the other.

  wait_set.AddHandle(p.handle1.get(), MOJO_HANDLE_SIGNAL_READABLE);
  WriteMessage(p.handle0, kTestMessage1);

  size_t num_ready_handles = 2;
  Handle ready_handles[2];
  MojoResult ready_results[2] = {MOJO_RESULT_UNKNOWN, MOJO_RESULT_UNKNOWN};
  HandleSignalsState hss[2];
  wait_set.Wait(nullptr, &num_ready_handles, ready_handles, ready_results, hss);

  EXPECT_EQ(1u, num_ready_handles);
  EXPECT_EQ(p.handle1.get(), ready_handles[0]);
  EXPECT_EQ(MOJO_RESULT_OK, ready_results[0]);
  EXPECT_TRUE(hss[0].readable() && hss[0].writable() && !hss[0].peer_closed());

  wait_set.RemoveHandle(p.handle1.get());

  // Now watch only the other handle and write to the first one.

  wait_set.AddHandle(p.handle0.get(), MOJO_HANDLE_SIGNAL_READABLE);
  WriteMessage(p.handle1, kTestMessage1);

  num_ready_handles = 2;
  ready_results[0] = MOJO_RESULT_UNKNOWN;
  ready_results[1] = MOJO_RESULT_UNKNOWN;
  wait_set.Wait(nullptr, &num_ready_handles, ready_handles, ready_results, hss);

  EXPECT_EQ(1u, num_ready_handles);
  EXPECT_EQ(p.handle0.get(), ready_handles[0]);
  EXPECT_EQ(MOJO_RESULT_OK, ready_results[0]);
  EXPECT_TRUE(hss[0].readable() && hss[0].writable() && !hss[0].peer_closed());

  // Now wait on both of them.
  wait_set.AddHandle(p.handle1.get(), MOJO_HANDLE_SIGNAL_READABLE);

  num_ready_handles = 2;
  ready_results[0] = MOJO_RESULT_UNKNOWN;
  ready_results[1] = MOJO_RESULT_UNKNOWN;
  wait_set.Wait(nullptr, &num_ready_handles, ready_handles, ready_results, hss);
  EXPECT_EQ(2u, num_ready_handles);
  EXPECT_TRUE((ready_handles[0] == p.handle0.get() &&
               ready_handles[1] == p.handle1.get()) ||
              (ready_handles[0] == p.handle1.get() &&
               ready_handles[1] == p.handle0.get()));
  EXPECT_EQ(MOJO_RESULT_OK, ready_results[0]);
  EXPECT_EQ(MOJO_RESULT_OK, ready_results[1]);
  EXPECT_TRUE(hss[0].readable() && hss[0].writable() && !hss[0].peer_closed());
  EXPECT_TRUE(hss[1].readable() && hss[1].writable() && !hss[1].peer_closed());

  // Wait on both again, but with only enough output space for one result.
  num_ready_handles = 1;
  ready_results[0] = MOJO_RESULT_UNKNOWN;
  wait_set.Wait(nullptr, &num_ready_handles, ready_handles, ready_results, hss);
  EXPECT_EQ(1u, num_ready_handles);
  EXPECT_TRUE(ready_handles[0] == p.handle0.get() ||
              ready_handles[0] == p.handle1.get());
  EXPECT_EQ(MOJO_RESULT_OK, ready_results[0]);

  // Remove the ready handle from the set and wait one more time.
  EXPECT_EQ(MOJO_RESULT_OK, wait_set.RemoveHandle(ready_handles[0]));

  num_ready_handles = 1;
  ready_results[0] = MOJO_RESULT_UNKNOWN;
  wait_set.Wait(nullptr, &num_ready_handles, ready_handles, ready_results, hss);
  EXPECT_EQ(1u, num_ready_handles);
  EXPECT_TRUE(ready_handles[0] == p.handle0.get() ||
              ready_handles[0] == p.handle1.get());
  EXPECT_EQ(MOJO_RESULT_OK, ready_results[0]);

  EXPECT_EQ(MOJO_RESULT_OK, wait_set.RemoveHandle(ready_handles[0]));

  // The wait set should be empty now. Nothing to wait on.
  num_ready_handles = 2;
  wait_set.Wait(nullptr, &num_ready_handles, ready_handles, ready_results);
  EXPECT_EQ(0u, num_ready_handles);
}

TEST_F(WaitSetTest, Unsatisfiable) {
  MessagePipe p, q;
  WaitSet wait_set;

  wait_set.AddHandle(q.handle0.get(), MOJO_HANDLE_SIGNAL_READABLE);
  wait_set.AddHandle(q.handle1.get(), MOJO_HANDLE_SIGNAL_READABLE);
  wait_set.AddHandle(p.handle0.get(), MOJO_HANDLE_SIGNAL_READABLE);

  size_t num_ready_handles = 2;
  Handle ready_handles[2];
  MojoResult ready_results[2] = {MOJO_RESULT_UNKNOWN, MOJO_RESULT_UNKNOWN};

  p.handle1.reset();
  wait_set.Wait(nullptr, &num_ready_handles, ready_handles, ready_results);
  EXPECT_EQ(1u, num_ready_handles);
  EXPECT_EQ(p.handle0.get(), ready_handles[0]);
  EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION, ready_results[0]);
}

TEST_F(WaitSetTest, CloseWhileWaiting) {
  MessagePipe p;
  WaitSet wait_set;

  wait_set.AddHandle(p.handle0.get(), MOJO_HANDLE_SIGNAL_READABLE);

  const Handle handle0_value = p.handle0.get();
  ThreadedRunner close_after_delay(base::BindOnce(
      [](ScopedMessagePipeHandle* handle) {
        // Wait a little while, then close the handle.
        base::PlatformThread::Sleep(base::Milliseconds(200));
        handle->reset();
      },
      &p.handle0));
  close_after_delay.Start();

  size_t num_ready_handles = 2;
  Handle ready_handles[2];
  MojoResult ready_results[2] = {MOJO_RESULT_UNKNOWN, MOJO_RESULT_UNKNOWN};
  wait_set.Wait(nullptr, &num_ready_handles, ready_handles, ready_results);
  EXPECT_EQ(1u, num_ready_handles);
  EXPECT_EQ(handle0_value, ready_handles[0]);
  EXPECT_EQ(MOJO_RESULT_CANCELLED, ready_results[0]);

  EXPECT_EQ(MOJO_RESULT_NOT_FOUND, wait_set.RemoveHandle(handle0_value));
}

TEST_F(WaitSetTest, CloseBeforeWaiting) {
  MessagePipe p;
  WaitSet wait_set;

  wait_set.AddHandle(p.handle0.get(), MOJO_HANDLE_SIGNAL_READABLE);
  wait_set.AddHandle(p.handle1.get(), MOJO_HANDLE_SIGNAL_READABLE);

  Handle handle0_value = p.handle0.get();
  Handle handle1_value = p.handle1.get();

  p.handle0.reset();
  p.handle1.reset();

  // Ensure that the WaitSet user is always made aware of all cancellations even
  // if they happen while not waiting, or they have to be returned over the span
  // of multiple Wait() calls due to insufficient output storage.

  size_t num_ready_handles = 1;
  Handle ready_handle;
  MojoResult ready_result = MOJO_RESULT_UNKNOWN;
  wait_set.Wait(nullptr, &num_ready_handles, &ready_handle, &ready_result);
  EXPECT_EQ(1u, num_ready_handles);
  EXPECT_TRUE(ready_handle == handle0_value || ready_handle == handle1_value);
  EXPECT_EQ(MOJO_RESULT_CANCELLED, ready_result);
  EXPECT_EQ(MOJO_RESULT_NOT_FOUND, wait_set.RemoveHandle(handle0_value));

  wait_set.Wait(nullptr, &num_ready_handles, &ready_handle, &ready_result);
  EXPECT_EQ(1u, num_ready_handles);
  EXPECT_TRUE(ready_handle == handle0_value || ready_handle == handle1_value);
  EXPECT_EQ(MOJO_RESULT_CANCELLED, ready_result);
  EXPECT_EQ(MOJO_RESULT_NOT_FOUND, wait_set.RemoveHandle(handle0_value));

  // Nothing more to wait on.
  wait_set.Wait(nullptr, &num_ready_handles, &ready_handle, &ready_result);
  EXPECT_EQ(0u, num_ready_handles);
}

TEST_F(WaitSetTest, SatisfiedThenUnsatisfied) {
  MessagePipe p;
  WaitSet wait_set;

  wait_set.AddHandle(p.handle0.get(), MOJO_HANDLE_SIGNAL_READABLE);
  wait_set.AddHandle(p.handle1.get(), MOJO_HANDLE_SIGNAL_READABLE);

  const char kTestMessage1[] = "testing testing testing";
  WriteMessage(p.handle0, kTestMessage1);

  size_t num_ready_handles = 2;
  Handle ready_handles[2];
  MojoResult ready_results[2] = {MOJO_RESULT_UNKNOWN, MOJO_RESULT_UNKNOWN};
  wait_set.Wait(nullptr, &num_ready_handles, ready_handles, ready_results);
  EXPECT_EQ(1u, num_ready_handles);
  EXPECT_EQ(p.handle1.get(), ready_handles[0]);
  EXPECT_EQ(MOJO_RESULT_OK, ready_results[0]);

  EXPECT_EQ(kTestMessage1, ReadMessage(p.handle1));

  ThreadedRunner write_after_delay(base::BindOnce(
      [](ScopedMessagePipeHandle* handle) {
        // Wait a little while, then write a message.
        base::PlatformThread::Sleep(base::Milliseconds(200));
        WriteMessage(*handle, "wakey wakey");
      },
      &p.handle1));
  write_after_delay.Start();

  num_ready_handles = 2;
  wait_set.Wait(nullptr, &num_ready_handles, ready_handles, ready_results);
  EXPECT_EQ(1u, num_ready_handles);
  EXPECT_EQ(p.handle0.get(), ready_handles[0]);
  EXPECT_EQ(MOJO_RESULT_OK, ready_results[0]);
}

TEST_F(WaitSetTest, EventOnly) {
  base::WaitableEvent event(base::WaitableEvent::ResetPolicy::MANUAL,
                            base::WaitableEvent::InitialState::SIGNALED);
  WaitSet wait_set;
  wait_set.AddEvent(&event);

  base::WaitableEvent* ready_event = nullptr;
  size_t num_ready_handles = 1;
  Handle ready_handle;
  MojoResult ready_result = MOJO_RESULT_UNKNOWN;
  wait_set.Wait(&ready_event, &num_ready_handles, &ready_handle, &ready_result);
  EXPECT_EQ(0u, num_ready_handles);
  EXPECT_EQ(&event, ready_event);
}

TEST_F(WaitSetTest, EventAndHandle) {
  const char kTestMessage[] = "hello hello";

  MessagePipe p;
  WriteMessage(p.handle0, kTestMessage);

  base::WaitableEvent event(base::WaitableEvent::ResetPolicy::MANUAL,
                            base::WaitableEvent::InitialState::NOT_SIGNALED);

  WaitSet wait_set;
  wait_set.AddHandle(p.handle1.get(), MOJO_HANDLE_SIGNAL_READABLE);
  wait_set.AddEvent(&event);

  base::WaitableEvent* ready_event = nullptr;
  size_t num_ready_handles = 1;
  Handle ready_handle;
  MojoResult ready_result = MOJO_RESULT_UNKNOWN;
  wait_set.Wait(&ready_event, &num_ready_handles, &ready_handle, &ready_result);
  EXPECT_EQ(1u, num_ready_handles);
  EXPECT_EQ(nullptr, ready_event);
  EXPECT_EQ(p.handle1.get(), ready_handle);
  EXPECT_EQ(MOJO_RESULT_OK, ready_result);

  EXPECT_EQ(kTestMessage, ReadMessage(p.handle1));

  ThreadedRunner signal_after_delay(base::BindOnce(
      [](base::WaitableEvent* event) {
        // Wait a little while, then close the handle.
        base::PlatformThread::Sleep(base::Milliseconds(200));
        event->Signal();
      },
      &event));
  signal_after_delay.Start();

  wait_set.Wait(&ready_event, &num_ready_handles, &ready_handle, &ready_result);
  EXPECT_EQ(0u, num_ready_handles);
  EXPECT_EQ(&event, ready_event);
}

TEST_F(WaitSetTest, NoStarvation) {
  const char kTestMessage[] = "wait for it";
  const size_t kNumTestPipes = 50;
  const size_t kNumTestEvents = 10;

  // Create a bunch of handles and events which are always ready and add them
  // to a shared WaitSet.

  WaitSet wait_set;

  MessagePipe pipes[kNumTestPipes];
  for (size_t i = 0; i < kNumTestPipes; ++i) {
    WriteMessage(pipes[i].handle0, kTestMessage);
    Wait(pipes[i].handle1.get(), MOJO_HANDLE_SIGNAL_READABLE);

    WriteMessage(pipes[i].handle1, kTestMessage);
    Wait(pipes[i].handle0.get(), MOJO_HANDLE_SIGNAL_READABLE);

    wait_set.AddHandle(pipes[i].handle0.get(), MOJO_HANDLE_SIGNAL_READABLE);
    wait_set.AddHandle(pipes[i].handle1.get(), MOJO_HANDLE_SIGNAL_READABLE);
  }

  std::vector<std::unique_ptr<base::WaitableEvent>> events(kNumTestEvents);
  for (auto& event_ptr : events) {
    event_ptr = std::make_unique<base::WaitableEvent>(
        base::WaitableEvent::ResetPolicy::MANUAL,
        base::WaitableEvent::InitialState::NOT_SIGNALED);
    event_ptr->Signal();
    wait_set.AddEvent(event_ptr.get());
  }

  // Now verify that all handle and event signals are deteceted within a finite
  // number of consecutive Wait() calls. Do it a few times for good measure.
  for (size_t i = 0; i < 3; ++i) {
    std::set<base::WaitableEvent*> ready_events;
    std::set<Handle> ready_handles;
    while (ready_events.size() < kNumTestEvents ||
           ready_handles.size() < kNumTestPipes * 2) {
      base::WaitableEvent* ready_event = nullptr;
      size_t num_ready_handles = 1;
      Handle ready_handle;
      MojoResult ready_result = MOJO_RESULT_UNKNOWN;
      wait_set.Wait(&ready_event, &num_ready_handles, &ready_handle,
                    &ready_result);
      if (ready_event)
        ready_events.insert(ready_event);

      if (num_ready_handles) {
        EXPECT_EQ(1u, num_ready_handles);
        EXPECT_EQ(MOJO_RESULT_OK, ready_result);
        ready_handles.insert(ready_handle);
      }
    }
  }
}

}  // namespace
}  // namespace mojo
