// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/image_replacement/image_replacement.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "cc/paint/paint_op.h"
#include "cc/paint/paint_op_buffer_iterator.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/image_replacement/image_replacement.mojom-blink.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/public/web/web_script_source.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/web_remote_frame_impl.h"
#include "third_party/blink/renderer/core/html/html_iframe_element.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/image_replacement/document_image_replacements.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/layout/hit_test_location.h"
#include "third_party/blink/renderer/core/layout/hit_test_result.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/layout_image.h"
#include "third_party/blink/renderer/core/layout/layout_image_replacement.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/skia/include/codec/SkCodec.h"
#include "third_party/skia/include/codec/SkWebpDecoder.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkImage.h"

namespace blink {

namespace {
size_t CountDrawImageRectOps(const cc::PaintRecord& record) {
  size_t count = 0;
  for (const cc::PaintOp& op : record) {
    if (op.GetType() == cc::PaintOpType::kDrawImageRect) {
      count++;
    } else if (op.GetType() == cc::PaintOpType::kDrawRecord) {
      const auto& record_op = static_cast<const cc::DrawRecordOp&>(op);
      count += CountDrawImageRectOps(record_op.record);
    }
  }
  return count;
}

// Returns a 1x1 transparent GIF image as a vector of bytes.
Vector<char> GetTransparentGifBytes() {
  static const unsigned char kGifBytes[] = {
      0x47, 0x49, 0x46, 0x38, 0x39, 0x61, 0x01, 0x00, 0x01, 0x00, 0x80,
      0x00, 0x00, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x21, 0xf9, 0x04,
      0x01, 0x00, 0x00, 0x00, 0x00, 0x2c, 0x00, 0x00, 0x00, 0x00, 0x01,
      0x00, 0x01, 0x00, 0x00, 0x02, 0x02, 0x44, 0x01, 0x00, 0x3b};
  Vector<char> gif_data;
  gif_data.append_range(kGifBytes);
  return gif_data;
}

class ClickEventListener : public NativeEventListener {
 public:
  void Invoke(ExecutionContext*, Event* event) override { clicked_ = true; }
  bool clicked() const { return clicked_; }

 private:
  bool clicked_ = false;
};

class ErrorEventListener : public NativeEventListener {
 public:
  explicit ErrorEventListener(base::OnceClosure quit_closure)
      : quit_closure_(std::move(quit_closure)) {}

  void Invoke(ExecutionContext*, Event* event) override {
    if (quit_closure_) {
      std::move(quit_closure_).Run();
    }
  }

 private:
  base::OnceClosure quit_closure_;
};

}  // namespace

class MockImageReplacementHost : public mojom::blink::ImageReplacementHost {
 public:
  void ReplacementFrameAttached(
      const blink::LocalFrameToken& frame_token,
      mojom::blink::ReplacementDataPtr replacement_data) override {
    EXPECT_TRUE(replacement_data);
    EXPECT_TRUE(replacement_data->original_image);
    if (replacement_data->original_image) {
      EXPECT_GT(replacement_data->original_image->webp_bytes.size(), 0u);

      // Decode the image.
      auto data = SkData::MakeWithCopy(
          replacement_data->original_image->webp_bytes.data(),
          replacement_data->original_image->webp_bytes.size());
      auto codec = SkWebpDecoder::Decode(data, nullptr);
      EXPECT_TRUE(codec);
      if (codec) {
        SkImageInfo info = codec->getInfo();
        decoded_bitmap_.allocPixels(info);
        EXPECT_EQ(codec->getPixels(info, decoded_bitmap_.getPixels(),
                                   decoded_bitmap_.rowBytes()),
                  SkCodec::kSuccess);
      }
    }
    frame_token_ = frame_token;
    original_image_ = std::move(replacement_data->original_image);
    tracked_element_id_ = replacement_data->tracked_element_id;
    replacement_data_ = std::move(replacement_data);
  }

  const std::optional<blink::LocalFrameToken>& frame_token() const {
    return frame_token_;
  }

  const std::optional<base::Token>& tracked_element_id() const {
    return tracked_element_id_;
  }

  const mojom::blink::ReplacementDataPtr& replacement_data() const {
    return replacement_data_;
  }

  const SkBitmap& decoded_bitmap() const { return decoded_bitmap_; }

  mojo::Receiver<mojom::blink::ImageReplacementHost>& receiver() {
    return receiver_;
  }

 private:
  mojo::Receiver<mojom::blink::ImageReplacementHost> receiver_{this};
  std::optional<blink::LocalFrameToken> frame_token_;
  mojom::blink::ImageDataPtr original_image_;
  std::optional<base::Token> tracked_element_id_;
  mojom::blink::ReplacementDataPtr replacement_data_;
  SkBitmap decoded_bitmap_;
};

class ImageReplacementSimTest : public SimTest {};

TEST_F(ImageReplacementSimTest, ShadowTreeClearedOnLayoutDispositionChange) {
  auto* image = MakeGarbageCollected<HTMLImageElement>(GetDocument());
  EXPECT_FALSE(image->UserAgentShadowRoot());

  // 1) Switch to fallback content.
  image->EnsureFallbackForGeneratedContent();
  ASSERT_TRUE(image->UserAgentShadowRoot());
  EXPECT_TRUE(image->UserAgentShadowRoot()->HasChildren());

  // 2) Switch back to primary content.
  // This should clear the shadow tree children.
  image->EnsurePrimaryContent();
  ASSERT_TRUE(image->UserAgentShadowRoot());
  EXPECT_FALSE(image->UserAgentShadowRoot()->HasChildren());

  // 3) Switch to image replacement.
  // This should create a new shadow tree for image replacement which contains
  // exactly one child (an iframe).
  image->StartImageReplacement();
  ASSERT_TRUE(image->UserAgentShadowRoot());
  ASSERT_EQ(1u, image->UserAgentShadowRoot()->CountChildren());
  EXPECT_TRUE(
      IsA<HTMLIFrameElement>(image->UserAgentShadowRoot()->firstChild()));
}

TEST_F(ImageReplacementSimTest, ImageReplacementLifecycle) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kImageReplacement);
  SimRequest main_resource("https://example.com/index.html", "text/html");
  LoadURL("https://example.com/index.html");
  main_resource.Complete(R"(
    <img src="data:image/gif;base64,R0lGODlhAQABAIAAAP///////yH5BAEKAAEALAAAAAABAAEAAAICTAEAOw=="
         id="target"></img>
  )");

  Compositor().BeginFrame();
  test::RunPendingTasks();

  HTMLImageElement* img = To<HTMLImageElement>(
      GetDocument().getElementById(AtomicString("target")));
  ASSERT_TRUE(img);
  ASSERT_TRUE(img->complete());
  ASSERT_TRUE(img->GetLayoutObject());
  EXPECT_TRUE(img->GetLayoutObject()->IsLayoutImage());

  auto result = ImageReplacement::CreateAndBindReceiver(*img);
  ASSERT_TRUE(result.has_value());

  mojo::Remote<mojom::blink::ImageReplacement> replacement_remote(
      std::move(result.value()));

  MockImageReplacementHost mock_host;
  replacement_remote->StartReplacement(
      mock_host.receiver().BindNewPipeAndPassRemote(), std::nullopt);
  test::RunPendingTasks();

  // Verify replacement state
  EXPECT_TRUE(img->GetLayoutObject()->IsLayoutImageReplacement());
  ASSERT_TRUE(img->UserAgentShadowRoot());

  auto* iframe =
      DynamicTo<HTMLIFrameElement>(img->UserAgentShadowRoot()->firstChild());
  ASSERT_TRUE(iframe);
  ASSERT_TRUE(iframe->InlineStyle());
  EXPECT_EQ(
      iframe->InlineStyle()->GetPropertyValue(CSSPropertyID::kPointerEvents),
      "none");
  ASSERT_TRUE(iframe->ContentFrame());
  EXPECT_TRUE(mock_host.frame_token().has_value());
  EXPECT_EQ(iframe->ContentFrame()->GetFrameToken(), mock_host.frame_token());

  // Attempt to create another replacement should fail
  auto result2 = ImageReplacement::CreateAndBindReceiver(*img);
  EXPECT_FALSE(result2.has_value());

  // Verify reset on disconnect
  replacement_remote.reset();
  test::RunPendingTasks();
  Compositor().BeginFrame();

  EXPECT_TRUE(img->GetLayoutObject()->IsLayoutImage());
  EXPECT_FALSE(img->UserAgentShadowRoot()->HasChildren());
  EXPECT_FALSE(DocumentImageReplacements::FromIfExists(GetDocument())
                   ->GetImageReplacement(img));
}

