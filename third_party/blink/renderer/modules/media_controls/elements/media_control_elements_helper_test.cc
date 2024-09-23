// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/media_controls/elements/media_control_elements_helper.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"

namespace blink {

class MediaControlElementsHelperTest : public PageTestBase {
 public:
  void SetUp() final {
    // Create page and add a video element with controls.
    PageTestBase::SetUp();
    media_element_ = MakeGarbageCollected<HTMLVideoElement>(GetDocument());
    media_element_->SetBooleanAttribute(html_names::kControlsAttr, true);
    GetDocument().body()->AppendChild(media_element_);
  }

  void TearDown() final { media_element_ = nullptr; }

  HTMLMediaElement& GetElement() const { return *media_element_; }

 private:
  Persistent<HTMLMediaElement> media_element_;
};

TEST_F(MediaControlElementsHelperTest, DipSizeUnaffectedByPageZoom) {
  ASSERT_FALSE(GetElement().GetLayoutObject());

  gfx::Size test_size(123, 456);
  EXPECT_EQ(test_size, MediaControlElementsHelper::GetSizeOrDefault(
                           GetElement(), test_size));
  GetDocument().GetFrame()->SetLayoutZoomFactor(2.f);
  EXPECT_EQ(test_size, MediaControlElementsHelper::GetSizeOrDefault(
                           GetElement(), test_size));
}

TEST_F(MediaControlElementsHelperTest, LayoutSizeAffectedByPageZoom) {
  ASSERT_FALSE(GetElement().GetLayoutObject());
  UpdateAllLifecyclePhasesForTest();
  ASSERT_TRUE(GetElement().GetLayoutObject());

  gfx::Size test_size(123, 456);
  gfx::Size real_size =
      MediaControlElementsHelper::GetSizeOrDefault(GetElement(), test_size);
  EXPECT_NE(real_size, test_size);
  GetDocument().GetFrame()->SetLayoutZoomFactor(2.f);
  gfx::Size zoom_size =
      MediaControlElementsHelper::GetSizeOrDefault(GetElement(), test_size);
  EXPECT_LT(zoom_size.width(), real_size.width());
  EXPECT_LT(zoom_size.height(), real_size.height());
}

}  // namespace blink
