// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/track/cue_timeline.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/html/html_body_element.h"
#include "third_party/blink/renderer/core/html/html_div_element.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/html/track/text_track.h"
#include "third_party/blink/renderer/core/html/track/text_track_container.h"
#include "third_party/blink/renderer/core/html/track/text_track_cue_list.h"
#include "third_party/blink/renderer/core/html/track/text_track_list.h"
#include "third_party/blink/renderer/core/html/track/vtt/vtt_cue.h"
#include "third_party/blink/renderer/core/html/track/vtt/vtt_cue_box.h"
#include "third_party/blink/renderer/core/html/track/vtt/vtt_element.h"
#include "third_party/blink/renderer/core/html/track/vtt/vtt_region.h"
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

// Focuses a synthetic probe element inside the first rendered cue box so that
// removing or reparenting the box synchronously dispatches blur/focusout. All
// UA text track elements themselves refuse focus, so we append a test-only
// HTMLDivElement child to exercise the defense-in-depth re-entrancy guards.
void FocusInsideFirstCueBox(Document& document, HTMLVideoElement& video) {
  VTTCueBox* display_box =
      Traversal<VTTCueBox>::FirstWithin(*video.UserAgentShadowRoot());
  ASSERT_NE(display_box, nullptr);
  auto* inner = MakeGarbageCollected<HTMLDivElement>(document);
  display_box->AppendChild(inner);
  inner->setTabIndex(0);
  document.UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  inner->Focus();
  ASSERT_EQ(document.FocusedElement(), inner);
}

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
    FocusInsideFirstCueBox(page_holder->GetDocument(), *video);
  };

  [&]() {
    Document& document = page_holder->GetDocument();
    auto* cue = VTTCue::Create(document, 0.0, 2.0, "<b>cue</b> <i>text</i>");
    track1->addCue(cue);
    ASSERT_TRUE(cue->IsActive());
    auto* backdrop = Traversal<VTTCueBackgroundBox>::FirstWithin(
        *video->UserAgentShadowRoot());
    ASSERT_NE(backdrop, nullptr);
    EXPECT_EQ(backdrop->GetTrack(), track1);
    ASSERT_NE(Traversal<VTTElement>::FirstWithin(*backdrop), nullptr);
    for (const VTTElement& element :
         Traversal<VTTElement>::DescendantsOf(*backdrop)) {
      EXPECT_EQ(element.GetTrack(), track1);
    }
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
    EXPECT_EQ(backdrop->GetTrack(), track2);
    ASSERT_NE(Traversal<VTTElement>::FirstWithin(*backdrop), nullptr);
    for (const VTTElement& element :
         Traversal<VTTElement>::DescendantsOf(*backdrop)) {
      EXPECT_EQ(element.GetTrack(), track2);
    }
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
    EXPECT_EQ(backdrop->GetTrack(), nullptr);
    for (const VTTElement& element :
         Traversal<VTTElement>::DescendantsOf(*backdrop)) {
      EXPECT_EQ(element.GetTrack(), nullptr);
    }
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

// Regression test for b/572139051: Re-adding a cue during synchronous focusout
// dispatch from TextTrack::Reset() or TextTrackList::Remove() must not leave
// stale raw pointers in CueTimeline.
TEST(CueTimelineTest, ResetDuringFocusoutDoesNotLeaveStaleCue) {
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

  Persistent<TextTrack> track = video->addTextTrack(
      V8TextTrackKind(V8TextTrackKind::Enum::kSubtitles), AtomicString("t1"),
      AtomicString("en"), ASSERT_NO_EXCEPTION);
  track->SetModeEnum(TextTrackMode::kShowing);

  auto focus_first_cue_box = [&]() {
    FocusInsideFirstCueBox(page_holder->GetDocument(), *video);
  };

  [&]() {
    Document& document = page_holder->GetDocument();

    // Case 1: Re-adding a cue during TextTrack::Reset().
    auto* cue1 = VTTCue::Create(document, 0.0, 2.0, "cue 1");
    auto* readded_cue = VTTCue::Create(document, 4.0, 6.0, "readded cue");
    track->addCue(cue1);
    ASSERT_TRUE(cue1->IsActive());
    focus_first_cue_box();

    auto* listener1 =
        MakeGarbageCollected<ReentrantFocusoutListener>(track, readded_cue);
    document.addEventListener(event_type_names::kFocusout, listener1, false);
    track->Reset();
    EXPECT_TRUE(listener1->invoked());
    EXPECT_EQ(cue1->track(), nullptr);
    EXPECT_EQ(readded_cue->track(), track);
    EXPECT_EQ(track->cues()->length(), 1u);
    listener1->ClearCue();

    // Case 2: Re-adding a cue during TextTrackList::Remove().
    auto* cue2 = VTTCue::Create(document, 0.0, 2.0, "cue 2");
    auto* detached_cue = VTTCue::Create(document, 4.0, 6.0, "detached cue");
    track->addCue(cue2);
    ASSERT_TRUE(cue2->IsActive());
    focus_first_cue_box();

    auto* listener2 =
        MakeGarbageCollected<ReentrantFocusoutListener>(track, detached_cue);
    document.addEventListener(event_type_names::kFocusout, listener2, false);
    video->textTracks()->Remove(track);
    EXPECT_TRUE(listener2->invoked());
    listener2->ClearCue();
  }();

  track.Clear();
  test::RunPendingTasks();
  ThreadState::Current()->CollectAllGarbageForTesting();

  video->setCurrentTime(5.0);
  frame_client->client()->TimeChanged();
  EXPECT_TRUE(video->GetCueTimeline().CurrentlyActiveCues().empty());

  video.Clear();
  frame_client.Clear();
  page_holder.reset();
  test::RunPendingTasks();
}

// The UA text track rendering elements are reachable by author style sheets
// via ::-webkit-media-text-track-{container,display,region,region-container}
// and must never become focusable (via tabindex, editability, or being a
// scroller), because removing or reparenting a focused element would run
// script synchronously from inside CueTimeline/TextTrack mutations.
TEST(CueTimelineTest, TextTrackRenderingElementsAreNotFocusable) {
  test::TaskEnvironment task_environment;

  Persistent<TestLocalFrameClient> frame_client =
      MakeGarbageCollected<TestLocalFrameClient>();
  auto page_holder = std::make_unique<DummyPageHolder>(gfx::Size(800, 600),
                                                       nullptr, frame_client);
  page_holder->GetPage().GetFocusController().SetActive(true);
  page_holder->GetPage().GetFocusController().SetFocused(true);
  Document& document = page_holder->GetDocument();

  Persistent<HTMLVideoElement> video =
      MakeGarbageCollected<HTMLVideoElement>(document);
  document.body()->AppendChild(video);
  video->SetSrc(AtomicString("http://example.com/foo.mp4"));
  test::RunPendingTasks();
  ASSERT_NE(frame_client->client(), nullptr);
  frame_client->client()->ReadyStateChanged();

  video->setCurrentTime(0.5);
  frame_client->client()->TimeChanged();

  Persistent<TextTrack> track = video->addTextTrack(
      V8TextTrackKind(V8TextTrackKind::Enum::kSubtitles), AtomicString("t1"),
      AtomicString("en"), ASSERT_NO_EXCEPTION);
  track->SetModeEnum(TextTrackMode::kShowing);

  auto* region = VTTRegion::Create(document);
  auto* cue = VTTCue::Create(document, 0.0, 2.0,
                             "<00:00:01.000><b>line 1</b>\nline 2\nline 3");
  cue->setRegion(region);
  track->addCue(cue);
  ASSERT_TRUE(cue->IsActive());

  VTTCueBox* display_box =
      Traversal<VTTCueBox>::FirstWithin(*video->UserAgentShadowRoot());
  ASSERT_NE(display_box, nullptr);
  auto* backdrop = Traversal<VTTCueBackgroundBox>::FirstWithin(*display_box);
  ASSERT_NE(backdrop, nullptr);
  auto* vtt_element = Traversal<VTTElement>::FirstWithin(*display_box);
  ASSERT_NE(vtt_element, nullptr);
  auto* container =
      Traversal<TextTrackContainer>::FirstWithin(*video->UserAgentShadowRoot());
  ASSERT_NE(container, nullptr);
  HTMLDivElement* region_box = region->GetDisplayTree(document);
  ASSERT_NE(region_box, nullptr);
  auto* region_container = DynamicTo<HTMLDivElement>(region_box->firstChild());
  ASSERT_NE(region_container, nullptr);
  ASSERT_TRUE(region_container->contains(display_box));

  for (Element* element :
       {static_cast<Element*>(display_box), static_cast<Element*>(backdrop),
        static_cast<Element*>(vtt_element), static_cast<Element*>(container),
        static_cast<Element*>(region_box),
        static_cast<Element*>(region_container)}) {
    // Each of the ways a web page could try to make the element focusable.
    element->setTabIndex(0);
    if (element->IsStyledElement()) {
      element->SetInlineStyleProperty(CSSPropertyID::kDisplay,
                                      CSSValueID::kBlock);
      element->SetInlineStyleProperty(CSSPropertyID::kWebkitUserModify,
                                      CSSValueID::kReadWrite);
      element->SetInlineStyleProperty(CSSPropertyID::kOverflow,
                                      CSSValueID::kScroll);
      element->SetInlineStyleProperty(CSSPropertyID::kMaxHeight, 1.0,
                                      CSSPrimitiveValue::UnitType::kPixels);
    }
    document.UpdateStyleAndLayout(DocumentUpdateReason::kTest);

    EXPECT_FALSE(element->IsFocusable()) << element->ShadowPseudoId();
    element->Focus();
    EXPECT_NE(document.FocusedElement(), element) << element->ShadowPseudoId();
  }

  track->SetModeEnum(TextTrackMode::kDisabled);
  track.Clear();
  video.Clear();
  frame_client.Clear();
  page_holder.reset();
  test::RunPendingTasks();
}

// A cue keeps its (detached) display tree after being deactivated. If script
// then moves the cue into a region whose display tree was never created and
// the display tree is removed with region notification (e.g. HideCues() on a
// mode change), VTTRegion::WillRemoveVTTCueBox() must not dereference the
// missing cue container.
TEST(CueTimelineTest, RemoveDisplayTreeWithUnrenderedRegionDoesNotCrash) {
  test::TaskEnvironment task_environment;

  Persistent<TestLocalFrameClient> frame_client =
      MakeGarbageCollected<TestLocalFrameClient>();
  auto page_holder = std::make_unique<DummyPageHolder>(gfx::Size(800, 600),
                                                       nullptr, frame_client);
  Document& document = page_holder->GetDocument();

  Persistent<HTMLVideoElement> video =
      MakeGarbageCollected<HTMLVideoElement>(document);
  document.body()->AppendChild(video);
  video->SetSrc(AtomicString("http://example.com/foo.mp4"));
  test::RunPendingTasks();
  ASSERT_NE(frame_client->client(), nullptr);
  frame_client->client()->ReadyStateChanged();

  video->setCurrentTime(0.5);
  frame_client->client()->TimeChanged();

  Persistent<TextTrack> track = video->addTextTrack(
      V8TextTrackKind(V8TextTrackKind::Enum::kSubtitles), AtomicString("t1"),
      AtomicString("en"), ASSERT_NO_EXCEPTION);
  track->SetModeEnum(TextTrackMode::kShowing);

  auto* cue = VTTCue::Create(document, 0.0, 2.0, "cue text");
  track->addCue(cue);
  ASSERT_TRUE(cue->IsActive());
  ASSERT_NE(Traversal<VTTCueBox>::FirstWithin(*video->UserAgentShadowRoot()),
            nullptr);

  // Deactivate the cue; its display tree is detached but retained.
  track->SetModeEnum(TextTrackMode::kDisabled);
  ASSERT_FALSE(cue->IsActive());

  // Move the inactive cue into a region that has never been rendered.
  cue->setRegion(VTTRegion::Create(document));

  // HideCues() removes display trees with region notification.
  track->SetModeEnum(TextTrackMode::kHidden);

  track->SetModeEnum(TextTrackMode::kDisabled);
  track.Clear();
  video.Clear();
  frame_client.Clear();
  page_holder.reset();
  test::RunPendingTasks();
}

TEST(CueTimelineTest, ActiveCueMutationUpdatesDisplayTreeImmediately) {
  test::TaskEnvironment task_environment;

  Persistent<TestLocalFrameClient> frame_client =
      MakeGarbageCollected<TestLocalFrameClient>();
  auto page_holder = std::make_unique<DummyPageHolder>(gfx::Size(800, 600),
                                                       nullptr, frame_client);
  Document& document = page_holder->GetDocument();

  Persistent<HTMLVideoElement> video =
      MakeGarbageCollected<HTMLVideoElement>(document);
  document.body()->AppendChild(video);
  video->SetSrc(AtomicString("http://example.com/foo.mp4"));
  test::RunPendingTasks();
  ASSERT_NE(frame_client->client(), nullptr);
  frame_client->client()->ReadyStateChanged();

  video->setCurrentTime(0.5);
  frame_client->client()->TimeChanged();

  Persistent<TextTrack> track = video->addTextTrack(
      V8TextTrackKind(V8TextTrackKind::Enum::kSubtitles), AtomicString("t1"),
      AtomicString("en"), ASSERT_NO_EXCEPTION);
  track->SetModeEnum(TextTrackMode::kShowing);

  auto* cue = VTTCue::Create(document, 0.0, 2.0, "initial text");
  track->addCue(cue);
  ASSERT_TRUE(cue->IsActive());

  VTTCueBox* display_box =
      Traversal<VTTCueBox>::FirstWithin(*video->UserAgentShadowRoot());
  ASSERT_NE(display_box, nullptr);
  EXPECT_EQ(display_box->textContent(), "initial text");

  cue->setText("updated text");
  EXPECT_EQ(display_box->textContent(), "updated text");

  track->SetModeEnum(TextTrackMode::kDisabled);
  track.Clear();
  video.Clear();
  frame_client.Clear();
  page_holder.reset();
  test::RunPendingTasks();
}

TEST(CueTimelineTest,
     CueLayoutWithAnonymousWrapperAndExtremeDimensionsDoesNotCrashOrHang) {
  test::TaskEnvironment task_environment;

  Persistent<TestLocalFrameClient> frame_client =
      MakeGarbageCollected<TestLocalFrameClient>();
  auto page_holder = std::make_unique<DummyPageHolder>(gfx::Size(800, 600),
                                                       nullptr, frame_client);
  Document& document = page_holder->GetDocument();

  Persistent<HTMLVideoElement> video =
      MakeGarbageCollected<HTMLVideoElement>(document);
  document.body()->AppendChild(video);
  video->SetSrc(AtomicString("http://example.com/foo.mp4"));
  test::RunPendingTasks();
  ASSERT_NE(frame_client->client(), nullptr);
  frame_client->client()->ReadyStateChanged();

  video->setCurrentTime(0.5);
  frame_client->client()->TimeChanged();

  Persistent<TextTrack> track = video->addTextTrack(
      V8TextTrackKind(V8TextTrackKind::Enum::kSubtitles), AtomicString("t1"),
      AtomicString("en"), ASSERT_NO_EXCEPTION);
  track->SetModeEnum(TextTrackMode::kShowing);

  auto* cue = VTTCue::Create(document, 0.0, 2.0, "cue text");
  track->addCue(cue);
  ASSERT_TRUE(cue->IsActive());

  auto* container =
      Traversal<TextTrackContainer>::FirstWithin(*video->UserAgentShadowRoot());
  ASSERT_NE(container, nullptr);
  VTTCueBox* display_box =
      Traversal<VTTCueBox>::FirstWithin(*video->UserAgentShadowRoot());
  ASSERT_NE(display_box, nullptr);

  // Case 1: Multicol on TextTrackContainer + static VTTCueBox inserts an
  // anonymous LayoutMultiColumnFlowThread between container and display_box.
  container->SetInlineStyleProperty(CSSPropertyID::kColumnCount, 2,
                                    CSSPrimitiveValue::UnitType::kInteger);
  display_box->SetInlineStyleProperty(CSSPropertyID::kPosition,
                                      CSSValueID::kStatic);
  display_box->remove();
  container->AppendChild(display_box);
  ASSERT_FALSE(display_box->IsAdjusted());
  page_holder->GetFrameView().UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(display_box->IsAdjusted());

  // Case 2: Orthogonal writing-mode on VTTCueBox promotes VTTCueBackgroundBox
  // to inline-block in StyleAdjuster.
  container->RemoveInlineStyleProperty(CSSPropertyID::kColumnCount);
  display_box->SetInlineStyleProperty(CSSPropertyID::kPosition,
                                      CSSValueID::kAbsolute);
  display_box->SetInlineStyleProperty(CSSPropertyID::kWritingMode,
                                      CSSValueID::kVerticalRl);
  display_box->remove();
  container->AppendChild(display_box);
  ASSERT_FALSE(display_box->IsAdjusted());
  page_holder->GetFrameView().UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(display_box->IsAdjusted());

  // Case 3a: Saturated LayoutUnit::Max() height with cue box wider than the
  // video title area (default negative line, stepping upward then switching to
  // step_ > 0) must not spin forever in AdjustPositionWithSnapToLines().
  constexpr double kExceedsLayoutUnitMaxPx = 40000000.0;
  constexpr double kSaturatingStepFontSizePx = 100000.0;
  constexpr double kCueBoxMinWidthPx = 500.0;
  constexpr double kNarrowVideoWidthPx = 300.0;
  constexpr char kSaturatingPositiveLine[] = "line:1000000";

  display_box->SetInlineStyleProperty(CSSPropertyID::kWritingMode,
                                      CSSValueID::kHorizontalTb);
  display_box->SetInlineStyleProperty(CSSPropertyID::kFontSize,
                                      kSaturatingStepFontSizePx,
                                      CSSPrimitiveValue::UnitType::kPixels);
  display_box->SetInlineStyleProperty(CSSPropertyID::kMinWidth,
                                      kCueBoxMinWidthPx,
                                      CSSPrimitiveValue::UnitType::kPixels);
  video->SetInlineStyleProperty(CSSPropertyID::kHeight, kExceedsLayoutUnitMaxPx,
                                CSSPrimitiveValue::UnitType::kPixels);
  video->SetInlineStyleProperty(CSSPropertyID::kWidth, kNarrowVideoWidthPx,
                                CSSPrimitiveValue::UnitType::kPixels);
  display_box->remove();
  container->AppendChild(display_box);
  ASSERT_FALSE(display_box->IsAdjusted());
  page_holder->GetFrameView().UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(display_box->IsAdjusted());

  // Case 3b: Positive line at LayoutUnit::Max() height exercises step_ > 0
  // overflow/saturation on the initial pass.
  cue->ParseSettings(nullptr, kSaturatingPositiveLine);
  cue->setText("cue text positive line");
  display_box->SetInlineStyleProperty(CSSPropertyID::kFontSize,
                                      kSaturatingStepFontSizePx,
                                      CSSPrimitiveValue::UnitType::kPixels);
  display_box->SetInlineStyleProperty(CSSPropertyID::kMinWidth,
                                      kCueBoxMinWidthPx,
                                      CSSPrimitiveValue::UnitType::kPixels);
  display_box->remove();
  container->AppendChild(display_box);
  ASSERT_FALSE(display_box->IsAdjusted());
  page_holder->GetFrameView().UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(display_box->IsAdjusted());

  track->SetModeEnum(TextTrackMode::kDisabled);
  track.Clear();
  video.Clear();
  frame_client.Clear();
  page_holder.reset();
  test::RunPendingTasks();
}

}  // namespace blink