TEST_F(ImageReplacementSimTest, OriginalImageIsEncodedCorrectly) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kImageReplacement);
  SimRequest main_resource("https://example.com/index.html", "text/html");
  LoadURL("https://example.com/index.html");

  // A 1x1 solid green PNG.
  constexpr char kGreenPngDataUrl[] =
      "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADU"
      "lEQVR42mNk+M/wHwAEBgIApD5fRAAAAABJRU5ErkJggg==";
  main_resource.Complete("<img src=\"" + String(kGreenPngDataUrl) +
                         "\" id=\"target\">");

  Compositor().BeginFrame();
  test::RunPendingTasks();

  HTMLImageElement* img = To<HTMLImageElement>(
      GetDocument().getElementById(AtomicString("target")));
  ASSERT_TRUE(img);
  ASSERT_TRUE(img->complete());
  ASSERT_TRUE(img->GetLayoutObject());

  auto result = ImageReplacement::CreateAndBindReceiver(*img);
  ASSERT_TRUE(result.has_value());

  mojo::Remote<mojom::blink::ImageReplacement> replacement_remote(
      std::move(result.value()));

  MockImageReplacementHost mock_host;
  replacement_remote->StartReplacement(
      mock_host.receiver().BindNewPipeAndPassRemote(), std::nullopt);
  test::RunPendingTasks();

  // Verify the decoded image in the mock host.
  const SkBitmap& bitmap = mock_host.decoded_bitmap();
  ASSERT_FALSE(bitmap.isNull());
  EXPECT_EQ(bitmap.width(), 1);
  EXPECT_EQ(bitmap.height(), 1);

  // Verify that it is close to green (0, 255, 0).
  // Lossy compression might alter the color slightly, so we check for a range.
  SkColor color = bitmap.getColor(0, 0);
  EXPECT_NEAR(SkColorGetR(color), 0, 10);
  EXPECT_NEAR(SkColorGetG(color), 255, 10);
  EXPECT_NEAR(SkColorGetB(color), 0, 10);
}

TEST_F(ImageReplacementSimTest, OriginalImageRespectsOrientation) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kImageReplacement);
  SimRequest main_resource("https://example.com/index.html", "text/html");
  LoadURL("https://example.com/index.html");

  // Image with orientation 8 (Left-Bottom).
  // The base64 string was generated from
  // third_party/blink/web_tests/external/wpt/css/css-images/image-orientation/support/exif-orientation-8-llo.jpg
  constexpr char kExifJpegDataUrl[] =
      "data:image/jpeg;base64,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";
  main_resource.Complete("<img src=\"" + String(kExifJpegDataUrl) +
                         "\" id=\"target\">");

  Compositor().BeginFrame();
  test::RunPendingTasks();

  HTMLImageElement* img = To<HTMLImageElement>(
      GetDocument().getElementById(AtomicString("target")));
  ASSERT_TRUE(img);
  ASSERT_TRUE(img->complete());
  ASSERT_TRUE(img->GetLayoutObject());

  auto result = ImageReplacement::CreateAndBindReceiver(*img);
  if (!result.has_value()) {
    FAIL() << "CreateAndBindReceiver failed: " << result.error();
  }

  mojo::Remote<mojom::blink::ImageReplacement> replacement_remote(
      std::move(result.value()));

  MockImageReplacementHost mock_host;
  replacement_remote->StartReplacement(
      mock_host.receiver().BindNewPipeAndPassRemote(), std::nullopt);
  test::RunPendingTasks();

  // Verify the decoded image in the mock host.
  const SkBitmap& bitmap = mock_host.decoded_bitmap();
  ASSERT_FALSE(bitmap.isNull());

  EXPECT_EQ(bitmap.width(), 50);
  EXPECT_EQ(bitmap.height(), 100);
}

TEST_F(ImageReplacementSimTest, ImageReplacementSendsTrackedElementId) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kImageReplacement);
  SimRequest main_resource("https://example.com/index.html", "text/html");
  LoadURL("https://example.com/index.html");
  main_resource.Complete(R"(
    <style>
      body { margin: 0; }
      #target { position: absolute; left: 50px; top: 100px; width: 100px; height: 100px; }
    </style>
    <img src="data:image/gif;base64,R0lGODlhAQABAIAAAP///////yH5BAEKAAEALAAAAAABAAEAAAICTAEAOw=="
         id="target"></img>
  )");

  Compositor().BeginFrame();
  test::RunPendingTasks();

  HTMLImageElement* img = To<HTMLImageElement>(
      GetDocument().getElementById(AtomicString("target")));
  ASSERT_TRUE(img);
  ASSERT_TRUE(img->GetLayoutObject());

  auto result = ImageReplacement::CreateAndBindReceiver(*img);
  ASSERT_TRUE(result.has_value());

  mojo::Remote<mojom::blink::ImageReplacement> replacement_remote(
      std::move(result.value()));

  MockImageReplacementHost mock_host;
  // Start replacement with layout tracking enabled (using kIndigoToolbar as
  // feature ID).
  replacement_remote->StartReplacement(
      mock_host.receiver().BindNewPipeAndPassRemote(), 0);
  test::RunPendingTasks();

  EXPECT_TRUE(mock_host.tracked_element_id().has_value());
  EXPECT_NE(mock_host.tracked_element_id().value(), base::Token());
}

TEST_F(ImageReplacementSimTest, ImageReplacementDoesNotSendTrackedElementId) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kImageReplacement);
  SimRequest main_resource("https://example.com/index.html", "text/html");
  LoadURL("https://example.com/index.html");
  main_resource.Complete(R"(
    <style>
      body { margin: 0; }
      #target { position: absolute; left: 50px; top: 100px; width: 100px; height: 100px; }
    </style>
    <img src="data:image/gif;base64,R0lGODlhAQABAIAAAP///////yH5BAEKAAEALAAAAAABAAEAAAICTAEAOw=="
         id="target"></img>
  )");

  Compositor().BeginFrame();
  test::RunPendingTasks();

  HTMLImageElement* img = To<HTMLImageElement>(
      GetDocument().getElementById(AtomicString("target")));
  ASSERT_TRUE(img);
  ASSERT_TRUE(img->GetLayoutObject());

  auto result = ImageReplacement::CreateAndBindReceiver(*img);
  ASSERT_TRUE(result.has_value());

  mojo::Remote<mojom::blink::ImageReplacement> replacement_remote(
      std::move(result.value()));

  MockImageReplacementHost mock_host;
  // Start replacement with layout tracking disabled.
  replacement_remote->StartReplacement(
      mock_host.receiver().BindNewPipeAndPassRemote(), std::nullopt);
  test::RunPendingTasks();

  EXPECT_FALSE(mock_host.tracked_element_id().has_value());
}

