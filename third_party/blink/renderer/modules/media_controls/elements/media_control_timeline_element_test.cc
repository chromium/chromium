// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/media_controls/elements/media_control_timeline_element.h"

#include "third_party/blink/public/common/input/web_pointer_properties.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_pointer_event_init.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_touch_event_init.h"
#include "third_party/blink/renderer/core/events/pointer_event.h"
#include "third_party/blink/renderer/core/events/touch_event.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/modules/media_controls/media_controls_impl.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

class MediaControlTimelineElementTest : public PageTestBase {
 public:
  static PointerEventInit* GetValidPointerEventInit() {
    PointerEventInit* init = PointerEventInit::Create();
    init->setIsPrimary(true);
    init->setButton(static_cast<int>(WebPointerProperties::Button::kLeft));
    return init;
  }

  static TouchEventInit* GetValidTouchEventInit() {
    return TouchEventInit::Create();
  }

  void SetUp() override {
    PageTestBase::SetUp(gfx::Size(100, 100));

    video_ = MakeGarbageCollected<HTMLVideoElement>(GetDocument());
    controls_ = MakeGarbageCollected<MediaControlsImpl>(*video_);
    timeline_ = MakeGarbageCollected<MediaControlTimelineElement>(*controls_);

    controls_->InitializeControls();

    // Connects the timeline element. Ideally, we should be able to set the
    // NodeFlags::kConnectedFlag.
    GetDocument().body()->AppendChild(timeline_);
  }

  HTMLVideoElement* Video() const { return video_; }

  MediaControlTimelineElement* Timeline() const { return timeline_; }

 private:
  Persistent<HTMLVideoElement> video_;
  Persistent<MediaControlTimelineElement> timeline_;
  Persistent<MediaControlsImpl> controls_;
};

TEST_F(MediaControlTimelineElementTest, PointerDownPausesPlayback) {
  Video()->Play();
  ASSERT_FALSE(Video()->paused());

  Timeline()->DispatchEvent(*PointerEvent::Create(
      event_type_names::kPointerdown, GetValidPointerEventInit()));
  EXPECT_TRUE(Video()->paused());
}

TEST_F(MediaControlTimelineElementTest, PointerDownRightClickNoOp) {
  Video()->Play();
  ASSERT_FALSE(Video()->paused());

  PointerEventInit* init = GetValidPointerEventInit();
  init->setButton(static_cast<int>(WebPointerProperties::Button::kRight));
  Timeline()->DispatchEvent(
      *PointerEvent::Create(event_type_names::kPointerdown, init));
  EXPECT_FALSE(Video()->paused());
}

TEST_F(MediaControlTimelineElementTest, PointerDownNotPrimaryNoOp) {
  Video()->Play();
  ASSERT_FALSE(Video()->paused());

  PointerEventInit* init = GetValidPointerEventInit();
  init->setIsPrimary(false);
  Timeline()->DispatchEvent(
      *PointerEvent::Create(event_type_names::kPointerdown, init));
  EXPECT_FALSE(Video()->paused());
}

TEST_F(MediaControlTimelineElementTest, PointerUpResumesPlayback) {
  Video()->Play();
  ASSERT_FALSE(Video()->paused());

  Timeline()->DispatchEvent(*PointerEvent::Create(
      event_type_names::kPointerdown, GetValidPointerEventInit()));
  Timeline()->DispatchEvent(*PointerEvent::Create(event_type_names::kPointerup,
                                                  GetValidPointerEventInit()));
  EXPECT_FALSE(Video()->paused());
}

TEST_F(MediaControlTimelineElementTest, PointerUpRightClickNoOp) {
  Video()->Play();
  ASSERT_FALSE(Video()->paused());

  Timeline()->DispatchEvent(*PointerEvent::Create(
      event_type_names::kPointerdown, GetValidPointerEventInit()));

  PointerEventInit* init = GetValidPointerEventInit();
  init->setButton(static_cast<int>(WebPointerProperties::Button::kRight));
  Timeline()->DispatchEvent(
      *PointerEvent::Create(event_type_names::kPointerup, init));
  EXPECT_TRUE(Video()->paused());
}

