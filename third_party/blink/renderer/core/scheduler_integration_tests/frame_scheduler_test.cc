// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code if governed by a BSD-style license that can be
// found in LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/public/frame_scheduler.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"

using testing::ElementsAre;

namespace blink {

class FrameSchedulerFrameTypeTest : public SimTest {};

TEST_F(FrameSchedulerFrameTypeTest, GetFrameType) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(R"HTML(
    <!DOCTYPE HTML>
    <body>
    <iframe src="about:blank"></iframe>
    </body>
  )HTML");

  EXPECT_EQ(FrameScheduler::FrameType::kMainFrame,
            MainFrame().GetFrame()->GetFrameScheduler()->GetFrameType());

  Frame* child = MainFrame().GetFrame()->Tree().FirstChild();
  EXPECT_EQ(FrameScheduler::FrameType::kSubframe,
            To<LocalFrame>(child)->GetFrameScheduler()->GetFrameType());
}

}  // namespace blink
