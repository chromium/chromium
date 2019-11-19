// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/batching_media_log.h"

#include "base/macros.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/task_environment.h"
#include "base/test/test_mock_time_task_runner.h"
#include "content/common/view_messages.h"
#include "content/public/test/mock_render_thread.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"

namespace content {

class BatchingMediaLogTest;

class TestEventHandler : public BatchingMediaLog::EventHandler {
 public:
  explicit TestEventHandler(BatchingMediaLogTest* test_cls)
      : test_cls_(test_cls) {}
  void SendQueuedMediaEvents(std::vector<media::MediaLogEvent> events) override;
  void OnWebMediaPlayerDestroyed() override;

 private:
  BatchingMediaLogTest* test_cls_;
};

class BatchingMediaLogTest : public testing::Test {
 public:
  BatchingMediaLogTest()
      : task_runner_(new base::TestMockTimeTaskRunner()),
        log_(GURL("http://foo.com"),
             task_runner_,
             std::make_unique<TestEventHandler>(this)) {
    log_.SetTickClockForTesting(&tick_clock_);
  }

  ~BatchingMediaLogTest() override { task_runner_->ClearPendingTasks(); }

  void AddEvent(media::MediaLogEvent::Type type) {
    log_.AddEvent(log_.CreateEvent(type));
    // AddEvent() could post. Run the task runner to make sure it's executed.
    task_runner_->RunUntilIdle();
  }

  void Advance(base::TimeDelta delta) {
    tick_clock_.Advance(delta);
    task_runner_->FastForwardBy(delta);
  }

  int message_count() { return add_events_count_; }

  std::vector<media::MediaLogEvent> GetMediaLogEvents() {
    std::vector<media::MediaLogEvent> return_events = std::move(events_);
    return return_events;
  }

 private:
  friend class TestEventHandler;
  void AddEventsForTesting(std::vector<media::MediaLogEvent> events) {
    events_.insert(events_.end(), events.begin(), events.end());
    add_events_count_++;
  }
  int add_events_count_ = 0;
  std::vector<media::MediaLogEvent> events_;
  base::test::TaskEnvironment task_environment_;
  MockRenderThread render_thread_;
  base::SimpleTestTickClock tick_clock_;
  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;
  BatchingMediaLog log_;

  DISALLOW_COPY_AND_ASSIGN(BatchingMediaLogTest);
};

void TestEventHandler::SendQueuedMediaEvents(
    std::vector<media::MediaLogEvent> events) {
  test_cls_->AddEventsForTesting(events);
}

void TestEventHandler::OnWebMediaPlayerDestroyed() {}

TEST_F(BatchingMediaLogTest, ThrottleSendingEvents) {
  AddEvent(media::MediaLogEvent::LOAD);
  EXPECT_EQ(0, message_count());

  // Still shouldn't send anything.
  Advance(base::TimeDelta::FromMilliseconds(500));
  AddEvent(media::MediaLogEvent::SEEK);
  EXPECT_EQ(0, message_count());

  // Now we should expect an IPC.
  Advance(base::TimeDelta::FromMilliseconds(500));
  EXPECT_EQ(1, message_count());

  // Verify contents.
  std::vector<media::MediaLogEvent> events = GetMediaLogEvents();
  ASSERT_EQ(2u, events.size());
  EXPECT_EQ(media::MediaLogEvent::LOAD, events[0].type);
  EXPECT_EQ(media::MediaLogEvent::SEEK, events[1].type);

  // Adding another event shouldn't send anything.
  AddEvent(media::MediaLogEvent::PIPELINE_ERROR);
  EXPECT_EQ(1, message_count());
}

TEST_F(BatchingMediaLogTest, EventSentWithoutDelayAfterIpcInterval) {
  AddEvent(media::MediaLogEvent::LOAD);
  Advance(base::TimeDelta::FromMilliseconds(1000));
  EXPECT_EQ(1, message_count());

  // After the ipc send interval passes, the next event should be sent
  // right away.
  Advance(base::TimeDelta::FromMilliseconds(2000));
  AddEvent(media::MediaLogEvent::LOAD);
  EXPECT_EQ(2, message_count());
}

TEST_F(BatchingMediaLogTest, DurationChanged) {
  AddEvent(media::MediaLogEvent::LOAD);
  AddEvent(media::MediaLogEvent::SEEK);

  // This event is handled separately and should always appear last regardless
  // of how many times we see it.
  AddEvent(media::MediaLogEvent::DURATION_SET);
  AddEvent(media::MediaLogEvent::DURATION_SET);
  AddEvent(media::MediaLogEvent::DURATION_SET);

  EXPECT_EQ(0, message_count());
  Advance(base::TimeDelta::FromMilliseconds(1000));
  EXPECT_EQ(1, message_count());

  // Verify contents. There should only be a single buffered extents changed
  // event.
  std::vector<media::MediaLogEvent> events = GetMediaLogEvents();
  ASSERT_EQ(3u, events.size());
  EXPECT_EQ(media::MediaLogEvent::LOAD, events[0].type);
  EXPECT_EQ(media::MediaLogEvent::SEEK, events[1].type);
  EXPECT_EQ(media::MediaLogEvent::DURATION_SET, events[2].type);
}

}  // namespace content