TEST_F(ImageReplacementSimTest, ClickFiresOnImageAfterReplacement) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kImageReplacement);

  SimRequest main_resource("https://example.com/index.html", "text/html");
  LoadURL("https://example.com/index.html");
  main_resource.Complete(R"(
    <style> img { width: 100px; height: 100px; } </style>
    <img src="data:image/gif;base64,R0lGODlhAQABAIAAAP///////yH5BAEKAAEALAAAAAABAAEAAAICTAEAOw=="
         id="target"></img>
  )");

  Compositor().BeginFrame();
  test::RunPendingTasks();

  HTMLImageElement* img = To<HTMLImageElement>(
      GetDocument().getElementById(AtomicString("target")));
  ASSERT_TRUE(img);
  gfx::Point center =
      img->GetLayoutObject()->AbsoluteBoundingBoxRect().CenterPoint();

  auto result = ImageReplacement::CreateAndBindReceiver(*img);
  ASSERT_TRUE(result.has_value());
  mojo::Remote<mojom::blink::ImageReplacement> replacement_remote(
      std::move(result.value()));
  MockImageReplacementHost mock_host;
  replacement_remote->StartReplacement(
      mock_host.receiver().BindNewPipeAndPassRemote(), std::nullopt);
  test::RunPendingTasks();
  Compositor().BeginFrame();

  auto* iframe =
      DynamicTo<HTMLIFrameElement>(img->UserAgentShadowRoot()->firstChild());
  ASSERT_TRUE(iframe);

  HitTestLocation location(center);
  HitTestResult hit_result =
      GetDocument().GetFrame()->GetEventHandler().HitTestResultAtLocation(
          location);
  EXPECT_EQ(hit_result.InnerNode(), img);

  auto* listener = MakeGarbageCollected<ClickEventListener>();
  img->addEventListener(AtomicString("click"), listener);

  // Simulate click.
  GetDocument().GetFrame()->GetEventHandler().HandleMousePressEvent(
      WebMouseEvent(WebMouseEvent::Type::kMouseDown, gfx::PointF(center),
                    gfx::PointF(center), WebPointerProperties::Button::kLeft, 1,
                    WebInputEvent::Modifiers::kLeftButtonDown,
                    WebInputEvent::GetStaticTimeStampForTests()));
  GetDocument().GetFrame()->GetEventHandler().HandleMouseReleaseEvent(
      WebMouseEvent(WebMouseEvent::Type::kMouseUp, gfx::PointF(center),
                    gfx::PointF(center), WebPointerProperties::Button::kLeft, 1,
                    WebInputEvent::kNoModifiers,
                    WebInputEvent::GetStaticTimeStampForTests()));

  EXPECT_TRUE(listener->clicked());
}

TEST_F(ImageReplacementSimTest, ImageReplacementRendering) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kImageReplacement);
  SimRequest main_resource("https://example.com/index.html", "text/html");
  LoadURL("https://example.com/index.html");
  main_resource.Complete(R"(
    <img src="data:image/gif;base64,R0lGODlhAQABAIAAAP///////yH5BAEKAAEALAAAAAABAAEAAAICTAEAOw=="
         id="target"></img>
  )");

  Compositor().BeginFrame();
  test::RunPendingTasks();

  HTMLImageElement* img = To<HTMLImageElement>(
      GetDocument().getElementById(AtomicString("target")));
  ASSERT_TRUE(img);

  auto result = ImageReplacement::CreateAndBindReceiver(*img);
  ASSERT_TRUE(result.has_value());

  mojo::Remote<mojom::blink::ImageReplacement> replacement_remote(
      std::move(result.value()));
  MockImageReplacementHost mock_host;
  replacement_remote->StartReplacement(
      mock_host.receiver().BindNewPipeAndPassRemote(), std::nullopt);
  test::RunPendingTasks();
  Compositor().BeginFrame();

  // Swap the replacement iframe's content frame to be a remote frame (to more
  // accurately represent how image replacement will be used).
  auto* iframe =
      DynamicTo<HTMLIFrameElement>(img->UserAgentShadowRoot()->firstChild());
  ASSERT_TRUE(iframe);
  WebRemoteFrameImpl* remote_frame = frame_test_helpers::CreateRemote();
  frame_test_helpers::SwapRemoteFrame(
      WebFrame::FromCoreFrame(iframe->ContentFrame()), remote_frame);
  ASSERT_TRUE(iframe->ContentFrame()->IsRemoteFrame());
  Compositor().BeginFrame();

  // Before RenderReplacement is called, the image should still be painted. The
  // iframe should have a layout object created (and should be sized to fit
  // the image's content box), and its rendering should not be throttled.
  ASSERT_TRUE(iframe->GetLayoutBox());
  EXPECT_EQ(iframe->GetLayoutBox()->PhysicalBorderBoxRect(),
            img->GetLayoutBox()->PhysicalContentBoxRect());
  EXPECT_GT(CountDrawImageRectOps(GetDocument().View()->GetPaintRecord()), 0u);
  EXPECT_FALSE(iframe->ContentFrame()->View()->CanThrottleRendering());

  replacement_remote->RenderReplacement();
  test::RunPendingTasks();
  Compositor().BeginFrame();

  // After RenderReplacement is called, the image should no longer be painted.
  // The iframe should be visible and painted instead.
  ASSERT_TRUE(iframe->GetLayoutBox());
  EXPECT_EQ(iframe->GetLayoutBox()->PhysicalBorderBoxRect(),
            img->GetLayoutBox()->PhysicalContentBoxRect());
  EXPECT_EQ(0u, CountDrawImageRectOps(GetDocument().View()->GetPaintRecord()));
  EXPECT_FALSE(iframe->ContentFrame()->View()->CanThrottleRendering());
  EXPECT_TRUE(iframe->GetLayoutBox()->Layer()->IsSelfPaintingLayer());
}

TEST_F(ImageReplacementSimTest,
       ReplacementContinuesToRenderAfterLayoutObjectIsRecreated) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kImageReplacement);
  SimRequest main_resource("https://example.com/index.html", "text/html");
  LoadURL("https://example.com/index.html");
  main_resource.Complete(R"(
    <img src="data:image/gif;base64,R0lGODlhAQABAIAAAP///////yH5BAEKAAEALAAAAAABAAEAAAICTAEAOw=="
         id="target"></img>
  )");

  Compositor().BeginFrame();
  test::RunPendingTasks();

  HTMLImageElement* img = To<HTMLImageElement>(
      GetDocument().getElementById(AtomicString("target")));
  ASSERT_TRUE(img);

  auto result = ImageReplacement::CreateAndBindReceiver(*img);
  ASSERT_TRUE(result.has_value());

  mojo::Remote<mojom::blink::ImageReplacement> replacement_remote(
      std::move(result.value()));
  MockImageReplacementHost mock_host;
  replacement_remote->StartReplacement(
      mock_host.receiver().BindNewPipeAndPassRemote(), std::nullopt);
  test::RunPendingTasks();
  Compositor().BeginFrame();

  auto* iframe =
      DynamicTo<HTMLIFrameElement>(img->UserAgentShadowRoot()->firstChild());
  ASSERT_TRUE(iframe);
  WebRemoteFrameImpl* remote_frame = frame_test_helpers::CreateRemote();
  frame_test_helpers::SwapRemoteFrame(
      WebFrame::FromCoreFrame(iframe->ContentFrame()), remote_frame);
  ASSERT_TRUE(iframe->ContentFrame()->IsRemoteFrame());
  Compositor().BeginFrame();

  replacement_remote->RenderReplacement();
  test::RunPendingTasks();
  Compositor().BeginFrame();

  EXPECT_EQ(0u, CountDrawImageRectOps(GetDocument().View()->GetPaintRecord()));

  // Destroy and recreate the layout object to verify stickiness.
  img->SetInlineStyleProperty(CSSPropertyID::kDisplay, CSSValueID::kNone);
  Compositor().BeginFrame();
  test::RunPendingTasks();
  EXPECT_FALSE(img->GetLayoutObject());

  img->SetInlineStyleProperty(CSSPropertyID::kDisplay, CSSValueID::kBlock);
  Compositor().BeginFrame();
  test::RunPendingTasks();
  EXPECT_TRUE(img->GetLayoutObject());

  // The original image should still not paint.
  EXPECT_EQ(0u, CountDrawImageRectOps(GetDocument().View()->GetPaintRecord()));
}

