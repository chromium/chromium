// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/video_painter.h"

#include "cc/layers/layer.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_size.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/core/paint/paint_controller_paint_test.h"
#include "third_party/blink/renderer/platform/testing/empty_web_media_player.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

// Integration tests of video painting code (in CAP mode).

namespace blink {
namespace {

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
  HTMLMediaElement* element =
      ToHTMLMediaElement(GetDocument().body()->firstChild());
  StubWebMediaPlayer* player =
      static_cast<StubWebMediaPlayer*>(element->GetWebMediaPlayer());
  const cc::Layer* layer = player->GetCcLayer();
  ASSERT_TRUE(layer);
  EXPECT_TRUE(HasLayerAttached(*layer));
  // The layer bounds reflects the aspectn ratio and object-fit of the video.
  EXPECT_EQ(gfx::Vector2dF(8, 83), layer->offset_to_transform_parent());
  EXPECT_EQ(gfx::Size(300, 150), layer->bounds());
}

}  // namespace
}  // namespace blink
