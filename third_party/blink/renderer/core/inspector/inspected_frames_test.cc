// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/inspector/inspected_frames.h"

#include <memory>
#include <optional>

#include "base/unguessable_token.h"
#include "skia/ext/font_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/renderer/core/exported/web_view_impl.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/inspector/inspector_layer_tree_agent.h"
#include "third_party/blink/renderer/core/inspector/protocol/layer_tree.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/storage/blink_storage_key.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkFont.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "third_party/skia/include/core/SkPictureRecorder.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/include/core/SkSerialProcs.h"
#include "third_party/skia/include/core/SkStream.h"
#include "third_party/skia/include/core/SkString.h"
#include "third_party/skia/include/core/SkTextBlob.h"
#include "third_party/skia/include/core/SkTypeface.h"
#include "third_party/skia/include/effects/SkRuntimeEffect.h"

namespace blink {

namespace {

class TestLayerTreeClient final : public GarbageCollected<TestLayerTreeClient>,
                                  public InspectorLayerTreeAgent::Client {
 public:
  bool IsInspectorLayer(const cc::Layer*) override { return false; }
  void Trace(Visitor* visitor) const override {
    InspectorLayerTreeAgent::Client::Trace(visitor);
  }
};

std::unique_ptr<protocol::Array<protocol::LayerTree::PictureTile>>
MakePictureTiles(const SkPicture* picture,
                 const SkSerialProcs* procs = nullptr) {
  SkDynamicMemoryWStream stream;
  picture->serialize(&stream, procs);
  Vector<uint8_t> bytes(static_cast<wtf_size_t>(stream.bytesWritten()));
  stream.copyTo(bytes.data());

  auto tiles =
      std::make_unique<protocol::Array<protocol::LayerTree::PictureTile>>();
  tiles->emplace_back(
      protocol::LayerTree::PictureTile::create()
          .setX(0)
          .setY(0)
          .setPicture(protocol::Binary::fromVector(std::move(bytes)))
          .build());
  return tiles;
}

sk_sp<SkPicture> CreateSimplePicture() {
  SkPictureRecorder recorder;
  SkCanvas* canvas = recorder.beginRecording(16, 16);
  SkPaint paint;
  paint.setColor(SK_ColorBLUE);
  canvas->drawRect(SkRect::MakeWH(16, 16), paint);
  SkFont font = skia::DefaultFont();
  font.setSize(12.0f);
  if (sk_sp<SkTextBlob> blob = SkTextBlob::MakeFromString("A", font)) {
    canvas->drawTextBlob(blob, 2.0f, 12.0f, paint);
  }
  return recorder.finishRecordingAsPicture();
}

sk_sp<SkPicture> CreateRuntimeShaderPicture() {
  SkRuntimeEffect::Result result = SkRuntimeEffect::MakeForShader(
      SkString("half4 main(float2 p) { return half4(1, 0, 0, 1); }"));
  if (!result.effect) {
    return nullptr;
  }
  SkPictureRecorder recorder;
  SkCanvas* canvas = recorder.beginRecording(16, 16);
  SkPaint paint;
  paint.setShader(result.effect->makeShader(nullptr, {}));
  canvas->drawPaint(paint);
  return recorder.finishRecordingAsPicture();
}

}  // namespace

class InspectedFramesTest : public testing::Test {
 public:
  InspectedFramesTest() = default;
  ~InspectedFramesTest() override = default;