TEST_F(ImageReplacementSimTest, ImageReplacementWaitsForLoadAndResumes) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kImageReplacement);
  SimRequest main_resource("https://example.com/index.html", "text/html");
  SimSubresourceRequest image_resource("https://example.com/foo.png",
                                       "image/png");
  LoadURL("https://example.com/index.html");
  main_resource.Complete(R"(
    <img src="foo.png" id="target"></img>
  )");
  // Image not loaded yet.

  HTMLImageElement* img = To<HTMLImageElement>(
      GetDocument().getElementById(AtomicString("target")));
  auto result = ImageReplacement::CreateAndBindReceiver(*img);
  ASSERT_TRUE(result.has_value());

  mojo::Remote<mojom::blink::ImageReplacement> replacement_remote(
      std::move(result.value()));
  MockImageReplacementHost mock_host;
  replacement_remote->StartReplacement(
      mock_host.receiver().BindNewPipeAndPassRemote(), std::nullopt);
  test::RunPendingTasks();

  EXPECT_FALSE(img->HasImageReplacement());
  EXPECT_FALSE(img->UserAgentShadowRoot());
  EXPECT_FALSE(mock_host.frame_token().has_value());
  ImageReplacement* replacement =
      DocumentImageReplacements::From(GetDocument()).GetImageReplacement(img);
  ASSERT_TRUE(replacement);

  Vector<char> gif_data = GetTransparentGifBytes();
  // Finish image load.
  image_resource.Complete(gif_data);
  test::RunPendingTasks();

  EXPECT_TRUE(img->HasImageReplacement());
  EXPECT_TRUE(img->UserAgentShadowRoot());
  EXPECT_TRUE(mock_host.frame_token().has_value());
}

TEST_F(ImageReplacementSimTest, ImageReplacementRemovedFromDocument) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kImageReplacement);
  SimRequest main_resource("https://example.com/index.html", "text/html");
  LoadURL("https://example.com/index.html");
  main_resource.Complete(R"(
    <img src="data:image/gif;base64,R0lGODlhAQABAIAAAP///////yH5BAEKAAEALAAAAAABAAEAAAICTAEAOw=="
         id="target"></img>
  )");

  Compositor().BeginFrame();
  test::RunPendingTasks();

  HTMLImageElement* img = To<HTMLImageElement>(
      GetDocument().getElementById(AtomicString("target")));
  ASSERT_TRUE(img);
  ASSERT_TRUE(img->complete());

  auto result = ImageReplacement::CreateAndBindReceiver(*img);
  ASSERT_TRUE(result.has_value());

  mojo::Remote<mojom::blink::ImageReplacement> replacement_remote(
      std::move(result.value()));

  MockImageReplacementHost mock_host;
  replacement_remote->StartReplacement(
      mock_host.receiver().BindNewPipeAndPassRemote(), std::nullopt);
  test::RunPendingTasks();

  EXPECT_TRUE(img->HasImageReplacement());
  EXPECT_TRUE(img->UserAgentShadowRoot()->HasChildren());

  img->remove();
  test::RunPendingTasks();

  EXPECT_FALSE(img->HasImageReplacement());
  EXPECT_FALSE(img->UserAgentShadowRoot()->HasChildren());
  EXPECT_FALSE(replacement_remote.is_connected());
  EXPECT_FALSE(DocumentImageReplacements::FromIfExists(GetDocument())
                   ->GetImageReplacement(img));
}

TEST_F(ImageReplacementSimTest,
       ImageReplacementCreateOnDisconnectedImageFails) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kImageReplacement);
  SimRequest main_resource("https://example.com/index.html", "text/html");
  LoadURL("https://example.com/index.html");
  main_resource.Complete(R"(
    <img src="data:image/gif;base64,R0lGODlhAQABAIAAAP///////yH5BAEKAAEALAAAAAABAAEAAAICTAEAOw=="
         id="target"></img>
  )");

  Compositor().BeginFrame();
  test::RunPendingTasks();

  HTMLImageElement* img = To<HTMLImageElement>(
      GetDocument().getElementById(AtomicString("target")));
  ASSERT_TRUE(img);
  ASSERT_TRUE(img->complete());
  img->remove();
  auto result = ImageReplacement::CreateAndBindReceiver(*img);
  EXPECT_FALSE(result.has_value());
}

TEST_F(ImageReplacementSimTest, ImageReplacementCreateWithEmptySrcFails) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kImageReplacement);
  SimRequest main_resource("https://example.com/index.html", "text/html");
  LoadURL("https://example.com/index.html");
  main_resource.Complete(R"(
    <img src="" id="target"></img>
  )");

  Compositor().BeginFrame();
  test::RunPendingTasks();

  HTMLImageElement* img = To<HTMLImageElement>(
      GetDocument().getElementById(AtomicString("target")));
  ASSERT_TRUE(img);

  auto result = ImageReplacement::CreateAndBindReceiver(*img);
  EXPECT_FALSE(result.has_value());
}

TEST_F(ImageReplacementSimTest, ImageReplacementMovedToNewDocument) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kImageReplacement);
  SimRequest main_resource("https://example.com/index.html", "text/html");
  LoadURL("https://example.com/index.html");
  main_resource.Complete(R"(
    <iframe id="iframe"></iframe>
    <img src="data:image/gif;base64,R0lGODlhAQABAIAAAP///////yH5BAEKAAEALAAAAAABAAEAAAICTAEAOw=="
         id="target"></img>
  )");

  Compositor().BeginFrame();
  test::RunPendingTasks();

  HTMLImageElement* img = To<HTMLImageElement>(
      GetDocument().getElementById(AtomicString("target")));
  ASSERT_TRUE(img);
  ASSERT_TRUE(img->complete());

  auto result = ImageReplacement::CreateAndBindReceiver(*img);
  ASSERT_TRUE(result.has_value());

  mojo::Remote<mojom::blink::ImageReplacement> replacement_remote(
      std::move(result.value()));

  MockImageReplacementHost mock_host;
  replacement_remote->StartReplacement(
      mock_host.receiver().BindNewPipeAndPassRemote(), std::nullopt);
  test::RunPendingTasks();

  EXPECT_TRUE(img->HasImageReplacement());
  EXPECT_TRUE(img->UserAgentShadowRoot()->HasChildren());

  HTMLIFrameElement* iframe = To<HTMLIFrameElement>(
      GetDocument().getElementById(AtomicString("iframe")));
  ASSERT_TRUE(iframe);
  Document* new_document = iframe->contentDocument();
  ASSERT_TRUE(new_document);
  ASSERT_TRUE(new_document->body());

  new_document->body()->AppendChild(img);
  test::RunPendingTasks();

  EXPECT_FALSE(img->HasImageReplacement());
  EXPECT_FALSE(img->UserAgentShadowRoot()->HasChildren());
  EXPECT_FALSE(replacement_remote.is_connected());
  EXPECT_FALSE(DocumentImageReplacements::FromIfExists(GetDocument())
                   ->GetImageReplacement(img));
  EXPECT_FALSE(DocumentImageReplacements::FromIfExists(*new_document));

  // Verify that the replacement can be created in the new document.
  result = ImageReplacement::CreateAndBindReceiver(*img);
  EXPECT_TRUE(result.has_value());
  EXPECT_TRUE(DocumentImageReplacements::FromIfExists(*new_document)
                  ->GetImageReplacement(img));
}

