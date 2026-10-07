// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/track/cue_timeline.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/html/html_body_element.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/html/track/text_track.h"
#include "third_party/blink/renderer/core/html/track/text_track_cue_list.h"
#include "third_party/blink/renderer/core/html/track/text_track_list.h"
#include "third_party/blink/renderer/core/html/track/vtt/vtt_cue.h"
#include "third_party/blink/renderer/core/html/track/vtt/vtt_cue_box.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/media/media_player_client.h"
#include "third_party/blink/renderer/platform/testing/empty_web_media_player.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

namespace {

class TestWebMediaPlayer final : public EmptyWebMediaPlayer {
 public:
  ReadyState GetReadyState() const override {
    return kReadyStateHaveEnoughData;
  }
  bool HasVideo() const override { return true; }
  double Duration() const override { return 10.0; }
  double CurrentTime() const override { return current_time_; }
  void Seek(double seconds) override { current_time_ = seconds; }
  WebTimeRanges Seekable() const override { return WebTimeRanges(0.0, 10.0); }

 private:
  double current_time_ = 0.0;
};

class TestLocalFrameClient final : public EmptyLocalFrameClient {
 public:
  std::unique_ptr<WebMediaPlayer> CreateWebMediaPlayer(
      HTMLMediaElement&,
      const WebMediaPlayerSource&,
      WebMediaPlayerClient* client) override {
    client_ = static_cast<MediaPlayerClient*>(client);
    return std::make_unique<TestWebMediaPlayer>();
  }

  MediaPlayerClient* client() const { return client_.Get(); }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(client_);
    EmptyLocalFrameClient::Trace(visitor);
  }

 private:
  Member<MediaPlayerClient> client_;
};

class ReentrantFocusoutListener final : public NativeEventListener {
 public:
  ReentrantFocusoutListener(TextTrack* target_track, VTTCue* cue)
      : target_track_(target_track), cue_(cue) {}

  void Invoke(ExecutionContext*, Event*) override {
    if (invoked_ || !cue_) {
      return;
    }
    invoked_ = true;
    target_track_->addCue(cue_);
  }

  void ClearCue() { cue_ = nullptr; }
  bool invoked() const { return invoked_; }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(target_track_);
    visitor->Trace(cue_);
    NativeEventListener::Trace(visitor);
  }

 private:
  Member<TextTrack> target_track_;
  Member<VTTCue> cue_;
  bool invoked_ = false;
};

}  // namespace

// Regression test: CueEventTimerFired should not crash when poster flag is set.
TEST(CueTimelineTest, CueEventTimerFiredWithPosterFlagSet) {
  test::TaskEnvironment task_environment;
  auto page_holder = std::make_unique<DummyPageHolder>();
  auto* video =
      MakeGarbageCollected<HTMLVideoElement>(page_holder->GetDocument());

  // Video starts with show_poster_flag_ = true
  ASSERT_TRUE(video->IsShowPosterFlagSet());

  // Create text track to initialize CueTimeline
  TextTrack* track = MakeGarbageCollected<TextTrack>(
      V8TextTrackKind(V8TextTrackKind::Enum::kCaptions), g_empty_atom,
      g_empty_atom, *video);
  video->textTracks()->Append(track);

  CueTimeline& cue_timeline = video->GetCueTimeline();

  // Should not crash - guard in CueEventTimerFired
  cue_timeline.CueEventTimerFired(nullptr);
}

