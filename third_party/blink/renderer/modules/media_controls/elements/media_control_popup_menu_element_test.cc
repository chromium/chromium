// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/media_controls/elements/media_control_popup_menu_element.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/web_keyboard_event.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/html/forms/html_label_element.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/input_type_names.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/modules/media_controls/elements/media_control_download_button_element.h"
#include "third_party/blink/renderer/modules/media_controls/elements/media_control_overflow_menu_button_element.h"
#include "third_party/blink/renderer/modules/media_controls/elements/media_control_overflow_menu_list_element.h"
#include "third_party/blink/renderer/modules/media_controls/elements/media_control_playback_speed_button_element.h"
#include "third_party/blink/renderer/modules/media_controls/elements/media_control_playback_speed_list_element.h"
#include "third_party/blink/renderer/modules/media_controls/media_controls_impl.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/keyboard_codes.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

class MediaControlPopupMenuElementTest : public PageTestBase {
 public:
  void SetUp() final {
    // Create page and add a video element with controls.
    PageTestBase::SetUp();
    media_element_ = MakeGarbageCollected<HTMLVideoElement>(GetDocument());
    media_element_->SetBooleanAttribute(html_names::kControlsAttr, true);
    media_element_->setAttribute(html_names::kPreloadAttr,
                                 AtomicString("none"));
    media_element_->SetSrc(AtomicString("http://example.com/foo.mp4"));
    GetDocument().body()->AppendChild(media_element_);
    test::RunPendingTasks();
    UpdateAllLifecyclePhasesForTest();

    media_controls_ =
        static_cast<MediaControlsImpl*>(media_element_->GetMediaControls());
    ASSERT_NE(media_controls_, nullptr);
    media_controls_->OnLoadedMetadata();
  }

 protected:
  KeyboardEvent* KeyDownEvent(
      int key_code,
      Element* target = nullptr,
      WebInputEvent::Modifiers modifiers = WebInputEvent::kNoModifiers) {
    WebKeyboardEvent web_event = {WebInputEvent::Type::kRawKeyDown, modifiers,
                                  WebInputEvent::GetStaticTimeStampForTests()};
    web_event.windows_key_code = key_code;
    auto* event = KeyboardEvent::Create(web_event, nullptr);
    if (target)
      event->SetTarget(target);

    return event;
  }

  MediaControlOverflowMenuButtonElement& GetPopupAnchor() {
    return *media_controls_->overflow_menu_.Get();
  }
  MediaControlPopupMenuElement& GetPopupMenu() {
    return *media_controls_->overflow_list_.Get();
  }
  MediaControlPopupMenuElement& GetPlaybackSpeedMenu() {
    return *media_controls_->playback_speed_list_.Get();
  }
  HTMLLabelElement& GetDownloadButtonLabel() {
    return *media_controls_->download_button_->overflow_label_element_.Get();
  }
  HTMLLabelElement& GetPlaybackSpeedButtonLabel() {
    return *media_controls_->playback_speed_button_->overflow_label_element_
                .Get();
  }
  HTMLMediaElement& GetMediaElement() { return *media_element_.Get(); }

 private:
  Persistent<HTMLMediaElement> media_element_;
  Persistent<MediaControlsImpl> media_controls_;
};

TEST_F(MediaControlPopupMenuElementTest,
       FocusMovesBackToPopupAnchorOnItemSelectedFromKeyboard) {
  EXPECT_FALSE(GetPopupMenu().IsWanted());
  GetPopupMenu().SetIsWanted(true);
  EXPECT_TRUE(GetPopupMenu().IsWanted());

  GetDownloadButtonLabel().DispatchEvent(
      *KeyDownEvent(VKEY_RETURN, &GetDownloadButtonLabel()));
  EXPECT_FALSE(GetPopupMenu().IsWanted());
  EXPECT_EQ(GetPopupAnchor(), GetDocument().FocusedElement());
}

TEST_F(MediaControlPopupMenuElementTest,
       FocusDoesntMoveBackToPopupAnchorOnItemSelectedFromMouseClick) {
  EXPECT_FALSE(GetPopupMenu().IsWanted());
  GetPopupMenu().SetIsWanted(true);
  EXPECT_TRUE(GetPopupMenu().IsWanted());

  GetDownloadButtonLabel().DispatchSimulatedClick(nullptr);
  EXPECT_FALSE(GetPopupMenu().IsWanted());
  EXPECT_NE(GetPopupAnchor(), GetDocument().FocusedElement());
}

TEST_F(
    MediaControlPopupMenuElementTest,
    FocusDoesntMoveBackToPopupAnchorOnItemSelectedFromKeyboardButMenuStillOpened) {
  EXPECT_FALSE(GetPopupMenu().IsWanted());
  GetPopupMenu().SetIsWanted(true);
  EXPECT_TRUE(GetPopupMenu().IsWanted());

  GetPlaybackSpeedButtonLabel().DispatchEvent(
      *KeyDownEvent(VKEY_RETURN, &GetPlaybackSpeedButtonLabel()));
  EXPECT_FALSE(GetPopupMenu().IsWanted());
  EXPECT_TRUE(GetPlaybackSpeedMenu().IsWanted());
  EXPECT_NE(GetPopupAnchor(), GetDocument().FocusedElement());
}

}  // namespace blink