TEST_F(ImageReplacementSimTest, ImageReplacementCreateWithFailedLoadFails) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kImageReplacement);
  SimRequest main_resource("https://example.com/index.html", "text/html");
  SimSubresourceRequest image_resource("https://example.com/foo.png",
                                       "image/png");
  LoadURL("https://example.com/index.html");
  main_resource.Complete(R"(
    <img src="foo.png" id="target"></img>
  )");

  image_resource.Complete("invalid data");

  Compositor().BeginFrame();
  test::RunPendingTasks();

  HTMLImageElement* img = To<HTMLImageElement>(
      GetDocument().getElementById(AtomicString("target")));
  ASSERT_TRUE(img);
  ASSERT_TRUE(img->CachedImage()->ErrorOccurred());

  auto result = ImageReplacement::CreateAndBindReceiver(*img);
  EXPECT_FALSE(result.has_value());
}

TEST_F(ImageReplacementSimTest, ResumeReplacementFailsIfImageLoadFails) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kImageReplacement);
  SimRequest main_resource("https://example.com/index.html", "text/html");
  SimSubresourceRequest image_resource("https://example.com/foo.png",
                                       "image/png");
  LoadURL("https://example.com/index.html");
  main_resource.Complete(R"(
    <img src="foo.png" id="target"></img>
  )");
  // Image not loaded yet.

  HTMLImageElement* img = To<HTMLImageElement>(
      GetDocument().getElementById(AtomicString("target")));
  auto result = ImageReplacement::CreateAndBindReceiver(*img);
  ASSERT_TRUE(result.has_value());

  mojo::Remote<mojom::blink::ImageReplacement> replacement_remote(
      std::move(result.value()));
  MockImageReplacementHost mock_host;
  replacement_remote->StartReplacement(
      mock_host.receiver().BindNewPipeAndPassRemote(), std::nullopt);
  test::RunPendingTasks();

  // Replacement should not have started since the image is not loaded.
  EXPECT_FALSE(img->HasImageReplacement());
  EXPECT_FALSE(img->UserAgentShadowRoot());
  EXPECT_FALSE(mock_host.frame_token().has_value());

  ImageReplacement* replacement =
      DocumentImageReplacements::From(GetDocument()).GetImageReplacement(img);
  ASSERT_TRUE(replacement);

  base::test::TestFuture<void> future;
  auto* error_listener =
      MakeGarbageCollected<ErrorEventListener>(future.GetCallback());
  img->addEventListener(AtomicString("error"), error_listener);

  // Finish the load (with invalid data to cause an error).
  image_resource.Complete("invalid data");
  EXPECT_TRUE(future.Wait());
  EXPECT_TRUE(img->CachedImage()->ErrorOccurred());

  // Replacement should not have started since the image load failed.
  mock_host.receiver().reset();
  replacement_remote->StartReplacement(
      mock_host.receiver().BindNewPipeAndPassRemote(), std::nullopt);
  test::RunPendingTasks();
  EXPECT_FALSE(img->HasImageReplacement());
  EXPECT_FALSE(replacement_remote.is_connected());
  EXPECT_FALSE(DocumentImageReplacements::FromIfExists(GetDocument())
                   ->GetImageReplacement(img));
}

TEST_F(ImageReplacementSimTest, ImageReplacementResetAfterSrcChange) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kImageReplacement);
  SimRequest main_resource("https://example.com/index.html", "text/html");
  LoadURL("https://example.com/index.html");
  main_resource.Complete(R"(
    <img src="data:image/gif;base64,R0lGODlhAQABAIAAAP///////yH5BAEKAAEALAAAAAABAAEAAAICTAEAOw=="
         id="target"></img>
  )");

  Compositor().BeginFrame();
  test::RunPendingTasks();

  HTMLImageElement* img = To<HTMLImageElement>(
      GetDocument().getElementById(AtomicString("target")));
  ASSERT_TRUE(img);

  auto result = ImageReplacement::CreateAndBindReceiver(*img);
  ASSERT_TRUE(result.has_value());

  mojo::Remote<mojom::blink::ImageReplacement> replacement_remote(
      std::move(result.value()));
  MockImageReplacementHost mock_host;
  replacement_remote->StartReplacement(
      mock_host.receiver().BindNewPipeAndPassRemote(), std::nullopt);
  test::RunPendingTasks();

  EXPECT_TRUE(img->HasImageReplacement());

  // Update src to the same value.
  img->setAttribute(
      html_names::kSrcAttr,
      AtomicString(
          "data:image/gif;base64,R0lGODlhAQABAIAAAP///////yH5BAEKAAEALAAAAAAB"
          "AAEAAAICTAEAOw=="));
  test::RunPendingTasks();

  // Replacement should not be reset.
  EXPECT_TRUE(img->HasImageReplacement());

  // Update src to a different value.
  img->setAttribute(
      html_names::kSrcAttr,
      AtomicString(
          "data:image/"
          "png;base64,iVBORw0KGgoAAAANSUhEUgAAAAoAAAAKCAYAAACNMs+"
          "9AAAAFUlEQVR42mP8r8ZQz0AEYBxVSF+FAF4REHM0zJumAAAAAElFTkSuQmCC"));
  test::RunPendingTasks();

  // Replacement should be reset.
  EXPECT_FALSE(img->HasImageReplacement());
}

TEST_F(ImageReplacementSimTest, ImageReplacementSendsObjectFit) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kImageReplacement);
  SimRequest main_resource("https://example.com/index.html", "text/html");
  LoadURL("https://example.com/index.html");
  main_resource.Complete(R"(
    <style>
      #target { object-fit: cover; }
    </style>
    <img src="data:image/gif;base64,R0lGODlhAQABAIAAAP///////yH5BAEKAAEALAAAAAABAAEAAAICTAEAOw=="
         id="target"></img>
  )");

  Compositor().BeginFrame();
  test::RunPendingTasks();

  HTMLImageElement* img = To<HTMLImageElement>(
      GetDocument().getElementById(AtomicString("target")));
  ASSERT_TRUE(img);
  ASSERT_TRUE(img->GetLayoutObject());

  auto result = ImageReplacement::CreateAndBindReceiver(*img);
  ASSERT_TRUE(result.has_value());

  mojo::Remote<mojom::blink::ImageReplacement> replacement_remote(
      std::move(result.value()));

  MockImageReplacementHost mock_host;
  replacement_remote->StartReplacement(
      mock_host.receiver().BindNewPipeAndPassRemote(), std::nullopt);
  test::RunPendingTasks();

  ASSERT_TRUE(mock_host.replacement_data());
  EXPECT_EQ(mock_host.replacement_data()->object_fit,
            mojom::blink::ObjectFit::kCover);
}

