// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/media_controls/elements/media_control_loading_panel_element.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/css_style_declaration.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/input_type_names.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/modules/media_controls/media_controls_impl.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

class MediaControlLoadingPanelElementTest : public PageTestBase {
 public:
  void SetUp() final {
    // Create page and add a video element with controls.
    PageTestBase::SetUp();
    media_element_ = MakeGarbageCollected<HTMLVideoElement>(GetDocument());
    media_element_->SetBooleanAttribute(html_names::kControlsAttr, true);
    GetDocument().body()->AppendChild(media_element_);

    // Create instance of MediaControlInputElement to run tests on.
    media_controls_ =
        static_cast<MediaControlsImpl*>(media_element_->GetMediaControls());
    ASSERT_NE(media_controls_, nullptr);
    loading_element_ =
        MakeGarbageCollected<MediaControlLoadingPanelElement>(*media_controls_);
  }

 protected:
  void ExpectStateIsHidden() {
    EXPECT_EQ(MediaControlLoadingPanelElement::kHidden,
              loading_element_->state_);
    CheckIsHidden();
  }

  void ExpectStateIsPlaying() {
    EXPECT_EQ(MediaControlLoadingPanelElement::kPlaying,
              loading_element_->state_);
    CheckIsShown();
  }

  void ExpectStateIsCoolingDown() {
    EXPECT_EQ(MediaControlLoadingPanelElement::kCoolingDown,
              loading_element_->state_);
    CheckIsShown();
  }

  void SimulateLoadingMetadata() {
    SetMediaElementState(HTMLMediaElement::kHaveNothing,
                         HTMLMediaElement::kNetworkLoading);
    EXPECT_EQ(media_controls_->State(),
              MediaControlsImpl::kLoadingMetadataPaused);
    loading_element_->UpdateDisplayState();
  }

  void SimulateBuffering() {
    SetMediaElementState(HTMLMediaElement::kHaveCurrentData,
                         HTMLMediaElement::kNetworkLoading, false);
    EXPECT_EQ(media_controls_->State(), MediaControlsImpl::kBuffering);
    loading_element_->UpdateDisplayState();
  }

  void SimulateStopped() {
    SetMediaElementState(HTMLMediaElement::kHaveCurrentData,
                         HTMLMediaElement::kNetworkIdle);
    EXPECT_EQ(media_controls_->State(), MediaControlsImpl::kStopped);
    loading_element_->UpdateDisplayState();
  }

  void SimulatePlaying() {
    SetMediaElementState(HTMLMediaElement::kHaveCurrentData,
                         HTMLMediaElement::kNetworkIdle, false);
    EXPECT_EQ(media_controls_->State(), MediaControlsImpl::kPlaying);
    loading_element_->UpdateDisplayState();
  }

  void SimulateNoSource() {
    SetMediaElementState(HTMLMediaElement::kHaveNothing,
                         HTMLMediaElement::kNetworkNoSource);
    EXPECT_EQ(media_controls_->State(), MediaControlsImpl::kNoSource);
    loading_element_->UpdateDisplayState();
  }

  void SimulateAnimationIterations(int count) {
    for (int i = 0; i < count; i++) {
      TriggerEvent(event_type_names::kAnimationiteration);
    }
  }

  void ExpectAnimationIterationCount(const String& value) {
    ExpectAnimationIterationCount(loading_element_->mask1_background_, value);
    ExpectAnimationIterationCount(loading_element_->mask2_background_, value);
  }

  void ExpectAnimationIterationInfinite() {
    ExpectAnimationIterationCount("infinite");
  }

  void SimulateAnimationEnd() { TriggerEvent(event_type_names::kAnimationend); }

  void SimulateControlsHidden() { loading_element_->OnControlsHidden(); }

  void SimulateControlsShown() { loading_element_->OnControlsShown(); }