 private:
  test::TaskEnvironment task_environment_;
};

TEST_F(InspectedFramesTest, FindsFrameForGivenStorageKey) {
  auto security_origin =
      SecurityOrigin::CreateFromString("http://example.site");
  auto nonce = base::UnguessableToken::Create();
  auto blink_storage_key =
      BlinkStorageKey::CreateWithNonce(security_origin, nonce);

  auto page_holder = std::make_unique<DummyPageHolder>(
      gfx::Size(800, 600), nullptr, nullptr, base::NullCallback());
  LocalDOMWindow* dom_window = page_holder->GetFrame().DomWindow();
  dom_window->SetStorageKey(blink_storage_key);

  InspectedFrames* inspected_frames =
      MakeGarbageCollected<InspectedFrames>(&page_holder->GetFrame());
  std::string storage_key =
      static_cast<StorageKey>(blink_storage_key).Serialize();

  EXPECT_EQ(page_holder->GetFrame(),
            inspected_frames->FrameWithStorageKey(String(storage_key)));
}

TEST_F(InspectedFramesTest, LayerTreeLoadSnapshotRestrictedToMainFrame) {
  frame_test_helpers::WebViewHelper helper;
  WebViewImpl* web_view = helper.Initialize();
  WebLocalFrameImpl* main_web_frame = web_view->MainFrameImpl();
  frame_test_helpers::LoadHTMLString(
      main_web_frame, "<!DOCTYPE html><iframe name='sub'></iframe>",
      url_test_helpers::ToKURL("http://example.com/"));

  LocalFrame* main_frame = main_web_frame->GetFrame();
  ASSERT_NE(nullptr, main_frame);
  ASSERT_TRUE(main_frame->IsOutermostMainFrame());

  auto* child_web_frame =
      To<WebLocalFrameImpl>(main_web_frame->FirstChild()->ToWebLocalFrame());
  ASSERT_NE(nullptr, child_web_frame);
  LocalFrame* child_frame = child_web_frame->GetFrame();
  ASSERT_NE(nullptr, child_frame);
  ASSERT_FALSE(child_frame->IsOutermostMainFrame());

  sk_sp<SkPicture> picture = CreateSimplePicture();
  ASSERT_NE(nullptr, picture);
  SkSerialProcs serial_procs{
      .fTypefaceProc = [](SkTypeface* typeface, void*) -> SkSerialReturnType {
        return typeface->serialize(
            SkTypeface::SerializeBehavior::kDontIncludeData);
      },
  };

  auto* client = MakeGarbageCollected<TestLayerTreeClient>();

  // Loading a valid snapshot on the outermost main frame succeeds.
  auto* main_agent = MakeGarbageCollected<InspectorLayerTreeAgent>(
      MakeGarbageCollected<InspectedFrames>(main_frame), client);
  String main_snapshot_id;
  protocol::Response main_response = main_agent->loadSnapshot(
      MakePictureTiles(picture.get(), &serial_procs), &main_snapshot_id);
  EXPECT_TRUE(main_response.IsSuccess());
  EXPECT_FALSE(main_snapshot_id.empty());

  String data_url;
  protocol::Response replay_response = main_agent->replaySnapshot(
      main_snapshot_id, std::nullopt, std::nullopt, std::nullopt, &data_url);
  EXPECT_TRUE(replay_response.IsSuccess());
  EXPECT_FALSE(data_url.empty());

  // Loading a snapshot when rooted at a subframe is rejected.
  auto* child_agent = MakeGarbageCollected<InspectorLayerTreeAgent>(
      MakeGarbageCollected<InspectedFrames>(child_frame), client);
  String child_snapshot_id;
  protocol::Response child_response = child_agent->loadSnapshot(
      MakePictureTiles(picture.get(), &serial_procs), &child_snapshot_id);
  EXPECT_FALSE(child_response.IsSuccess());
  EXPECT_TRUE(child_snapshot_id.empty());

  helper.Reset();
}

TEST_F(InspectedFramesTest, LayerTreeLoadSnapshotRejectsRuntimeShaderPicture) {
  auto page_holder = std::make_unique<DummyPageHolder>(
      gfx::Size(800, 600), nullptr, nullptr, base::NullCallback());
  auto* inspected_frames =
      MakeGarbageCollected<InspectedFrames>(&page_holder->GetFrame());
  auto* client = MakeGarbageCollected<TestLayerTreeClient>();
  auto* agent =
      MakeGarbageCollected<InspectorLayerTreeAgent>(inspected_frames, client);

  sk_sp<SkPicture> shader_picture = CreateRuntimeShaderPicture();
  ASSERT_NE(nullptr, shader_picture);

  String snapshot_id;
  protocol::Response response =
      agent->loadSnapshot(MakePictureTiles(shader_picture.get()), &snapshot_id);
  EXPECT_FALSE(response.IsSuccess());
  EXPECT_TRUE(snapshot_id.empty());
}

}  // namespace blink