TEST_F(ImageReplacementSimTest, ImageReplacementNotResetAfterSizeChange) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kImageReplacement);
  SimRequest main_resource("https://example.com/index.html", "text/html");
  SimSubresourceRequest image_resource1("https://example.com/foo.png",
                                        "image/png");
  SimSubresourceRequest image_resource2("https://example.com/bar.png",
                                        "image/png");
  LoadURL("https://example.com/index.html");
  main_resource.Complete(R"(
    <style>
      #target { width: 50px; height: 50px; }
    </style>
    <img loading="lazy"
         sizes="auto"
         srcset="foo.png 100w, bar.png 200w"
         src="foo.png"
         id="target">
    </img>
  )");

  Vector<char> gif_data = GetTransparentGifBytes();

  HTMLImageElement* img = To<HTMLImageElement>(
      GetDocument().getElementById(AtomicString("target")));
  ASSERT_TRUE(img);

  Compositor().BeginFrame();
  test::RunPendingTasks();

  // Kick off the deferred load.
  img->LoadDeferredImageBlockingLoad();
  test::RunPendingTasks();

  image_resource1.Complete(gif_data);
  test::RunPendingTasks();
  Compositor().BeginFrame();

  ASSERT_TRUE(img->complete());

  auto result = ImageReplacement::CreateAndBindReceiver(*img);
  ASSERT_TRUE(result.has_value());

  mojo::Remote<mojom::blink::ImageReplacement> replacement_remote(
      std::move(result.value()));

  MockImageReplacementHost mock_host;
  replacement_remote->StartReplacement(
      mock_host.receiver().BindNewPipeAndPassRemote(), std::nullopt);
  test::RunPendingTasks();

  EXPECT_TRUE(img->HasImageReplacement());
  EXPECT_EQ(img->ImageSourceURL(), "foo.png");

  // Update layout size of the image to 150px and trigger selection of the
  // second srcset URL.
  img->SetInlineStyleProperty(CSSPropertyID::kWidth, "150px");
  Compositor().BeginFrame();
  test::RunPendingTasks();

  // The image's source should have updated.
  EXPECT_EQ(img->ImageSourceURL(), "bar.png");

  // The image replacement should not be reset.
  EXPECT_TRUE(img->HasImageReplacement());

  // Finish loading the new image resource.
  image_resource2.Complete(gif_data);
  test::RunPendingTasks();

  // The image replacement should still not be reset.
  EXPECT_TRUE(img->HasImageReplacement());
}

TEST_F(ImageReplacementSimTest,
       ImageReplacementNotResetAfterPictureSourceViewportSizeChange) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kImageReplacement);
  SimRequest main_resource("https://example.com/index.html", "text/html");
  SimSubresourceRequest image_resource1("https://example.com/foo.png",
                                        "image/png");
  SimSubresourceRequest image_resource2("https://example.com/bar.png",
                                        "image/png");
  LoadURL("https://example.com/index.html");
  WebView().MainFrameViewWidget()->Resize(gfx::Size(300, 300));
  main_resource.Complete(R"HTML(
    <picture id="picture">
      <source id="source1" media="(max-width: 400px)" srcset="foo.png">
      <source id="source2" media="(max-width: 800px)" srcset="bar.png">
      <img src="fallback.png" id="target">
    </picture>
  )HTML");

  Vector<char> gif_data = GetTransparentGifBytes();

  HTMLImageElement* img = To<HTMLImageElement>(
      GetDocument().getElementById(AtomicString("target")));
  ASSERT_TRUE(img);

  Compositor().BeginFrame();
  test::RunPendingTasks();

  // Kick off the deferred load.
  img->LoadDeferredImageBlockingLoad();
  test::RunPendingTasks();
  image_resource1.Complete(gif_data);
  test::RunPendingTasks();
  Compositor().BeginFrame();
  ASSERT_TRUE(img->complete());

  auto result = ImageReplacement::CreateAndBindReceiver(*img);
  ASSERT_TRUE(result.has_value());

  mojo::Remote<mojom::blink::ImageReplacement> replacement_remote(
      std::move(result.value()));
  MockImageReplacementHost mock_host;
  replacement_remote->StartReplacement(
      mock_host.receiver().BindNewPipeAndPassRemote(), std::nullopt);
  test::RunPendingTasks();

  EXPECT_TRUE(img->HasImageReplacement());
  EXPECT_EQ(img->ImageSourceURL(), "foo.png");

  // Resize viewport to 600x600.
  // - source1 media: (max-width: 400px) -> no longer matches.
  // - source2 media: (max-width: 800px) -> matches.
  // This triggers the selection of bar.png as the source URL.
  WebView().MainFrameViewWidget()->Resize(gfx::Size(600, 600));
  Compositor().BeginFrame();
  test::RunPendingTasks();

  EXPECT_EQ(img->ImageSourceURL(), "bar.png");

  // The image replacement should not be reset.
  EXPECT_TRUE(img->HasImageReplacement());

  // Finish loading the new image resource.
  image_resource2.Complete(gif_data);
  test::RunPendingTasks();

  // The image replacement should still not be reset.
  EXPECT_TRUE(img->HasImageReplacement());
}

TEST_F(ImageReplacementSimTest,
       ImageReplacementNotResetAfterSizeChangeWhileLoading) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kImageReplacement);
  SimRequest main_resource("https://example.com/index.html", "text/html");
  SimSubresourceRequest image_resource1("https://example.com/foo.png",
                                        "image/png");
  SimSubresourceRequest image_resource2("https://example.com/bar.png",
                                        "image/png");
  LoadURL("https://example.com/index.html");
  main_resource.Complete(R"(
    <style>
      #target { width: 50px; height: 50px; }
    </style>
    <img loading="lazy"
         sizes="auto" srcset="foo.png 100w, bar.png 200w"
         src="foo.png"
         id="target">
    </img>
  )");

  Vector<char> gif_data = GetTransparentGifBytes();

  HTMLImageElement* img = To<HTMLImageElement>(
      GetDocument().getElementById(AtomicString("target")));
  ASSERT_TRUE(img);

  // Perform initial layout and deliver ResizeObserver notifications to trigger
  // OnResize().
  Compositor().BeginFrame();
  test::RunPendingTasks();

  // Kick off the deferred load of foo.png.
  img->LoadDeferredImageBlockingLoad();
  test::RunPendingTasks();

  // At this point, image_resource1 is loading, but complete() is false.
  ASSERT_FALSE(img->complete());
  EXPECT_EQ(img->ImageSourceURL(), "foo.png");

  // Bind the image replacement receiver. Since complete() is false, this is a
  // pending replacement.
  auto result = ImageReplacement::CreateAndBindReceiver(*img);
  ASSERT_TRUE(result.has_value());

  mojo::Remote<mojom::blink::ImageReplacement> replacement_remote(
      std::move(result.value()));
  MockImageReplacementHost mock_host;
  replacement_remote->StartReplacement(
      mock_host.receiver().BindNewPipeAndPassRemote(), std::nullopt);
  test::RunPendingTasks();

  // Replacement is registered but not active (disposition is still
  // kPrimaryContent).
  EXPECT_FALSE(img->HasImageReplacement());

  // Update layout size of the image to 150px and trigger OnResize().
  // This will select bar.png and start loading it.
  img->SetInlineStyleProperty(CSSPropertyID::kWidth, "150px");
  Compositor().BeginFrame();
  test::RunPendingTasks();

  EXPECT_EQ(img->ImageSourceURL(), "bar.png");

  // Replacement should still be pending and not active.
  EXPECT_FALSE(img->HasImageReplacement());

  // Finish loading the new size-changed image (bar.png).
  image_resource2.Complete(gif_data);
  test::RunPendingTasks();

  // Now that the new image loaded, the replacement should resume and be active.
  EXPECT_TRUE(img->HasImageReplacement());
}

