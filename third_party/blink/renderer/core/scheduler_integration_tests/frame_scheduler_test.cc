// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/public/frame_scheduler.h"

#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

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

class FencedFrameFrameSchedulerTest
    : private ScopedFencedFramesForTest,
      public testing::WithParamInterface<const char*>,
      public SimTest {
 public:
  FencedFrameFrameSchedulerTest() : ScopedFencedFramesForTest(true) {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kFencedFrames, {{"implementation_type", "mparch"}});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(FencedFrameFrameSchedulerTest, GetFrameType) {
  InitializeFencedFrameRoot(
      blink::FencedFrame::DeprecatedFencedFrameMode::kDefault);
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(R"HTML(
    <!DOCTYPE HTML>
    <body>
    </body>
  )HTML");

  // A fenced frame root will should be treated as a main frame but
  // marked in an embedded frame tree.
  EXPECT_EQ(FrameScheduler::FrameType::kMainFrame,
            MainFrame().GetFrame()->GetFrameScheduler()->GetFrameType());
  EXPECT_TRUE(
      MainFrame().GetFrame()->GetFrameScheduler()->IsInEmbeddedFrameTree());
}

}  // namespace blink
