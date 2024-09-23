// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/video_painter.h"

#include <memory>

#include "base/unguessable_token.h"
#include "cc/layers/layer.h"
#include "components/paint_preview/common/paint_preview_tracker.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/css/css_default_style_sheets.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/core/paint/paint_controller_paint_test.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/testing/empty_web_media_player.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

// Integration tests of video painting code (in CAP mode).

namespace blink {
namespace {

void ExtractLinks(const PaintRecord& record,
                  std::vector<std::pair<GURL, SkRect>>* links) {
  for (const cc::PaintOp& op : record) {
    if (op.GetType() == cc::PaintOpType::kAnnotate) {
      const auto& annotate_op = static_cast<const cc::AnnotateOp&>(op);
      links->push_back(std::make_pair(
          GURL(std::string(
              reinterpret_cast<const char*>(annotate_op.data->data()),
              annotate_op.data->size())),
          annotate_op.rect));
    } else if (op.GetType() == cc::PaintOpType::kDrawRecord) {
      const auto& record_op = static_cast<const cc::DrawRecordOp&>(op);
      ExtractLinks(record_op.record, links);
    }
  }
}

size_t CountImagesOfType(const PaintRecord& record, cc::ImageType image_type) {
  size_t count = 0;
  for (const cc::PaintOp& op : record) {
    if (op.GetType() == cc::PaintOpType::kDrawImage) {
      const auto& image_op = static_cast<const cc::DrawImageOp&>(op);
      if (image_op.image.GetImageHeaderMetadata()->image_type == image_type) {
        ++count;
      }
    } else if (op.GetType() == cc::PaintOpType::kDrawImageRect) {
      const auto& image_op = static_cast<const cc::DrawImageRectOp&>(op);
      if (image_op.image.GetImageHeaderMetadata()->image_type == image_type) {
        ++count;
      }
    } else if (op.GetType() == cc::PaintOpType::kDrawRecord) {
      const auto& record_op = static_cast<const cc::DrawRecordOp&>(op);
      count += CountImagesOfType(record_op.record, image_type);
    }
  }
  return count;
}

class StubWebMediaPlayer : public EmptyWebMediaPlayer {
 public:
  StubWebMediaPlayer(WebMediaPlayerClient* client) : client_(client) {}

  const cc::Layer* GetCcLayer() { return layer_.get(); }

  // WebMediaPlayer
  LoadTiming Load(LoadType,
                  const WebMediaPlayerSource&,
                  CorsMode,
                  bool is_cache_disabled) override {
    network_state_ = kNetworkStateLoaded;
    client_->NetworkStateChanged();
    ready_state_ = kReadyStateHaveEnoughData;
    client_->ReadyStateChanged();
    layer_ = cc::Layer::Create();
    layer_->SetIsDrawable(true);
    layer_->SetHitTestable(true);
    client_->SetCcLayer(layer_.get());
    return LoadTiming::kImmediate;
  }
  NetworkState GetNetworkState() const override { return network_state_; }
  ReadyState GetReadyState() const override { return ready_state_; }

 private:
  WebMediaPlayerClient* client_;
  scoped_refptr<cc::Layer> layer_;
  NetworkState network_state_ = kNetworkStateEmpty;
  ReadyState ready_state_ = kReadyStateHaveNothing;
};

class VideoStubLocalFrameClient : public EmptyLocalFrameClient {
 public:
  // LocalFrameClient
  std::unique_ptr<WebMediaPlayer> CreateWebMediaPlayer(
      HTMLMediaElement&,
      const WebMediaPlayerSource&,
      WebMediaPlayerClient* client) override {
    return std::make_unique<StubWebMediaPlayer>(client);
  }
};

class VideoPainterTest : public PaintControllerPaintTestBase {
 public:
  VideoPainterTest()
      : PaintControllerPaintTestBase(
            MakeGarbageCollected<VideoStubLocalFrameClient>()) {}

