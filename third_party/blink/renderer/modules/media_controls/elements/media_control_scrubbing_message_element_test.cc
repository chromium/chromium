// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/media_controls/elements/media_control_scrubbing_message_element.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/modules/media_controls/media_controls_impl.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

namespace {

// The number of child elements the shadow DOM should have.
const unsigned kExpectedElementCount = 6;

}  // namespace

class MediaControlScrubbingMessageElementTest : public PageTestBase {
 public:
  void SetUp() final {
    // Create page and add a video element with controls.
    PageTestBase::SetUp();
    media_element_ = MakeGarbageCollected<HTMLVideoElement>(GetDocument());
    media_element_->SetBooleanAttribute(html_names::kControlsAttr, true);
    GetDocument().body()->AppendChild(media_element_);

    // Create instance of MediaControlScrubbingMessageElement to run tests on.
    media_controls_ =
        static_cast<MediaControlsImpl*>(media_element_->GetMediaControls());
    ASSERT_NE(nullptr, media_controls_);
    message_element_ =
        MakeGarbageCollected<MediaControlScrubbingMessageElement>(
            *media_controls_);
  }

 protected:
  void SetIsWanted(bool wanted) { message_element_->SetIsWanted(wanted); }

  unsigned CountChildren() const {
    return message_element_->GetShadowRoot()->CountChildren();
  }

 private:
  Persistent<HTMLMediaElement> media_element_;
  Persistent<MediaControlsImpl> media_controls_;
  Persistent<MediaControlScrubbingMessageElement> message_element_;
};

TEST_F(MediaControlScrubbingMessageElementTest, PopulateShadowDOM) {
  EXPECT_EQ(0u, CountChildren());

  // Show the element and the shadow DOM should now have children.
  SetIsWanted(true);
  EXPECT_EQ(kExpectedElementCount, CountChildren());

  // Show the element again and we should have no more children.
  SetIsWanted(true);
  EXPECT_EQ(kExpectedElementCount, CountChildren());

  // Hide the element and expect the children to remain.
  SetIsWanted(false);
  EXPECT_EQ(kExpectedElementCount, CountChildren());

  // Hide the element again and still expect the children to remain.
  SetIsWanted(false);
  EXPECT_EQ(kExpectedElementCount, CountChildren());
}

}  // namespace blink
