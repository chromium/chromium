// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/system/handle_signal_tracker.h"

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "mojo/public/cpp/system/wait.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace {

class HandleSignalTrackerTest : public testing::Test {
 public:
  HandleSignalTrackerTest() {}

  HandleSignalTrackerTest(const HandleSignalTrackerTest&) = delete;
  HandleSignalTrackerTest& operator=(const HandleSignalTrackerTest&) = delete;

  ~HandleSignalTrackerTest() override {}

  void WaitForNextNotification(HandleSignalTracker* tracker) {
    base::RunLoop loop;
    tracker->set_notification_callback(base::BindRepeating(
        [](base::RunLoop* loop, const HandleSignalsState& signals_state) {
          loop->Quit();
        },
        &loop));
    loop.Run();
    tracker->set_notification_callback(
        HandleSignalTracker::NotificationCallback());
  }

 private:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(HandleSignalTrackerTest, StartsWithCorrectState) {
  if (mojo::core::IsMojoIpczEnabled()) {
    GTEST_SKIP() << "HandleSignalTracker is not supported by MojoIpcz.";
  }

  MessagePipe pipe;
  {
    HandleSignalTracker tracker(pipe.handle0.get(),
                                MOJO_HANDLE_SIGNAL_READABLE);
    EXPECT_FALSE(tracker.last_known_state().readable());
  }

  WriteMessageRaw(pipe.handle1.get(), "hi", 2, nullptr, 0,
                  MOJO_WRITE_MESSAGE_FLAG_NONE);
  Wait(pipe.handle0.get(), MOJO_HANDLE_SIGNAL_READABLE,
       MOJO_WATCH_CONDITION_SATISFIED);

  {
    HandleSignalTracker tracker(pipe.handle0.get(),
                                MOJO_HANDLE_SIGNAL_READABLE);
    EXPECT_TRUE(tracker.last_known_state().readable());
  }
}

TEST_F(HandleSignalTrackerTest, BasicTracking) {
  if (mojo::core::IsMojoIpczEnabled()) {
    GTEST_SKIP() << "HandleSignalTracker is not supported by MojoIpcz.";
  }

  MessagePipe pipe;
  HandleSignalTracker tracker(pipe.handle0.get(), MOJO_HANDLE_SIGNAL_READABLE);
  EXPECT_FALSE(tracker.last_known_state().readable());

  WriteMessageRaw(pipe.handle1.get(), "hi", 2, nullptr, 0,
                  MOJO_WRITE_MESSAGE_FLAG_NONE);
  WaitForNextNotification(&tracker);
  EXPECT_TRUE(tracker.last_known_state().readable());

  std::vector<uint8_t> bytes;
  ReadMessageRaw(pipe.handle0.get(), &bytes, nullptr,
                 MOJO_READ_MESSAGE_FLAG_NONE);
  WaitForNextNotification(&tracker);
  EXPECT_FALSE(tracker.last_known_state().readable());
}

TEST_F(HandleSignalTrackerTest, DoesntUpdateOnIrrelevantChanges) {
  if (mojo::core::IsMojoIpczEnabled()) {
    GTEST_SKIP() << "HandleSignalTracker is not supported by MojoIpcz.";
  }

  MessagePipe pipe;
  HandleSignalTracker readable_tracker(pipe.handle0.get(),
                                       MOJO_HANDLE_SIGNAL_READABLE);
  HandleSignalTracker peer_closed_tracker(pipe.handle0.get(),
                                          MOJO_HANDLE_SIGNAL_PEER_CLOSED);
  EXPECT_FALSE(readable_tracker.last_known_state().readable());
  EXPECT_FALSE(peer_closed_tracker.last_known_state().peer_closed());

  WriteMessageRaw(pipe.handle1.get(), "hi", 2, nullptr, 0,
                  MOJO_WRITE_MESSAGE_FLAG_NONE);
  WaitForNextNotification(&readable_tracker);
  EXPECT_TRUE(readable_tracker.last_known_state().readable());
  EXPECT_FALSE(readable_tracker.last_known_state().peer_closed());

  // Closing the peer won't change the |readable_tracker|'s state since there's
  // still an unread message. Therefore the tracker's last known state should
  // continue to reflect the state prior to peer closure even after the handle's
  // signals state has updated.
  pipe.handle1.reset();
  WaitForNextNotification(&peer_closed_tracker);
  EXPECT_TRUE(pipe.handle0->QuerySignalsState().peer_closed());
  EXPECT_TRUE(peer_closed_tracker.last_known_state().peer_closed());
  EXPECT_FALSE(readable_tracker.last_known_state().peer_closed());

  // Now read the message, which will ultimately trigger the pipe becoming
  // unreadable.
  std::vector<uint8_t> bytes;
  ReadMessageRaw(pipe.handle0.get(), &bytes, nullptr,
                 MOJO_READ_MESSAGE_FLAG_NONE);
  WaitForNextNotification(&readable_tracker);
  EXPECT_FALSE(readable_tracker.last_known_state().readable());

  // And note that the |peer_closed_tracker| should not have seen the readable
  // state change above since it's not relevant to its tracked signal.
  EXPECT_TRUE(peer_closed_tracker.last_known_state().readable());
}

}  // namespace
}  // namespace mojo
