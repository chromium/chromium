// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/media_controls/elements/media_control_panel_element.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/input_type_names.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/modules/media_controls/media_controls_impl.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

class MediaControlPanelElementTest : public PageTestBase {
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
    panel_element_ =
        MakeGarbageCollected<MediaControlPanelElement>(*media_controls_);
  }

 protected:
  void SimulateTransitionEnd(Element& element) {
    TriggerEvent(element, event_type_names::kTransitionend);
  }

  void ExpectPanelIsDisplayed() { EXPECT_TRUE(GetPanel().IsWanted()); }

  void ExpectPanelIsNotDisplayed() { EXPECT_FALSE(GetPanel().IsWanted()); }

  void EventListenerNotCreated() { EXPECT_FALSE(GetPanel().event_listener_); }

  void EventListenerAttached() {
    EXPECT_TRUE(GetPanel().EventListenerIsAttachedForTest());
  }

  void EventListenerDetached() {
    EXPECT_FALSE(GetPanel().EventListenerIsAttachedForTest());
  }

  MediaControlPanelElement& GetPanel() { return *panel_element_.Get(); }
  HTMLMediaElement& GetMediaElement() { return *media_element_.Get(); }

 private:
  void TriggerEvent(Element& element, const AtomicString& name) {
    Event* event = Event::Create(name);
    event->SetTarget(&element);
    GetPanel().FireEventListeners(*event);
  }

  Persistent<HTMLMediaElement> media_element_;
  Persistent<MediaControlsImpl> media_controls_;
  Persistent<MediaControlPanelElement> panel_element_;
};

TEST_F(MediaControlPanelElementTest, StateTransitions) {
  auto* child_div =
      MakeGarbageCollected<HTMLDivElement>(GetPanel().GetDocument());
  GetPanel().ParserAppendChild(child_div);

  // Make sure we are displayed (we are already opaque).
  GetPanel().SetIsDisplayed(true);
  ExpectPanelIsDisplayed();

  // Ensure the event listener has not been created and make the panel
  // transparent.
  EventListenerNotCreated();
  GetPanel().MakeTransparent();

  // The event listener should now be attached
  EventListenerAttached();

  // Simulate child div transition end and the panel should not be hidden
  SimulateTransitionEnd(*child_div);
  ExpectPanelIsDisplayed();

  // Simulate panel transition end and the panel will be hidden
  SimulateTransitionEnd(GetPanel());
  ExpectPanelIsNotDisplayed();

  // The event listener should be detached. We should now make the panel
  // opaque again.
  EventListenerDetached();
  GetPanel().MakeOpaque();

  // The event listener should now be attached so we should simulate the
  // transition end event and the panel will be hidden.
  EventListenerAttached();
  SimulateTransitionEnd(GetPanel());
  ExpectPanelIsDisplayed();
}

TEST_F(MediaControlPanelElementTest, isConnected) {
  EXPECT_TRUE(
      GetMediaElement().GetMediaControls()->PanelElement()->isConnected());
  GetMediaElement().remove();
  EXPECT_FALSE(
      GetMediaElement().GetMediaControls()->PanelElement()->isConnected());
}

}  // namespace blink