TEST_F(ImageReplacementSimTest,
       ImageReplacementNotResetAfterViewportSizeChange) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kImageReplacement);
  SimRequest main_resource("https://example.com/index.html", "text/html");
  SimSubresourceRequest image_resource1("https://example.com/foo.png",
                                        "image/png");
  SimSubresourceRequest image_resource2("https://example.com/bar.png",
                                        "image/png");
  LoadURL("https://example.com/index.html");
  WebView().MainFrameViewWidget()->Resize(gfx::Size(100, 100));
  main_resource.Complete(R"(
    <style>
      #target { width: 50vw; height: 50vw; }
    </style>
    <img loading="lazy"
         sizes="auto"
         srcset="foo.png 100w, bar.png 200w"
         src="foo.png"
         id="target"></img>
  )");

  Vector<char> gif_data = GetTransparentGifBytes();

  HTMLImageElement* img = To<HTMLImageElement>(
      GetDocument().getElementById(AtomicString("target")));
  ASSERT_TRUE(img);

  Compositor().BeginFrame();
  test::RunPendingTasks();

  // Kick off the deferred load.
  img->LoadDeferredImageBlockingLoad();
  test::RunPendingTasks();
  image_resource1.Complete(gif_data);
  test::RunPendingTasks();
  Compositor().BeginFrame();
  ASSERT_TRUE(img->complete());

  auto result = ImageReplacement::CreateAndBindReceiver(*img);
  ASSERT_TRUE(result.has_value());

  mojo::Remote<mojom::blink::ImageReplacement> replacement_remote(
      std::move(result.value()));
  MockImageReplacementHost mock_host;
  replacement_remote->StartReplacement(
      mock_host.receiver().BindNewPipeAndPassRemote(), std::nullopt);
  test::RunPendingTasks();

  EXPECT_TRUE(img->HasImageReplacement());
  EXPECT_EQ(img->ImageSourceURL(), "foo.png");

  // Resize viewport to 300x300, which updates image layout size to 150px and
  // triggers the selection of bar.png as source URL.
  WebView().MainFrameViewWidget()->Resize(gfx::Size(300, 300));
  Compositor().BeginFrame();
  test::RunPendingTasks();

  EXPECT_EQ(img->ImageSourceURL(), "bar.png");

  // The image replacement should not be reset.
  EXPECT_TRUE(img->HasImageReplacement());

  // Finish loading the new image resource.
  image_resource2.Complete(gif_data);
  test::RunPendingTasks();

  // The image replacement should still not be reset.
  EXPECT_TRUE(img->HasImageReplacement());
}

TEST_F(ImageReplacementSimTest,
       ImageReplacementNotResetAfterSizesAttributeUpdate) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kImageReplacement);
  SimRequest main_resource("https://example.com/index.html", "text/html");
  SimSubresourceRequest image_resource1("https://example.com/foo.png",
                                        "image/png");
  SimSubresourceRequest image_resource2("https://example.com/bar.png",
                                        "image/png");
  LoadURL("https://example.com/index.html");
  main_resource.Complete(R"(
    <img sizes="50px"
         srcset="foo.png 100w, bar.png 200w"
         src="foo.png"
         id="target">
    </img>
  )");

  Vector<char> gif_data = GetTransparentGifBytes();

  HTMLImageElement* img = To<HTMLImageElement>(
      GetDocument().getElementById(AtomicString("target")));
  ASSERT_TRUE(img);

  Compositor().BeginFrame();
  test::RunPendingTasks();

  image_resource1.Complete(gif_data);
  test::RunPendingTasks();
  Compositor().BeginFrame();

  ASSERT_TRUE(img->complete());

  auto result = ImageReplacement::CreateAndBindReceiver(*img);
  ASSERT_TRUE(result.has_value());

  mojo::Remote<mojom::blink::ImageReplacement> replacement_remote(
      std::move(result.value()));

  MockImageReplacementHost mock_host;
  replacement_remote->StartReplacement(
      mock_host.receiver().BindNewPipeAndPassRemote(), std::nullopt);
  test::RunPendingTasks();

  EXPECT_TRUE(img->HasImageReplacement());
  EXPECT_EQ(img->ImageSourceURL(), "foo.png");

  // Update sizes attribute of the image to 150px and trigger selection of the
  // second srcset URL.
  img->setAttribute(html_names::kSizesAttr, AtomicString("150px"));
  Compositor().BeginFrame();
  test::RunPendingTasks();

  // The image's source should have updated.
  EXPECT_EQ(img->ImageSourceURL(), "bar.png");

  // Finish loading the new image resource.
  image_resource2.Complete(gif_data);
  test::RunPendingTasks();

  // The image replacement should not be reset.
  EXPECT_TRUE(img->HasImageReplacement());
}

TEST_F(ImageReplacementSimTest, UserAgentImageReplacementAPI) {
  ScopedUAImageReplacementAPIForTest scoped_ua_image_replacement(true);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kImageReplacement);
  SimRequest main_resource("https://example.com/index.html", "text/html");
  LoadURL("https://example.com/index.html");
  main_resource.Complete(R"(
    <img src="data:image/gif;base64,R0lGODlhAQABAIAAAP///////yH5BAEKAAEALAAAAAABAAEAAAICTAEAOw=="
         id="target">
  )");

  Compositor().BeginFrame();
  test::RunPendingTasks();

  HTMLImageElement* img = To<HTMLImageElement>(
      GetDocument().getElementById(AtomicString("target")));
  ASSERT_TRUE(img);
  ASSERT_TRUE(img->complete());

  EXPECT_FALSE(img->replacedByUserAgent());

  v8::HandleScope handle_scope(Window().GetIsolate());

  MainFrame().ExecuteScript(WebScriptSource(R"JS(
    const img = document.getElementById('target');
    window.startEvents = 0;
    window.endEvents = 0;
    window.onStartHandlerFired = 0;
    window.onEndHandlerFired = 0;

    img.addEventListener('uareplacestart', () => { window.startEvents++; });
    img.addEventListener('uareplaceend', () => { window.endEvents++; });
    img.onuareplacestart = () => { window.onStartHandlerFired++; };
    img.onuareplaceend = () => { window.onEndHandlerFired++; };
  )JS"));

  auto result = ImageReplacement::CreateAndBindReceiver(*img);
  ASSERT_TRUE(result.has_value());

  mojo::Remote<mojom::blink::ImageReplacement> replacement_remote(
      std::move(result.value()));

  MockImageReplacementHost mock_host;
  replacement_remote->StartReplacement(
      mock_host.receiver().BindNewPipeAndPassRemote(), std::nullopt);
  test::RunPendingTasks();

  EXPECT_TRUE(img->replacedByUserAgent());

  v8::Local<v8::Value> js_replaced = MainFrame().ExecuteScriptAndReturnValue(
      WebScriptSource("img.replacedByUserAgent"));
  EXPECT_TRUE(js_replaced->IsTrue());

  v8::Local<v8::Value> start_count = MainFrame().ExecuteScriptAndReturnValue(
      WebScriptSource("window.startEvents"));
  ASSERT_TRUE(start_count->IsInt32());
  EXPECT_EQ(1, start_count.As<v8::Int32>()->Value());

  v8::Local<v8::Value> on_start_count = MainFrame().ExecuteScriptAndReturnValue(
      WebScriptSource("window.onStartHandlerFired"));
  ASSERT_TRUE(on_start_count->IsInt32());
  EXPECT_EQ(1, on_start_count.As<v8::Int32>()->Value());

  // Reset image replacement.
  img->ResetImageReplacement();
  test::RunPendingTasks();

  EXPECT_FALSE(img->replacedByUserAgent());

  v8::Local<v8::Value> js_replaced_after_reset =
      MainFrame().ExecuteScriptAndReturnValue(
          WebScriptSource("img.replacedByUserAgent"));
  EXPECT_TRUE(js_replaced_after_reset->IsFalse());

  v8::Local<v8::Value> end_count = MainFrame().ExecuteScriptAndReturnValue(
      WebScriptSource("window.endEvents"));
  ASSERT_TRUE(end_count->IsInt32());
  EXPECT_EQ(1, end_count.As<v8::Int32>()->Value());

  v8::Local<v8::Value> on_end_count = MainFrame().ExecuteScriptAndReturnValue(
      WebScriptSource("window.onEndHandlerFired"));
  ASSERT_TRUE(on_end_count->IsInt32());
  EXPECT_EQ(1, on_end_count.As<v8::Int32>()->Value());

  EXPECT_TRUE(GetDocument().IsUseCounted(
      mojom::WebFeature::
          kV8HTMLImageElement_ReplacedByUserAgent_AttributeGetter));
  EXPECT_TRUE(GetDocument().IsUseCounted(
      mojom::WebFeature::kUAReplaceStartAddListener));
  EXPECT_TRUE(
      GetDocument().IsUseCounted(mojom::WebFeature::kUAReplaceEndAddListener));
}