// Regression test for b/568371493: Transferring or re-adding a cue during
// synchronous focusout dispatch must not leave stale raw pointers in
// CueTimeline.
TEST(CueTimelineTest, CueTransferDuringFocusoutDoesNotLeaveStaleCue) {
  test::TaskEnvironment task_environment;
  ScopedOmitBlurEventOnElementRemovalForTest omit_blur_on_removal(false);

  Persistent<TestLocalFrameClient> frame_client =
      MakeGarbageCollected<TestLocalFrameClient>();
  auto page_holder = std::make_unique<DummyPageHolder>(gfx::Size(800, 600),
                                                       nullptr, frame_client);
  page_holder->GetPage().GetFocusController().SetActive(true);
  page_holder->GetPage().GetFocusController().SetFocused(true);

  Persistent<HTMLVideoElement> video =
      MakeGarbageCollected<HTMLVideoElement>(page_holder->GetDocument());
  page_holder->GetDocument().body()->AppendChild(video);
  video->SetSrc(AtomicString("http://example.com/foo.mp4"));
  test::RunPendingTasks();
  ASSERT_NE(frame_client->client(), nullptr);
  frame_client->client()->ReadyStateChanged();

  video->setCurrentTime(0.5);
  frame_client->client()->TimeChanged();

  Persistent<TextTrack> track1 = video->addTextTrack(
      V8TextTrackKind(V8TextTrackKind::Enum::kSubtitles), AtomicString("t1"),
      AtomicString("en"), ASSERT_NO_EXCEPTION);
  Persistent<TextTrack> track2 = video->addTextTrack(
      V8TextTrackKind(V8TextTrackKind::Enum::kSubtitles), AtomicString("t2"),
      AtomicString("en"), ASSERT_NO_EXCEPTION);
  track1->SetModeEnum(TextTrackMode::kShowing);
  track2->SetModeEnum(TextTrackMode::kShowing);

  auto focus_first_cue_box = [&]() {
    Document& document = page_holder->GetDocument();
    VTTCueBox* display_box =
        Traversal<VTTCueBox>::FirstWithin(*video->UserAgentShadowRoot());
    ASSERT_NE(display_box, nullptr);
    display_box->setTabIndex(0);
    document.UpdateStyleAndLayout(DocumentUpdateReason::kTest);
    display_box->Focus();
    ASSERT_EQ(document.FocusedElement(), display_box);
  };

  [&]() {
    Document& document = page_holder->GetDocument();
    auto* cue = VTTCue::Create(document, 0.0, 2.0, "cue text");
    track1->addCue(cue);
    ASSERT_TRUE(cue->IsActive());
    focus_first_cue_box();

    // Case 1: Transferring `cue` to `track2` while focused fires `focusout`,
    // which re-adds `cue` to `track1`.
    auto* listener1 =
        MakeGarbageCollected<ReentrantFocusoutListener>(track1, cue);
    document.addEventListener(event_type_names::kFocusout, listener1, false);
    track2->addCue(cue);
    EXPECT_TRUE(listener1->invoked());
    EXPECT_EQ(cue->track(), track2);
    EXPECT_EQ(track1->cues()->length(), 0u);
    EXPECT_EQ(track2->cues()->length(), 1u);
    listener1->ClearCue();

    // Case 2: Mutating `startTime` while focused fires `focusout`, which
    // transfers `cue` to `track1`.
    focus_first_cue_box();
    auto* listener2 =
        MakeGarbageCollected<ReentrantFocusoutListener>(track1, cue);
    document.addEventListener(event_type_names::kFocusout, listener2, false);
    cue->setStartTime(1.0);
    EXPECT_TRUE(listener2->invoked());
    EXPECT_EQ(cue->track(), track1);
    listener2->ClearCue();

    track1->removeCue(cue, ASSERT_NO_EXCEPTION);
    track1->SetModeEnum(TextTrackMode::kDisabled);
    track2->SetModeEnum(TextTrackMode::kDisabled);
  }();

  test::RunPendingTasks();
  ThreadState::Current()->CollectAllGarbageForTesting();

  video->setCurrentTime(0.6);
  frame_client->client()->TimeChanged();
  EXPECT_TRUE(video->GetCueTimeline().CurrentlyActiveCues().empty());

  video.Clear();
  track1.Clear();
  track2.Clear();
  frame_client.Clear();
  page_holder.reset();
  test::RunPendingTasks();
}

}  // namespace blink