TEST_F(MediaControlTimelineElementTest, PointerUpNotPrimaryNoOp) {
  Video()->Play();
  ASSERT_FALSE(Video()->paused());

  Timeline()->DispatchEvent(*PointerEvent::Create(
      event_type_names::kPointerdown, GetValidPointerEventInit()));

  PointerEventInit* init = GetValidPointerEventInit();
  init->setIsPrimary(false);
  Timeline()->DispatchEvent(
      *PointerEvent::Create(event_type_names::kPointerup, init));
  EXPECT_TRUE(Video()->paused());
}

TEST_F(MediaControlTimelineElementTest, PointerOutDoesNotResume) {
  Video()->Play();
  ASSERT_FALSE(Video()->paused());

  Timeline()->DispatchEvent(*PointerEvent::Create(
      event_type_names::kPointerdown, GetValidPointerEventInit()));
  Timeline()->DispatchEvent(*PointerEvent::Create(event_type_names::kPointerout,
                                                  GetValidPointerEventInit()));
  EXPECT_TRUE(Video()->paused());
}

TEST_F(MediaControlTimelineElementTest, PointerMoveDoesNotResume) {
  Video()->Play();
  ASSERT_FALSE(Video()->paused());

  Timeline()->DispatchEvent(*PointerEvent::Create(
      event_type_names::kPointerdown, GetValidPointerEventInit()));
  Timeline()->DispatchEvent(*PointerEvent::Create(
      event_type_names::kPointermove, GetValidPointerEventInit()));
  EXPECT_TRUE(Video()->paused());
}

TEST_F(MediaControlTimelineElementTest, PointerCancelResumesPlayback) {
  Video()->Play();
  ASSERT_FALSE(Video()->paused());

  Timeline()->DispatchEvent(*PointerEvent::Create(
      event_type_names::kPointerdown, GetValidPointerEventInit()));
  Timeline()->DispatchEvent(*PointerEvent::Create(
      event_type_names::kPointercancel, GetValidPointerEventInit()));
  EXPECT_FALSE(Video()->paused());
}

TEST_F(MediaControlTimelineElementTest, TouchStartPausesPlayback) {
  Video()->Play();
  ASSERT_FALSE(Video()->paused());

  Timeline()->DispatchEvent(*TouchEvent::Create(event_type_names::kTouchstart,
                                                GetValidTouchEventInit()));
  EXPECT_TRUE(Video()->paused());
}

TEST_F(MediaControlTimelineElementTest, TouchEndResumesPlayback) {
  Video()->Play();
  ASSERT_FALSE(Video()->paused());

  Timeline()->DispatchEvent(*TouchEvent::Create(event_type_names::kTouchstart,
                                                GetValidTouchEventInit()));
  Timeline()->DispatchEvent(*TouchEvent::Create(event_type_names::kTouchend,
                                                GetValidTouchEventInit()));
  EXPECT_FALSE(Video()->paused());
}

TEST_F(MediaControlTimelineElementTest, TouchCancelResumesPlayback) {
  Video()->Play();
  ASSERT_FALSE(Video()->paused());

  Timeline()->DispatchEvent(*TouchEvent::Create(event_type_names::kTouchstart,
                                                GetValidTouchEventInit()));
  Timeline()->DispatchEvent(*TouchEvent::Create(event_type_names::kTouchcancel,
                                                GetValidTouchEventInit()));
  EXPECT_FALSE(Video()->paused());
}

TEST_F(MediaControlTimelineElementTest, ChangeResumesPlayback) {
  Video()->Play();
  ASSERT_FALSE(Video()->paused());

  Timeline()->DispatchEvent(*TouchEvent::Create(event_type_names::kTouchstart,
                                                GetValidTouchEventInit()));
  Timeline()->DispatchEvent(
      *Event::Create(event_type_names::kChange, GetValidTouchEventInit()));
  EXPECT_FALSE(Video()->paused());
}

TEST_F(MediaControlTimelineElementTest, TouchMoveDoesNotResume) {
  Video()->Play();
  ASSERT_FALSE(Video()->paused());

  Timeline()->DispatchEvent(*TouchEvent::Create(event_type_names::kTouchstart,
                                                GetValidTouchEventInit()));
  Timeline()->DispatchEvent(*TouchEvent::Create(event_type_names::kTouchmove,
                                                GetValidTouchEventInit()));
  EXPECT_TRUE(Video()->paused());
}

