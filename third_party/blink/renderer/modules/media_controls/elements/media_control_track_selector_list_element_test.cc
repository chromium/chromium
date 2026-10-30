// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/media_controls/elements/media_control_track_selector_list_element.h"

#include <vector>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_media_player.h"
#include "third_party/blink/public/strings/grit/blink_strings.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/html/track/audio_track.h"
#include "third_party/blink/renderer/core/html/track/audio_track_list.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/modules/media_controls/media_controls_impl.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"
#include "third_party/blink/renderer/platform/wtf/text/strcat.h"

namespace blink {
namespace {

using ::testing::ElementsAre;

class LocalePlatformSupport : public TestingPlatformSupport {
 public:
  WebString QueryLocalizedString(int resource_id,
                                 const WebString& parameter) override {
    if (resource_id == IDS_MEDIA_TRACK_IDENTIFIER_VERBOSE_TITLE_NUMBERED) {
      return StrCat({"Track #", String(parameter)});
    }
    return TestingPlatformSupport::QueryLocalizedString(resource_id, parameter);
  }
};

class MediaControlTrackSelectorListElementTest : public PageTestBase {
 public:
  void SetUp() final {
    PageTestBase::SetUp();
    media_element_ = MakeGarbageCollected<HTMLVideoElement>(GetDocument());
    media_element_->SetBooleanAttribute(html_names::kControlsAttr, true);
    GetDocument().body()->AppendChild(media_element_);

    media_controls_ =
        static_cast<MediaControlsImpl*>(media_element_->GetMediaControls());
    ASSERT_NE(nullptr, media_controls_);
  }

 protected:
  HTMLMediaElement& MediaElement() { return *media_element_; }

  std::vector<String> GetAudioTrackLabels() {
    media_controls_->ToggleTrackSelectionList(
        WebMediaPlayer::TrackType::kAudioTrack);
    std::vector<String> labels;
    for (Element& element : ElementTraversal::DescendantsOf(*media_controls_)) {
      if (element.ShadowPseudoId() ==
          "-internal-media-controls-track-selection-list-item") {
        labels.push_back(element.getAttribute(html_names::kAriaLabelAttr));
      }
    }
    return labels;
  }

 private:
  Persistent<HTMLMediaElement> media_element_;
  Persistent<MediaControlsImpl> media_controls_;
};

TEST_F(MediaControlTrackSelectorListElementTest,
       FallsBackWhenNonAdjacentTrackLabelsAreDuplicates) {
  // Labels have non-adjacent duplicates ("Foo", "Bar", "Foo"), so
  // `GetPreferenceList` should reject labels and fall back to unique languages
  // ("en", "es", "fr").
  MediaElement().audioTracks().Add(MakeGarbageCollected<AudioTrack>(
      "1", AudioTrack::MainKeyword(), AtomicString("Foo"), AtomicString("en"),
      /*enabled=*/true, /*exclusive=*/true));
  MediaElement().audioTracks().Add(MakeGarbageCollected<AudioTrack>(
      "2", AudioTrack::MainKeyword(), AtomicString("Bar"), AtomicString("es"),
      /*enabled=*/false, /*exclusive=*/true));
  MediaElement().audioTracks().Add(MakeGarbageCollected<AudioTrack>(
      "3", AudioTrack::MainKeyword(), AtomicString("Foo"), AtomicString("fr"),
      /*enabled=*/false, /*exclusive=*/true));

  EXPECT_THAT(GetAudioTrackLabels(), ElementsAre("en", "es", "fr"));
}

TEST_F(MediaControlTrackSelectorListElementTest,
       FallsBackToKindWhenTrackLabelsAndLanguagesAreDuplicates) {
  MediaElement().audioTracks().Add(MakeGarbageCollected<AudioTrack>(
      "1", AudioTrack::MainKeyword(), AtomicString("Foo"), AtomicString("en"),
      /*enabled=*/true, /*exclusive=*/true));
  MediaElement().audioTracks().Add(MakeGarbageCollected<AudioTrack>(
      "2", AudioTrack::CommentaryKeyword(), AtomicString("Bar"),
      AtomicString("es"), /*enabled=*/false, /*exclusive=*/true));
  MediaElement().audioTracks().Add(MakeGarbageCollected<AudioTrack>(
      "3", AudioTrack::DescriptionsKeyword(), AtomicString("Foo"),
      AtomicString("en"), /*enabled=*/false, /*exclusive=*/true));

  EXPECT_THAT(GetAudioTrackLabels(),
              ElementsAre("main", "commentary", "descriptions"));
}

TEST_F(MediaControlTrackSelectorListElementTest,
       FallsBackToIdWhenAllTrackPropertiesAreDuplicates) {
  ScopedTestingPlatformSupport<LocalePlatformSupport> support;

  MediaElement().audioTracks().Add(MakeGarbageCollected<AudioTrack>(
      "1", AudioTrack::MainKeyword(), AtomicString("Foo"), AtomicString("en"),
      /*enabled=*/true, /*exclusive=*/true));
  MediaElement().audioTracks().Add(MakeGarbageCollected<AudioTrack>(
      "2", AudioTrack::MainKeyword(), AtomicString("Bar"), AtomicString("es"),
      /*enabled=*/false, /*exclusive=*/true));
  MediaElement().audioTracks().Add(MakeGarbageCollected<AudioTrack>(
      "3", AudioTrack::MainKeyword(), AtomicString("Foo"), AtomicString("en"),
      /*enabled=*/false, /*exclusive=*/true));

  EXPECT_THAT(GetAudioTrackLabels(),
              ElementsAre("Track #1", "Track #2", "Track #3"));
}

}  // namespace
}  // namespace blink