  void SetUp() override {
    EnableCompositing();
    PaintControllerPaintTestBase::SetUp();
    GetDocument().SetURL(KURL(NullURL(), "https://example.com/"));
  }

  bool HasLayerAttached(const cc::Layer& layer) {
    return GetChromeClient().HasLayer(layer);
  }
};

TEST_F(VideoPainterTest, VideoLayerAppearsInLayerTree) {
  // Insert a <video> and allow it to begin loading.
  SetBodyInnerHTML("<video width=300 height=300 src=test.ogv>");
  test::RunPendingTasks();

  // Force the page to paint.
  UpdateAllLifecyclePhasesForTest();

  // Fetch the layer associated with the <video>, and check that it was
  // correctly configured in the layer tree.
  auto* element = To<HTMLMediaElement>(GetDocument().body()->firstChild());
  StubWebMediaPlayer* player =
      static_cast<StubWebMediaPlayer*>(element->GetWebMediaPlayer());
  const cc::Layer* layer = player->GetCcLayer();
  ASSERT_TRUE(layer);
  EXPECT_TRUE(HasLayerAttached(*layer));
  // The layer bounds reflects the aspect ratio and object-fit of the video.
  EXPECT_EQ(gfx::Vector2dF(0, 75), layer->offset_to_transform_parent());
  EXPECT_EQ(gfx::Size(300, 150), layer->bounds());
}

class MockWebMediaPlayer : public StubWebMediaPlayer {
 public:
  explicit MockWebMediaPlayer(WebMediaPlayerClient* client)
      : StubWebMediaPlayer(client) {}
  MOCK_CONST_METHOD0(HasAvailableVideoFrame, bool());
  MOCK_CONST_METHOD0(HasReadableVideoFrame, bool());
  MOCK_METHOD3(Paint,
               void(cc::PaintCanvas*, const gfx::Rect&, cc::PaintFlags&));
};

class TestWebFrameClientImpl : public frame_test_helpers::TestWebFrameClient {
 public:
  std::unique_ptr<WebMediaPlayer> CreateMediaPlayer(
      const WebMediaPlayerSource&,
      WebMediaPlayerClient* client,
      blink::MediaInspectorContext*,
      WebMediaPlayerEncryptedMediaClient*,
      WebContentDecryptionModule*,
      const WebString& sink_id,
      const cc::LayerTreeSettings* settings,
      scoped_refptr<base::TaskRunner> compositor_worker_task_runner) override {
    auto player = std::make_unique<MockWebMediaPlayer>(client);
    EXPECT_CALL(*player, HasAvailableVideoFrame)
        .WillRepeatedly(testing::Return(false));
    return player;
  }
};

class VideoPaintPreviewTest : public testing::Test,
                              public PaintTestConfigurations {
 public:
  ~VideoPaintPreviewTest() {
    CSSDefaultStyleSheets::Instance().PrepareForLeakDetection();
    ThreadState::Current()->CollectAllGarbageForTesting();
  }

  void SetUp() override {
    web_view_helper_.Initialize(&web_frame_client_);

    WebLocalFrameImpl& frame_impl = GetLocalMainFrame();
    frame_impl.ViewImpl()->MainFrameViewWidget()->Resize(
        gfx::Size(bounds().size()));

    frame_test_helpers::LoadFrame(&GetLocalMainFrame(), "about:blank");
    GetDocument().View()->SetParentVisible(true);
    GetDocument().View()->SetSelfVisible(true);
  }

  void TearDown() override { web_view_helper_.Reset(); }

  void SetBodyInnerHTML(const std::string& content) {
    frame_test_helpers::LoadHTMLString(&GetLocalMainFrame(), content,
                                       KURL("http://test.com"));
  }

  Document& GetDocument() { return *GetFrame()->GetDocument(); }

  WebLocalFrameImpl& GetLocalMainFrame() {
    return *web_view_helper_.LocalMainFrame();
  }

  const gfx::Rect& bounds() { return bounds_; }

  bool PlayVideo() {
    LocalFrame::NotifyUserActivation(
        GetFrame(), mojom::UserActivationNotificationType::kTest);
    auto* element = To<HTMLMediaElement>(GetDocument().body()->firstChild());
    MockWebMediaPlayer* player =
        static_cast<MockWebMediaPlayer*>(element->GetWebMediaPlayer());
    EXPECT_CALL(*player, HasAvailableVideoFrame)
        .WillRepeatedly(testing::Return(true));
    auto play_result = element->Play();
    EXPECT_FALSE(play_result.has_value())
        << "DOM Exception when playing: "
        << static_cast<int>(play_result.value());
    return !play_result.has_value();
  }

  cc::PaintRecord CapturePaintPreview(bool skip_accelerated_content) {
    auto token = base::UnguessableToken::Create();
    const base::UnguessableToken embedding_token =
        base::UnguessableToken::Create();
    const bool is_main_frame = true;

    cc::PaintRecorder recorder;
    paint_preview::PaintPreviewTracker tracker(token, embedding_token,
                                               is_main_frame);
    cc::PaintCanvas* canvas = recorder.beginRecording();
    canvas->SetPaintPreviewTracker(&tracker);

    GetLocalMainFrame().CapturePaintPreview(
        bounds(), canvas,
        /*include_linked_destinations=*/true,
        /*skip_accelerated_content=*/skip_accelerated_content);
    return recorder.finishRecordingAsPicture();
  }

 private:
  test::TaskEnvironment task_environment_;

  LocalFrame* GetFrame() { return GetLocalMainFrame().GetFrame(); }

  TestWebFrameClientImpl web_frame_client_;

  // This must be destroyed before `web_frame_client_`; when the WebViewHelper
  // is deleted, it destroys child views that were created, but the list of
  // child views is maintained on `web_frame_client_`.
  frame_test_helpers::WebViewHelper web_view_helper_;
  gfx::Rect bounds_ = {0, 0, 640, 480};
};

INSTANTIATE_PAINT_TEST_SUITE_P(VideoPaintPreviewTest);

TEST_P(VideoPaintPreviewTest, URLIsRecordedWhenPaintingPreview) {
  // Insert a <video> and allow it to begin loading. The image was taken from
  // the RFC for the data URI scheme https://tools.ietf.org/html/rfc2397.
  SetBodyInnerHTML(R"HTML(
    <style>body{margin:0}</style>
    <video width=300 height=300 src="test.ogv" poster="data:image/gif;base64,R0
      lGODdhMAAwAPAAAAAAAP///ywAAAAAMAAwAAAC8IyPqcvt3wCcDkiLc7C0qwyGHhSWpjQu5yq
      mCYsapyuvUUlvONmOZtfzgFzByTB10QgxOR0TqBQejhRNzOfkVJ+5YiUqrXF5Y5lKh/DeuNcP
      5yLWGsEbtLiOSpa/TPg7JpJHxyendzWTBfX0cxOnKPjgBzi4diinWGdkF8kjdfnycQZXZeYGe
      jmJlZeGl9i2icVqaNVailT6F5iJ90m6mvuTS4OK05M0vDk0Q4XUtwvKOzrcd3iq9uisF81M1O
      IcR7lEewwcLp7tuNNkM3uNna3F2JQFo97Vriy/Xl4/f1cf5VWzXyym7PHhhx4dbgYKAAA7"
      controls>
  )HTML");
  test::RunPendingTasks();

  auto record = CapturePaintPreview(/*skip_accelerated_content=*/false);

  std::vector<std::pair<GURL, SkRect>> links;
  ExtractLinks(record, &links);
  ASSERT_EQ(1lu, links.size());
  EXPECT_EQ("http://test.com/", links[0].first);

  // The captured record will contain a poster image (GIF) even through the flag
  // is not set since the video is not playing.
  EXPECT_EQ(1U, CountImagesOfType(record, cc::ImageType::kGIF));
}

TEST_P(VideoPaintPreviewTest, PosterFlagToggleFrameCapture) {
  // Insert a <video> and allow it to begin loading. The image was taken from
  // the RFC for the data URI scheme https://tools.ietf.org/html/rfc2397.
  SetBodyInnerHTML(R"HTML(
    <style>body{margin:0}</style>
    <video width=300 height=300 src="test.ogv" poster="data:image/gif;base64,R0
      lGODdhMAAwAPAAAAAAAP///ywAAAAAMAAwAAAC8IyPqcvt3wCcDkiLc7C0qwyGHhSWpjQu5yq
      mCYsapyuvUUlvONmOZtfzgFzByTB10QgxOR0TqBQejhRNzOfkVJ+5YiUqrXF5Y5lKh/DeuNcP
      5yLWGsEbtLiOSpa/TPg7JpJHxyendzWTBfX0cxOnKPjgBzi4diinWGdkF8kjdfnycQZXZeYGe
      jmJlZeGl9i2icVqaNVailT6F5iJ90m6mvuTS4OK05M0vDk0Q4XUtwvKOzrcd3iq9uisF81M1O
      IcR7lEewwcLp7tuNNkM3uNna3F2JQFo97Vriy/Xl4/f1cf5VWzXyym7PHhhx4dbgYKAAA7"
      controls loop>
  )HTML");
  test::RunPendingTasks();

  // Play the video.
  ASSERT_TRUE(PlayVideo());

  // Capture using poster.
  auto* element = To<HTMLMediaElement>(GetDocument().body()->firstChild());
  MockWebMediaPlayer* player =
      static_cast<MockWebMediaPlayer*>(element->GetWebMediaPlayer());
  EXPECT_CALL(*player, Paint(testing::_, testing::_, testing::_)).Times(0);
  auto record = CapturePaintPreview(/*skip_accelerated_content=*/true);

  std::vector<std::pair<GURL, SkRect>> links;
  ExtractLinks(record, &links);
  ASSERT_EQ(1lu, links.size());
  EXPECT_EQ("http://test.com/", links[0].first);

  // The captured record will contain a poster image (GIF) even though the video
  // is playing.
  EXPECT_EQ(1U, CountImagesOfType(record, cc::ImageType::kGIF));

  // Capture using video frame.
  EXPECT_CALL(*player, Paint(testing::_, testing::_, testing::_));
  record = CapturePaintPreview(/*skip_accelerated_content=*/false);

  links.clear();
  ExtractLinks(record, &links);
  ASSERT_EQ(1lu, links.size());
  EXPECT_EQ("http://test.com/", links[0].first);

  // A video frame is recorded rather than the poster image (GIF) as the video
  // is "playing". Note: this is actually just empty since we are using a
  // MockWebMediaPlayer.
  EXPECT_EQ(0U, CountImagesOfType(record, cc::ImageType::kGIF));
}

TEST_P(VideoPaintPreviewTest, PosterFlagToggleNoPosterFrameCapture) {
  // Insert a <video> and allow it to begin loading. The image was taken from
  // the RFC for the data URI scheme https://tools.ietf.org/html/rfc2397.
  SetBodyInnerHTML(R"HTML(
    <style>body{margin:0}</style>
    <video width=300 height=300 src="test.ogv" controls loop>
  )HTML");
  test::RunPendingTasks();

  // Play the video.
  ASSERT_TRUE(PlayVideo());

  // Expect to not have to paint the video as empty will be painted without a
  // poster.
  auto* element = To<HTMLMediaElement>(GetDocument().body()->firstChild());
  MockWebMediaPlayer* player =
      static_cast<MockWebMediaPlayer*>(element->GetWebMediaPlayer());
  EXPECT_CALL(*player, Paint(testing::_, testing::_, testing::_)).Times(0);

  // Capture without poster.
  auto record = CapturePaintPreview(/*skip_accelerated_content=*/true);

  std::vector<std::pair<GURL, SkRect>> links;
  ExtractLinks(record, &links);
  ASSERT_EQ(1lu, links.size());
  EXPECT_EQ("http://test.com/", links[0].first);
}

}  // namespace
}  // namespace blink