TEST_F(ImageReplacementSimTest, UserAgentImageReplacementAPIDisabled) {
  ScopedUAImageReplacementAPIForTest scoped_ua_image_replacement(false);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kImageReplacement);

  SimRequest main_resource("https://example.com/index.html", "text/html");
  LoadURL("https://example.com/index.html");
  main_resource.Complete(R"(
    <img src="data:image/gif;base64,R0lGODlhAQABAIAAAP///////yH5BAEKAAEALAAAAAABAAEAAAICTAEAOw=="
         id="target">
  )");

  Compositor().BeginFrame();
  test::RunPendingTasks();

  HTMLImageElement* img = To<HTMLImageElement>(
      GetDocument().getElementById(AtomicString("target")));
  ASSERT_TRUE(img);

  v8::HandleScope handle_scope(Window().GetIsolate());

  MainFrame().ExecuteScript(WebScriptSource(R"JS(
    const img = document.getElementById('target');
    window.eventsFired = 0;
    img.addEventListener('uareplacestart', () => { window.eventsFired++; });
    img.addEventListener('uareplaceend', () => { window.eventsFired++; });
  )JS"));

  auto result = ImageReplacement::CreateAndBindReceiver(*img);
  ASSERT_TRUE(result.has_value());

  mojo::Remote<mojom::blink::ImageReplacement> replacement_remote(
      std::move(result.value()));

  MockImageReplacementHost mock_host;
  replacement_remote->StartReplacement(
      mock_host.receiver().BindNewPipeAndPassRemote(), std::nullopt);
  test::RunPendingTasks();

  img->ResetImageReplacement();
  test::RunPendingTasks();

  v8::Local<v8::Value> count = MainFrame().ExecuteScriptAndReturnValue(
      WebScriptSource("window.eventsFired"));
  ASSERT_TRUE(count->IsInt32());
  EXPECT_EQ(0, count.As<v8::Int32>()->Value());
}

TEST_F(ImageReplacementSimTest, UserAgentImageReplacementAPIElementRemoved) {
  ScopedUAImageReplacementAPIForTest scoped_ua_image_replacement(true);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kImageReplacement);
  SimRequest main_resource("https://example.com/index.html", "text/html");
  LoadURL("https://example.com/index.html");
  main_resource.Complete(R"(
    <img src="data:image/gif;base64,R0lGODlhAQABAIAAAP///////yH5BAEKAAEALAAAAAABAAEAAAICTAEAOw=="
         id="target">
  )");

  Compositor().BeginFrame();
  test::RunPendingTasks();

  HTMLImageElement* img = To<HTMLImageElement>(
      GetDocument().getElementById(AtomicString("target")));
  ASSERT_TRUE(img);

  v8::HandleScope handle_scope(Window().GetIsolate());

  MainFrame().ExecuteScript(WebScriptSource(R"JS(
    const img = document.getElementById('target');
    window.endEvents = 0;
    img.addEventListener('uareplaceend', () => { window.endEvents++; });
  )JS"));

  auto result = ImageReplacement::CreateAndBindReceiver(*img);
  ASSERT_TRUE(result.has_value());

  mojo::Remote<mojom::blink::ImageReplacement> replacement_remote(
      std::move(result.value()));

  MockImageReplacementHost mock_host;
  replacement_remote->StartReplacement(
      mock_host.receiver().BindNewPipeAndPassRemote(), std::nullopt);
  test::RunPendingTasks();

  EXPECT_TRUE(img->replacedByUserAgent());

  // Remove element from DOM.
  MainFrame().ExecuteScript(WebScriptSource("img.remove();"));
  test::RunPendingTasks();

  EXPECT_FALSE(img->replacedByUserAgent());

  v8::Local<v8::Value> end_count = MainFrame().ExecuteScriptAndReturnValue(
      WebScriptSource("window.endEvents"));
  ASSERT_TRUE(end_count->IsInt32());
  EXPECT_EQ(1, end_count.As<v8::Int32>()->Value());
}

TEST_F(ImageReplacementSimTest, UserAgentImageReplacementAPIWaitsForImageLoad) {
  ScopedUAImageReplacementAPIForTest scoped_ua_image_replacement(true);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kImageReplacement);

  SimRequest main_resource("https://example.com/index.html", "text/html");
  SimSubresourceRequest image_resource("https://example.com/foo.png",
                                       "image/png");
  LoadURL("https://example.com/index.html");
  main_resource.Complete(R"(
    <img src="foo.png" id="target">
  )");

  HTMLImageElement* img = To<HTMLImageElement>(
      GetDocument().getElementById(AtomicString("target")));
  ASSERT_TRUE(img);
  ASSERT_FALSE(img->complete());

  v8::HandleScope handle_scope(Window().GetIsolate());

  MainFrame().ExecuteScript(WebScriptSource(R"JS(
    const img = document.getElementById('target');
    window.startEvents = 0;
    img.addEventListener('uareplacestart', () => { window.startEvents++; });
  )JS"));

  auto result = ImageReplacement::CreateAndBindReceiver(*img);
  ASSERT_TRUE(result.has_value());

  mojo::Remote<mojom::blink::ImageReplacement> replacement_remote(
      std::move(result.value()));
  MockImageReplacementHost mock_host;
  replacement_remote->StartReplacement(
      mock_host.receiver().BindNewPipeAndPassRemote(), std::nullopt);
  test::RunPendingTasks();

  // Image load is still pending. Replacement should not be active yet.
  EXPECT_FALSE(img->replacedByUserAgent());

  v8::Local<v8::Value> js_replaced_before_load =
      MainFrame().ExecuteScriptAndReturnValue(
          WebScriptSource("img.replacedByUserAgent"));
  EXPECT_TRUE(js_replaced_before_load->IsFalse());

  v8::Local<v8::Value> start_count_before_load =
      MainFrame().ExecuteScriptAndReturnValue(
          WebScriptSource("window.startEvents"));
  ASSERT_TRUE(start_count_before_load->IsInt32());
  EXPECT_EQ(0, start_count_before_load.As<v8::Int32>()->Value());

  // Finish image load.
  Vector<char> gif_data = GetTransparentGifBytes();
  image_resource.Complete(gif_data);
  test::RunPendingTasks();

  // Replacement becomes active and uareplacestart fires.
  EXPECT_TRUE(img->replacedByUserAgent());

  v8::Local<v8::Value> js_replaced_after_load =
      MainFrame().ExecuteScriptAndReturnValue(
          WebScriptSource("img.replacedByUserAgent"));
  EXPECT_TRUE(js_replaced_after_load->IsTrue());

  v8::Local<v8::Value> start_count_after_load =
      MainFrame().ExecuteScriptAndReturnValue(
          WebScriptSource("window.startEvents"));
  ASSERT_TRUE(start_count_after_load->IsInt32());
  EXPECT_EQ(1, start_count_after_load.As<v8::Int32>()->Value());
}

}  // namespace blink