  void RunPlayingTestCycle() {
    ExpectStateIsHidden();

    // Show the panel when we are loading metadata.
    SimulateLoadingMetadata();
    ExpectStateIsPlaying();

    // Simulate some animations.
    SimulateAnimationIterations(3);
    ExpectAnimationIterationInfinite();

    // Transition the media controls to a playing state and expect the loading
    // panel to hide immediately.
    SimulatePlaying();

    // Make sure the loading panel is hidden now.
    ExpectStateIsHidden();

    // Show the panel when we are buffering.
    SimulateBuffering();
    ExpectStateIsPlaying();

    // Simulate some animations.
    SimulateAnimationIterations(3);
    ExpectAnimationIterationInfinite();

    // Transition the media controls to a playing state and expect the loading
    // panel to hide immediately.
    SimulatePlaying();

    // Make sure the loading panel is hidden now.
    ExpectStateIsHidden();
  }

 private:
  void SetMediaElementState(HTMLMediaElement::ReadyState ready_state,
                            HTMLMediaElement::NetworkState network_state,
                            bool paused = true) {
    media_element_->ready_state_ = ready_state;
    media_element_->network_state_ = network_state;
    media_element_->paused_ = paused;
  }

  void CheckIsHidden() {
    EXPECT_FALSE(loading_element_->IsWanted());
    EXPECT_FALSE(loading_element_->GetShadowRoot()->HasChildren());
  }

  void CheckIsShown() {
    EXPECT_TRUE(loading_element_->IsWanted());
    EXPECT_TRUE(loading_element_->GetShadowRoot()->HasChildren());
  }

  void ExpectAnimationIterationCount(Element* element, const String& value) {
    EXPECT_EQ(value,
              element->style()->getPropertyValue("animation-iteration-count"));
  }

  void TriggerEvent(const AtomicString& name) {
    Event* event = Event::Create(name);
    loading_element_->mask1_background_->DispatchEvent(*event);
  }

  Persistent<HTMLMediaElement> media_element_;
  Persistent<MediaControlsImpl> media_controls_;
  Persistent<MediaControlLoadingPanelElement> loading_element_;
};

TEST_F(MediaControlLoadingPanelElementTest, StateTransitions_ToPlaying) {
  RunPlayingTestCycle();
}

TEST_F(MediaControlLoadingPanelElementTest, StateTransitions_ToStopped) {
  ExpectStateIsHidden();

  // Show the panel when we are loading metadata.
  SimulateLoadingMetadata();
  ExpectStateIsPlaying();

  // Simulate some animations.
  SimulateAnimationIterations(5);
  ExpectAnimationIterationInfinite();

  // Transition the media controls to a stopped state and expect the loading
  // panel to start cooling down.
  SimulateStopped();
  ExpectStateIsCoolingDown();
  ExpectAnimationIterationCount("6");

  // Simulate the animations ending.
  SimulateAnimationEnd();

  // Make sure the loading panel is hidden now.
  ExpectStateIsHidden();
}

TEST_F(MediaControlLoadingPanelElementTest, Reset_AfterComplete) {
  RunPlayingTestCycle();

  // Reset to kNoSource.
  SimulateNoSource();
  RunPlayingTestCycle();
}

TEST_F(MediaControlLoadingPanelElementTest, Reset_DuringCycle) {
  ExpectStateIsHidden();

  // Show the panel when we are loading metadata.
  SimulateLoadingMetadata();
  ExpectStateIsPlaying();

  // Reset to kNoSource.
  SimulateNoSource();
  ExpectStateIsCoolingDown();

  // Start loading metadata again before we have hidden.
  SimulateLoadingMetadata();
  SimulateAnimationEnd();

  // We should now be showing the controls again.
  ExpectStateIsPlaying();
  ExpectAnimationIterationInfinite();

  // Now move to playing.
  SimulatePlaying();
  ExpectStateIsHidden();
}

TEST_F(MediaControlLoadingPanelElementTest, SkipLoadingMetadata) {
  ExpectStateIsHidden();
  SimulatePlaying();
  ExpectStateIsHidden();
}

TEST_F(MediaControlLoadingPanelElementTest, AnimationHiddenWhenControlsHidden) {
  // Animation doesn't start when Media Controls are already hidden.
  SimulateControlsHidden();
  SimulateLoadingMetadata();
  ExpectStateIsHidden();

  // Animation appears once Media Controls are shown.
  SimulateControlsShown();
  ExpectStateIsPlaying();

  // Animation is hidden when Media Controls are hidden again.
  SimulateControlsHidden();
  ExpectStateIsHidden();
}

}  // namespace blink
