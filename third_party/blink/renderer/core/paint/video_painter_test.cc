// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/video_painter.h"

#include "base/unguessable_token.h"
#include "cc/layers/layer.h"
#include "components/paint_preview/common/paint_preview_tracker.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_size.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/core/paint/paint_controller_paint_test.h"
#include "third_party/blink/renderer/platform/testing/empty_web_media_player.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

// Integration tests of video painting code (in CAP mode).

namespace blink {
namespace {

void ExtractLinks(const cc::PaintOpBuffer* buffer,
                  std::vector<std::pair<GURL, SkRect>>* links) {
  for (cc::PaintOpBuffer::Iterator it(buffer); it; ++it) {
    if (it->GetType() == cc::PaintOpType::Annotate) {
      auto* annotate_op = static_cast<cc::AnnotateOp*>(*it);
      links->push_back(std::make_pair(
          GURL(std::string(
              reinterpret_cast<const char*>(annotate_op->data->data()),
              annotate_op->data->size())),
          annotate_op->rect));
    } else if (it->GetType() == cc::PaintOpType::DrawRecord) {
      auto* record_op = static_cast<cc::DrawRecordOp*>(*it);
      ExtractLinks(record_op->record.get(), links);
    }
  }
}

class StubWebMediaPlayer : public EmptyWebMediaPlayer {
 public:
  StubWebMediaPlayer(WebMediaPlayerClient* client) : client_(client) {}

  const cc::Layer* GetCcLayer() { return layer_.get(); }

  // WebMediaPlayer
  LoadTiming Load(LoadType, const WebMediaPlayerSource&, CorsMode) override {
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

class VideoPainterTestForCAP : private ScopedCompositeAfterPaintForTest,
                               public PaintControllerPaintTestBase {
 public:
  VideoPainterTestForCAP()
      : ScopedCompositeAfterPaintForTest(true),
        PaintControllerPaintTestBase(
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

TEST_F(VideoPainterTestForCAP, VideoLayerAppearsInLayerTree) {
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

class VideoPaintPreviewTest : public testing::Test,
                              public PaintTestConfigurations {
 public:
  void SetUp() override {
    web_view_helper_.Initialize();

    WebLocalFrameImpl& frame_impl = GetLocalMainFrame();
    frame_impl.ViewImpl()->MainFrameViewWidget()->Resize(
        gfx::Size(bounds().size()));

    frame_test_helpers::LoadFrame(&GetLocalMainFrame(), "about:blank");
    GetDocument().View()->SetParentVisible(true);
    GetDocument().View()->SetSelfVisible(true);
  }

  void SetBodyInnerHTML(const std::string& content) {
    frame_test_helpers::LoadHTMLString(&GetLocalMainFrame(), content,
                                       KURL("http://test.com"));
  }

  Document& GetDocument() { return *GetFrame()->GetDocument(); }

  WebLocalFrameImpl& GetLocalMainFrame() {
    return *web_view_helper_.LocalMainFrame();
  }

  const gfx::Rect& bounds() { return bounds_; }

 private:
  LocalFrame* GetFrame() { return GetLocalMainFrame().GetFrame(); }

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

  auto token = base::UnguessableToken::Create();
  const base::UnguessableToken embedding_token =
      base::UnguessableToken::Create();
  const bool is_main_frame = true;

  cc::PaintRecorder recorder;
  paint_preview::PaintPreviewTracker tracker(token, embedding_token,
                                             is_main_frame);
  cc::PaintCanvas* canvas =
      recorder.beginRecording(bounds().width(), bounds().height());
  canvas->SetPaintPreviewTracker(&tracker);

  GetLocalMainFrame().CapturePaintPreview(WebRect(bounds()), canvas,
                                          /*include_linked_destinations=*/true);
  auto record = recorder.finishRecordingAsPicture();
  std::vector<std::pair<GURL, SkRect>> links;
  ExtractLinks(record.get(), &links);

  ASSERT_EQ(1lu, links.size());
  EXPECT_EQ("http://test.com/", links[0].first);
  EXPECT_EQ(SkRect::MakeWH(300, 300), links[0].second);
}

}  // namespace
}  // namespace blink