TEST_F(MediaControlTimelineElementTest, TouchMoveAfterPointerDoesNotResume) {
  Video()->Play();
  ASSERT_FALSE(Video()->paused());

  Timeline()->DispatchEvent(*PointerEvent::Create(
      event_type_names::kPointerdown, GetValidPointerEventInit()));
  Timeline()->DispatchEvent(*TouchEvent::Create(event_type_names::kTouchmove,
                                                GetValidTouchEventInit()));
  EXPECT_TRUE(Video()->paused());
}

TEST_F(MediaControlTimelineElementTest, TouchEndAfterPointerDoesNotResume) {
  Video()->Play();
  ASSERT_FALSE(Video()->paused());

  Timeline()->DispatchEvent(*PointerEvent::Create(
      event_type_names::kPointerdown, GetValidPointerEventInit()));
  Timeline()->DispatchEvent(*TouchEvent::Create(event_type_names::kTouchend,
                                                GetValidTouchEventInit()));
  EXPECT_TRUE(Video()->paused());
}

TEST_F(MediaControlTimelineElementTest, TouchCancelAfterPointerDoesNotResume) {
  Video()->Play();
  ASSERT_FALSE(Video()->paused());

  Timeline()->DispatchEvent(*PointerEvent::Create(
      event_type_names::kPointerdown, GetValidPointerEventInit()));
  Timeline()->DispatchEvent(*TouchEvent::Create(event_type_names::kTouchcancel,
                                                GetValidTouchEventInit()));
  EXPECT_TRUE(Video()->paused());
}

TEST_F(MediaControlTimelineElementTest, ChangeAfterPointerDoesNotResume) {
  Video()->Play();
  ASSERT_FALSE(Video()->paused());

  Timeline()->DispatchEvent(*PointerEvent::Create(
      event_type_names::kPointerdown, GetValidPointerEventInit()));
  Timeline()->DispatchEvent(
      *Event::Create(event_type_names::kChange, GetValidTouchEventInit()));
  EXPECT_TRUE(Video()->paused());
}

TEST_F(MediaControlTimelineElementTest, PointerUpAfterTouchDoesNotResume) {
  Video()->Play();
  ASSERT_FALSE(Video()->paused());

  Timeline()->DispatchEvent(*TouchEvent::Create(event_type_names::kTouchstart,
                                                GetValidTouchEventInit()));
  Timeline()->DispatchEvent(*PointerEvent::Create(event_type_names::kPointerup,
                                                  GetValidPointerEventInit()));
  EXPECT_TRUE(Video()->paused());
}

TEST_F(MediaControlTimelineElementTest, PointerCancelAfterTouchDoesNotResume) {
  Video()->Play();
  ASSERT_FALSE(Video()->paused());

  Timeline()->DispatchEvent(*TouchEvent::Create(event_type_names::kTouchstart,
                                                GetValidTouchEventInit()));
  Timeline()->DispatchEvent(*PointerEvent::Create(
      event_type_names::kPointercancel, GetValidPointerEventInit()));
  EXPECT_TRUE(Video()->paused());
}

TEST_F(MediaControlTimelineElementTest, UpgradePointerEventToTouchAllowed) {
  Video()->Play();
  ASSERT_FALSE(Video()->paused());

  Timeline()->DispatchEvent(*PointerEvent::Create(
      event_type_names::kPointerdown, GetValidPointerEventInit()));
  Timeline()->DispatchEvent(*TouchEvent::Create(event_type_names::kTouchstart,
                                                GetValidTouchEventInit()));
  Timeline()->DispatchEvent(*TouchEvent::Create(event_type_names::kTouchend,
                                                GetValidTouchEventInit()));
  EXPECT_FALSE(Video()->paused());
}

TEST_F(MediaControlTimelineElementTest, UpgradeTouchEventToPointerDenied) {
  Video()->Play();
  ASSERT_FALSE(Video()->paused());

  Timeline()->DispatchEvent(*TouchEvent::Create(event_type_names::kTouchstart,
                                                GetValidTouchEventInit()));
  Timeline()->DispatchEvent(*PointerEvent::Create(
      event_type_names::kPointerdown, GetValidPointerEventInit()));
  Timeline()->DispatchEvent(*PointerEvent::Create(event_type_names::kPointerup,
                                                  GetValidPointerEventInit()));
  EXPECT_TRUE(Video()->paused());
}

}  // namespace blink
