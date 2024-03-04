// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fragment_directive/text_fragment_test_util.h"

#include "third_party/blink/public/web/web_view.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/fragment_directive/text_fragment_anchor.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/web_frame_widget_impl.h"
#include "third_party/blink/renderer/platform/scheduler/public/main_thread_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "ui/gfx/geometry/size.h"

namespace blink {

TextFragmentAnchorTestBase::TextFragmentAnchorTestBase()
    : enable_virtual_time_(true) {}

TextFragmentAnchorTestBase::TextFragmentAnchorTestBase(
    base::test::TaskEnvironment::TimeSource time_source)
    : SimTest(time_source) {}

void TextFragmentAnchorTestBase::SetUp() {
  SimTest::SetUp();
  if (enable_virtual_time_) {
    // Most tests aren't concerned with the post-load task timers so use virtual
    // time so tests don't spend time waiting for the real-clock timers to fire.
    WebView().Scheduler()->GetVirtualTimeController()->EnableVirtualTime(
        base::Time());
  }
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
}

void TextFragmentAnchorTestBase::TearDown() {
  DisableVirtualTimeIfSet();
  SimTest::TearDown();
}

void TextFragmentAnchorTestBase::RunAsyncMatchingTasks() {
  ThreadScheduler::Current()
      ->ToMainThreadScheduler()
      ->StartIdlePeriodForTesting();
  if (enable_virtual_time_) {
    test::RunPendingTasks();
  } else {
    task_environment().FastForwardUntilNoTasksRemain();
  }
}

void TextFragmentAnchorTestBase::RunUntilTextFragmentFinalization() {
  FragmentAnchor* base_anchor =
      GetDocument().GetFrame()->View()->GetFragmentAnchor();
  CHECK(base_anchor);
  CHECK(base_anchor->IsTextFragmentAnchor());

  TextFragmentAnchor* anchor = static_cast<TextFragmentAnchor*>(base_anchor);

  CHECK_NE(anchor->iteration_, TextFragmentAnchor::kLoad);

  if (anchor->iteration_ == TextFragmentAnchor::kParsing) {
    // Dispatch load event if needed, ensure a frame is produced to perform
    // the search if needed.
    test::RunPendingTasks();
    Compositor().BeginFrame();

    // If all directives were found, the anchor may already have been removed.
    if (!GetDocument().GetFrame()->View()->GetFragmentAnchor()) {
      return;
    }
  }
  if (anchor->iteration_ == TextFragmentAnchor::kPostLoad) {
    // Run the TextFragmentAnchor::PostLoadTask which is on a timer delay.
    if (enable_virtual_time_) {
      test::RunDelayedTasks(TextFragmentAnchor::PostLoadTaskTimeout());
    } else {
      task_environment().FastForwardBy(
          TextFragmentAnchor::PostLoadTaskTimeout());
    }
    // PostLoadTask schedules a new frame to perform the final text search.
    // Perform that here.
    Compositor().BeginFrame();
  }

  CHECK(anchor->iteration_ == TextFragmentAnchor::kDone);

  // Finalization occurs in the frame after the final search, where script
  // can run.
  Compositor().BeginFrame();
}

void TextFragmentAnchorTestBase::DisableVirtualTimeIfSet() {
  if (!enable_virtual_time_) {
    return;
  }
  auto* virtual_time_controller =
      WebView().Scheduler()->GetVirtualTimeController();
  if (virtual_time_controller) {
    virtual_time_controller->DisableVirtualTimeForTesting();
  }
}

}  // namespace blink
