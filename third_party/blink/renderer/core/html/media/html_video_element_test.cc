// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/media/html_video_element.h"

#include "cc/layers/layer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/media/display_type.h"
#include "third_party/blink/public/platform/web_fullscreen_video_status.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/media/html_media_test_helper.h"
#include "third_party/blink/renderer/core/html/media/media_video_visibility_tracker.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_rect.h"
#include "third_party/blink/renderer/core/layout/layout_box_model_object.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/empty_web_media_player.h"
#include "third_party/blink/renderer/platform/testing/paint_test_configurations.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

using testing::_;
using testing::Return;

namespace blink {

namespace {

class HTMLVideoElementMockMediaPlayer : public EmptyWebMediaPlayer {
 public:
  MOCK_METHOD1(SetIsEffectivelyFullscreen, void(WebFullscreenVideoStatus));
  MOCK_METHOD1(OnDisplayTypeChanged, void(DisplayType));
  MOCK_CONST_METHOD0(HasAvailableVideoFrame, bool());
  MOCK_CONST_METHOD0(HasReadableVideoFrame, bool());
  MOCK_METHOD(void,
              RecordVideoOcclusionState,
              (std::string_view occlusion_state));
  MOCK_METHOD(gfx::Size, NaturalSize, (), (const));
};
}  // namespace

class HTMLVideoElementTest : public PaintTestConfigurations,
                             public RenderingTest {
 public:
  void SetUp() override {
    auto mock_media_player =
        std::make_unique<HTMLVideoElementMockMediaPlayer>();
    media_player_ = mock_media_player.get();
    SetupPageWithClients(nullptr,
                         MakeGarbageCollected<test::MediaStubLocalFrameClient>(
                             std::move(mock_media_player)),
                         nullptr);
    video_ = MakeGarbageCollected<HTMLVideoElement>(GetDocument());
    GetDocument().body()->appendChild(video_);
  }

  void SetFakeCcLayer(cc::Layer* layer) { video_->SetCcLayer(layer); }

  HTMLVideoElement* video() { return video_.Get(); }

  HTMLVideoElementMockMediaPlayer* MockWebMediaPlayer() {
    return media_player_;
  }

  HTMLVideoElementMockMediaPlayer* MockMediaPlayer() { return media_player_; }

  MediaVideoVisibilityTracker* VideoVisibilityTracker() {
    return video_ ? video_->visibility_tracker_for_tests() : nullptr;
  }

  MediaVideoVisibilityTracker::TrackerAttachedToDocument
  VideoVisibilityTrackerAttachedToDocument() const {
    DCHECK(video_);
    DCHECK(video_->visibility_tracker_for_tests());
    return video_->visibility_tracker_for_tests()
        ->tracker_attached_to_document_;
  }

  void RequestVisibility(HTMLMediaElement::RequestVisibilityCallback
                             request_visibility_callback) const {
    DCHECK(video_);
    video_->RequestVisibility(std::move(request_visibility_callback));
  }

  const MediaVideoVisibilityTracker::OcclusionState& TrackerOcclusionState() {
    DCHECK(video_);
    DCHECK(video_->visibility_tracker_for_tests());
    return video_->visibility_tracker_for_tests()->occlusion_state_;
  }

 private:
  Persistent<HTMLVideoElement> video_;

  // Owned by HTMLVideoElementFrameClient.
  HTMLVideoElementMockMediaPlayer* media_player_;
};
INSTANTIATE_PAINT_TEST_SUITE_P(HTMLVideoElementTest);

TEST_P(HTMLVideoElementTest, PictureInPictureInterstitialAndTextContainer) {
  scoped_refptr<cc::Layer> layer = cc::Layer::Create();
  SetFakeCcLayer(layer.get());

  video()->SetBooleanAttribute(html_names::kControlsAttr, true);
  video()->SetSrc(AtomicString("http://example.com/foo.mp4"));
  test::RunPendingTasks();

  // Simulate the text track being displayed.
  video()->UpdateTextTrackDisplay();
  video()->UpdateTextTrackDisplay();

  // Simulate entering Picture-in-Picture.
  EXPECT_CALL(*MockWebMediaPlayer(),
              OnDisplayTypeChanged(DisplayType::kInline));
  video()->OnEnteredPictureInPicture();

  // Simulate that text track are displayed again.
  video()->UpdateTextTrackDisplay();

  EXPECT_EQ(3u, video()->EnsureUserAgentShadowRoot().CountChildren());
  EXPECT_CALL(*MockWebMediaPlayer(),
              OnDisplayTypeChanged(DisplayType::kInline));
  // Reset cc::layer to avoid crashes depending on timing.
  SetFakeCcLayer(nullptr);
}

TEST_P(HTMLVideoElementTest, PictureInPictureInterstitial_Reattach) {
  scoped_refptr<cc::Layer> layer = cc::Layer::Create();
  SetFakeCcLayer(layer.get());

  video()->SetBooleanAttribute(html_names::kControlsAttr, true);
  video()->SetSrc(AtomicString("http://example.com/foo.mp4"));
  test::RunPendingTasks();

  EXPECT_CALL(*MockWebMediaPlayer(),
              OnDisplayTypeChanged(DisplayType::kInline));
  EXPECT_CALL(*MockWebMediaPlayer(), HasAvailableVideoFrame())
      .WillRepeatedly(testing::Return(true));

  // Simulate entering Picture-in-Picture.
  video()->OnEnteredPictureInPicture();

  EXPECT_CALL(*MockWebMediaPlayer(), OnDisplayTypeChanged(DisplayType::kInline))
      .Times(3);

  // Try detaching and reattaching. This should not crash.
  GetDocument().body()->removeChild(video());
  GetDocument().body()->appendChild(video());
  GetDocument().body()->removeChild(video());
}

TEST_P(HTMLVideoElementTest, EffectivelyFullscreen_DisplayType) {
  video()->SetSrc(AtomicString("http://example.com/foo.mp4"));
  test::RunPendingTasks();
  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(DisplayType::kInline, video()->GetDisplayType());

  // Vector of data to use for tests. First value is to be set when calling
  // SetIsEffectivelyFullscreen(). The second one is the expected DisplayType.
  // This is testing all possible values of WebFullscreenVideoStatus and then
  // sets the value back to a value that should put the DisplayType back to
  // inline.
  Vector<std::pair<WebFullscreenVideoStatus, DisplayType>> tests = {
      {WebFullscreenVideoStatus::kNotEffectivelyFullscreen,
       DisplayType::kInline},
      {WebFullscreenVideoStatus::kFullscreenAndPictureInPictureEnabled,
       DisplayType::kFullscreen},
      {WebFullscreenVideoStatus::kFullscreenAndPictureInPictureDisabled,
       DisplayType::kFullscreen},
      {WebFullscreenVideoStatus::kNotEffectivelyFullscreen,
       DisplayType::kInline},
  };

  for (const auto& test : tests) {
    EXPECT_CALL(*MockWebMediaPlayer(), SetIsEffectivelyFullscreen(test.first));
    EXPECT_CALL(*MockWebMediaPlayer(), OnDisplayTypeChanged(test.second));
    video()->SetIsEffectivelyFullscreen(test.first);

    EXPECT_EQ(test.second, video()->GetDisplayType());
    testing::Mock::VerifyAndClearExpectations(MockWebMediaPlayer());
  }
}

TEST_P(HTMLVideoElementTest, ChangeLayerNeedsCompositingUpdate) {
  video()->SetSrc(AtomicString("http://example.com/foo.mp4"));
  test::RunPendingTasks();
  UpdateAllLifecyclePhasesForTest();

  auto layer1 = cc::Layer::Create();
  SetFakeCcLayer(layer1.get());
  auto* painting_layer =
      To<LayoutBoxModelObject>(video()->GetLayoutObject())->PaintingLayer();
  EXPECT_TRUE(painting_layer->SelfNeedsRepaint());
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(painting_layer->SelfNeedsRepaint());

  // Change to another cc layer.
  auto layer2 = cc::Layer::Create();
  SetFakeCcLayer(layer2.get());
  EXPECT_TRUE(painting_layer->SelfNeedsRepaint());
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(painting_layer->SelfNeedsRepaint());

  // Remove cc layer.
  SetFakeCcLayer(nullptr);
  EXPECT_TRUE(painting_layer->SelfNeedsRepaint());
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(painting_layer->SelfNeedsRepaint());
}

TEST_P(HTMLVideoElementTest, HasAvailableVideoFrameChecksWMP) {
  video()->SetSrc(AtomicString("http://example.com/foo.mp4"));
  test::RunPendingTasks();
  UpdateAllLifecyclePhasesForTest();

  EXPECT_CALL(*MockWebMediaPlayer(), HasAvailableVideoFrame())
      .WillOnce(testing::Return(false))
      .WillOnce(testing::Return(true));
  EXPECT_FALSE(video()->HasAvailableVideoFrame());
  EXPECT_TRUE(video()->HasAvailableVideoFrame());
}

TEST_P(HTMLVideoElementTest, AutoPIPExitPIPTest) {
  video()->SetSrc(AtomicString("http://example.com/foo.mp4"));
  test::RunPendingTasks();

  // Set in auto PIP.
  video()->SetPersistentState(true);

  // Shouldn't get to PictureInPictureController::ExitPictureInPicture
  // and fail the DCHECK.
  EXPECT_NO_FATAL_FAILURE(video()->DidEnterFullscreen());
  test::RunPendingTasks();
}

// TODO(1190335): Remove this once we no longer support "default poster image"
// Blink embedders (such as Webview) can set the default poster image for a
// video using `blink::Settings`. In some cases we still need to distinguish
// between a "real" poster image and the default poster image.
TEST_P(HTMLVideoElementTest, DefaultPosterImage) {
  String const kDefaultPosterImage = "http://www.example.com/foo.jpg";

  // Override the default poster image
  GetDocument().GetSettings()->SetDefaultVideoPosterURL(kDefaultPosterImage);

  // Need to create a new video element, since
  // `HTMLVideoElement::default_poster_url_` is set upon construction.
  auto* video = MakeGarbageCollected<HTMLVideoElement>(GetDocument());
  GetDocument().body()->appendChild(video);

  // Assert that video element (without an explicitly set poster image url) has
  // the same poster image URL as what we just set.
  EXPECT_TRUE(video->IsDefaultPosterImageURL());
  EXPECT_EQ(kDefaultPosterImage, video->PosterImageURL());

  // Set the poster image of the video to something
  video->setAttribute(html_names::kPosterAttr,
                      AtomicString("http://www.example.com/bar.jpg"));
  EXPECT_FALSE(video->IsDefaultPosterImageURL());
  EXPECT_NE(kDefaultPosterImage, video->PosterImageURL());
}

TEST_P(HTMLVideoElementTest,
       RecordVideoOcclusionStateCalledWhenVisibilityIsRequested) {
  video()->SetSrc(AtomicString("http://example.com/foo.mp4"));
  test::RunPendingTasks();
  UpdateAllLifecyclePhasesForTest();
  ASSERT_EQ(VideoVisibilityTracker(), nullptr);

  video()->Play();
  EXPECT_TRUE(video()->GetWebMediaPlayer());
  ASSERT_NE(VideoVisibilityTracker(), nullptr);
  EXPECT_NE(VideoVisibilityTrackerAttachedToDocument(), nullptr);

  // Request visibility and verify `RecordVideoOcclusionState` is called.
  const std::string expected_occlusion_state =
      "has sufficiently visible video: {True}, occluded area: {0.00}, "
      "occluding rects: {None}, intersection rect: {x: 8, y: 8, width: 300, "
      "height: 150}, video element rect: {x: 8, y: 8, width: 300, height: "
      "150}, visibility threshold: {10000}";
  EXPECT_CALL((*MockMediaPlayer()),
              RecordVideoOcclusionState(expected_occlusion_state));
  RequestVisibility(base::DoNothing());

  // Verify that `RecordVideoOcclusionState` is also called when the document is
  // not in the `DocumentUpdateReason::kPaintClean` lifecycle state.
  //
  // Set the lifecycle state to a value < `DocumentUpdateReason::kPaintClean`.
  // This will cause the tracker to used the cached
  // `meets_visibility_threshold_` value when we request visibility.
  GetDocument().View()->UpdateLifecycleToLayoutClean(
      DocumentUpdateReason::kTest);
  EXPECT_CALL((*MockMediaPlayer()),
              RecordVideoOcclusionState(expected_occlusion_state));
  RequestVisibility(base::DoNothing());
}

TEST_P(HTMLVideoElementTest,
       RecordVideoOcclusionStateNotCalledIfVisibilityIsNotRequested) {
  video()->SetSrc(AtomicString("http://example.com/foo.mp4"));
  test::RunPendingTasks();
  UpdateAllLifecyclePhasesForTest();
  ASSERT_EQ(VideoVisibilityTracker(), nullptr);

  video()->Play();

  // Update all lifecycle phases and verify `RecordVideoOcclusionState` is not
  // called.
  EXPECT_CALL((*MockMediaPlayer()), RecordVideoOcclusionState(_)).Times(0);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(video()->GetWebMediaPlayer());
  ASSERT_NE(VideoVisibilityTracker(), nullptr);
  EXPECT_NE(VideoVisibilityTrackerAttachedToDocument(), nullptr);
}

TEST_P(HTMLVideoElementTest,
       RecordVideoOcclusionStateCalledWhenTrackerNotAttached) {
  video()->SetSrc(AtomicString("http://example.com/foo.mp4"));
  test::RunPendingTasks();
  UpdateAllLifecyclePhasesForTest();
  ASSERT_EQ(VideoVisibilityTracker(), nullptr);

  video()->Play();
  EXPECT_TRUE(video()->GetWebMediaPlayer());
  ASSERT_NE(VideoVisibilityTracker(), nullptr);
  EXPECT_NE(VideoVisibilityTrackerAttachedToDocument(), nullptr);

  // Remove video, and verify that the visibility tracker has been detached.
  NonThrowableExceptionState should_not_throw;
  video()->remove(should_not_throw);
  test::RunPendingTasks();
  EXPECT_EQ(VideoVisibilityTrackerAttachedToDocument(), nullptr);

  // Request visibility and verify `RecordVideoOcclusionState` is called since
  // the tracker has a request visibility callback.
  const std::string expected_occlusion_state =
      "has sufficiently visible video: {False}, occluded area: {0.00}, "
      "occluding rects: {None}, intersection rect: {None}, video element rect: "
      "{None}, visibility threshold: {10000}";
  EXPECT_CALL((*MockMediaPlayer()),
              RecordVideoOcclusionState(expected_occlusion_state));
  RequestVisibility(base::DoNothing());
}

TEST_P(HTMLVideoElementTest, VideoVisibilityTrackerVideoElementRectDimensions) {
  const auto& expected_natural_size = gfx::Size(1920, 1080);
  EXPECT_CALL(*MockMediaPlayer(), NaturalSize())
      .WillRepeatedly(Return(expected_natural_size));

  video()->SetSrc(AtomicString("http://example.com/foo.mp4"));
  video()->setAttribute(html_names::kStyleAttr,
                        AtomicString("width: 300px; height: 300px"));
  test::RunPendingTasks();
  UpdateAllLifecyclePhasesForTest();
  ASSERT_EQ(VideoVisibilityTracker(), nullptr);

  video()->Play();
  EXPECT_TRUE(video()->GetWebMediaPlayer());
  ASSERT_EQ(expected_natural_size,
            gfx::Size(video()->videoWidth(), video()->videoHeight()));
  ASSERT_NE(VideoVisibilityTracker(), nullptr);
  EXPECT_NE(VideoVisibilityTrackerAttachedToDocument(), nullptr);

  RequestVisibility(base::DoNothing());

  // Verify that the visibility tracker is using the content rect for the
  // `video_element_rect`.
  const auto occlusion_state = TrackerOcclusionState();
  LayoutBox* box = To<LayoutBox>(video()->GetLayoutObject());
  EXPECT_EQ(box->AbsoluteBoundingBoxRect().width(),
            occlusion_state.video_element_rect.Width());
  EXPECT_GT(box->AbsoluteBoundingBoxRect().height(),
            occlusion_state.video_element_rect.Height());

  // Verify that the `video_element_rect` intersects the `HTMLVideoElement`
  // bounds.
  const auto intersection = Intersection(VisualRectInDocument(*box),
                                         occlusion_state.video_element_rect);
  EXPECT_EQ(occlusion_state.video_element_rect, intersection);
}

}  // namespace blink
