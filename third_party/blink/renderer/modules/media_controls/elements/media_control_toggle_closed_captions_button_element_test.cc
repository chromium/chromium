// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/media_controls/elements/media_control_toggle_closed_captions_button_element.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/strings/grit/blink_strings.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_text_track_kind.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/modules/media_controls/media_controls_impl.h"
#include "third_party/blink/renderer/modules/media_controls/media_controls_text_track_manager.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"

namespace blink {

namespace {

const char kTextTracksOffString[] = "Off";
const char kEnglishLabel[] = "English";

class LocalePlatformSupport : public TestingPlatformSupport {
 public:
  WebString QueryLocalizedString(int resource_id) override {
    if (resource_id == IDS_MEDIA_TRACKS_OFF)
      return kTextTracksOffString;
    return TestingPlatformSupport::QueryLocalizedString(resource_id);
  }
};

}  // anonymous namespace

class MediaControlToggleClosedCaptionsButtonElementTest : public PageTestBase {
 public:
  void SetUp() final {
    PageTestBase::SetUp();
    SetBodyInnerHTML("<video controls></video>");
    media_element_ =
        static_cast<HTMLMediaElement*>(GetDocument().body()->firstChild());
    media_controls_ =
        static_cast<MediaControlsImpl*>(media_element_->GetMediaControls());
    captions_overflow_button_ =
        MakeGarbageCollected<MediaControlToggleClosedCaptionsButtonElement>(
            *media_controls_);
  }

 protected:
  HTMLMediaElement* MediaElement() { return media_element_; }
  void SelectTextTrack(unsigned index) {
    media_controls_->GetTextTrackManager().ShowTextTrackAtIndex(index);
  }
  void SelectOff() {
    media_controls_->GetTextTrackManager().DisableShowingTextTracks();
  }
  String GetOverflowMenuSubtitleString() {
    return captions_overflow_button_->GetOverflowMenuSubtitleString();
  }

 private:
  Persistent<HTMLMediaElement> media_element_;
  Persistent<MediaControlsImpl> media_controls_;
  Persistent<MediaControlToggleClosedCaptionsButtonElement>
      captions_overflow_button_;
};

TEST_F(MediaControlToggleClosedCaptionsButtonElementTest,
       SubtitleStringMatchesSelectedTrack) {
  ScopedTestingPlatformSupport<LocalePlatformSupport> support;

  // Before any text tracks are added, the subtitle string should be null.
  EXPECT_EQ(String(), GetOverflowMenuSubtitleString());

  // After adding a text track, the subtitle string should be off.
  MediaElement()->addTextTrack(
      V8TextTrackKind(V8TextTrackKind::Enum::kSubtitles),
      AtomicString(kEnglishLabel), AtomicString("en"), ASSERT_NO_EXCEPTION);
  EXPECT_EQ(kTextTracksOffString, GetOverflowMenuSubtitleString());

  // After selecting the text track, the subtitle string should match the label.
  SelectTextTrack(0);
  EXPECT_EQ(kEnglishLabel, GetOverflowMenuSubtitleString());

  // After selecting off, the subtitle string should be off again.
  SelectOff();
  EXPECT_EQ(kTextTracksOffString, GetOverflowMenuSubtitleString());
}

}  // namespace blink
