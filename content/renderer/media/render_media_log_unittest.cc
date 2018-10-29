// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <tuple>

#include "base/macros.h"
#include "base/test/scoped_task_environment.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/test_mock_time_task_runner.h"
#include "content/common/view_messages.h"
#include "content/public/test/mock_render_thread.h"
#include "content/renderer/media/render_media_log.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"

namespace content {

class RenderMediaLogTest : public testing::Test {
 public:
  RenderMediaLogTest()
      : log_(GURL("http://foo.com"),
             blink::scheduler::GetSingleThreadTaskRunnerForTesting()),
        task_runner_(new base::TestMockTimeTaskRunner()) {
    log_.SetTickClockForTesting(&tick_clock_);
    log_.SetTaskRunnerForTesting(task_runner_);
  }

  ~RenderMediaLogTest() override {
    task_runner_->ClearPendingTasks();
  }

  void AddEvent(media::MediaLogEvent::Type type) {
    log_.AddEvent(log_.CreateEvent(type));
    // AddEvent() could post. Run the task runner to make sure it's executed.
    task_runner_->RunUntilIdle();
  }

  void Advance(base::TimeDelta delta) {
    tick_clock_.Advance(delta);
    task_runner_->FastForwardBy(delta);
  }

  int message_count() { return render_thread_.sink().message_count(); }

  std::vector<media::MediaLogEvent> GetMediaLogEvents() {
    const IPC::Message* msg = render_thread_.sink().GetFirstMessageMatching(
        ViewHostMsg_MediaLogEvents::ID);
    if (!msg) {
      ADD_FAILURE() << "Did not find ViewHostMsg_MediaLogEvents IPC message";
      return std::vector<media::MediaLogEvent>();
    }

    std::tuple<std::vector<media::MediaLogEvent>> events;
    ViewHostMsg_MediaLogEvents::Read(msg, &events);
    return std::get<0>(events);
  }

 private:
  base::test::ScopedTaskEnvironment task_environment_;
  MockRenderThread render_thread_;
  base::SimpleTestTickClock tick_clock_;
  RenderMediaLog log_;
  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;

  DISALLOW_COPY_AND_ASSIGN(RenderMediaLogTest);
};

TEST_F(RenderMediaLogTest, ThrottleSendingEvents) {
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

TEST_F(RenderMediaLogTest, EventSentWithoutDelayAfterIpcInterval) {
  AddEvent(media::MediaLogEvent::LOAD);
  Advance(base::TimeDelta::FromMilliseconds(1000));
  EXPECT_EQ(1, message_count());

  // After the ipc send interval passes, the next event should be sent
  // right away.
  Advance(base::TimeDelta::FromMilliseconds(2000));
  AddEvent(media::MediaLogEvent::LOAD);
  EXPECT_EQ(2, message_count());
}

TEST_F(RenderMediaLogTest, DurationChanged) {
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
