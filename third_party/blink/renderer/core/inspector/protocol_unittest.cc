// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits>
#include <memory>

#include "base/containers/span.h"
#include "skia/ext/font_utils.h"
#include "skia/ext/skia_utils_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/inspector/inspected_frames.h"
#include "third_party/blink/renderer/core/inspector/inspector_layer_tree_agent.h"
#include "third_party/blink/renderer/core/inspector/protocol/layer_tree.h"
#include "third_party/blink/renderer/core/inspector/v8_inspector_string.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/graphics/picture_snapshot.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkFont.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "third_party/skia/include/core/SkPictureRecorder.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/include/core/SkSerialProcs.h"
#include "third_party/skia/include/core/SkShader.h"
#include "third_party/skia/include/core/SkString.h"
#include "third_party/skia/include/core/SkSurfaceProps.h"
#include "third_party/skia/include/core/SkTextBlob.h"
#include "third_party/skia/include/core/SkTypeface.h"
#include "third_party/skia/include/effects/SkRuntimeEffect.h"
#include "third_party/skia/include/utils/SkCustomTypeface.h"

namespace blink {
namespace protocol {
namespace {

base::span<const uint8_t> SkDataSpan(const sk_sp<const SkData>& data) {
  return skia::as_byte_span(*data);
}

std::unique_ptr<Array<LayerTree::PictureTile>> MakeSingleTileArray(
    const sk_sp<const SkData>& data,
    double x = 0.0,
    double y = 0.0) {
  auto tiles = std::make_unique<Array<LayerTree::PictureTile>>();
  tiles->emplace_back(LayerTree::PictureTile::create()
                          .setX(x)
                          .setY(y)
                          .setPicture(Binary::fromSpan(SkDataSpan(data)))
                          .build());
  return tiles;
}

sk_sp<const SkData> SerializeValidPicture() {
  SkPictureRecorder recorder;
  SkCanvas* canvas = recorder.beginRecording(50, 50);
  SkPaint paint;
  paint.setColor(SK_ColorBLUE);
  canvas->drawRect(SkRect::MakeWH(25, 25), paint);
  SkFont font = skia::DefaultFont();
  sk_sp<SkTextBlob> blob = SkTextBlob::MakeFromString("Sample", font);
  if (blob) {
    canvas->drawTextBlob(blob, 5, 20, paint);
  }
  sk_sp<SkPicture> picture = recorder.finishRecordingAsPicture();
  SkSerialProcs procs{
      .fTypefaceProc = [](SkTypeface* tf, void*) -> SkSerialReturnType {
        return tf->serialize(SkTypeface::SerializeBehavior::kDontIncludeData);
      }};
  return picture->serialize(&procs);
}

TEST(ProtocolBinaryTest, base64EmptyArgs) {
  EXPECT_EQ(protocol::String(), Binary().toBase64());

  bool success = false;
  Binary decoded = Binary::fromBase64("", &success);
  EXPECT_TRUE(success);
  Vector<uint8_t> decoded_bytes;
  decoded_bytes.append_range(decoded.Span());
  EXPECT_EQ(Vector<uint8_t>(), decoded_bytes);
}

TEST(ProtocolStringTest, AllBytesBase64Roundtrip) {
  Vector<uint8_t> all_bytes;
  for (int ii = 0; ii < 255; ++ii) {
    all_bytes.push_back(ii);
  }
  Binary binary = Binary::fromVector(all_bytes);
  bool success = false;
  Binary decoded = Binary::fromBase64(binary.toBase64(), &success);
  EXPECT_TRUE(success);
  Vector<uint8_t> decoded_bytes;
  decoded_bytes.append_range(decoded.Span());
  EXPECT_EQ(all_bytes, decoded_bytes);
}

TEST(ProtocolStringTest, HelloWorldBase64Roundtrip) {
  const char kMsg[] = "Hello, world.";
  Vector<uint8_t> msg;
  msg.append_range(base::span_from_cstring(kMsg));
  EXPECT_EQ(strlen(kMsg), msg.size());

  protocol::String encoded = Binary::fromVector(msg).toBase64();
  EXPECT_EQ("SGVsbG8sIHdvcmxkLg==", encoded);
  bool success = false;
  Binary decoded_binary = Binary::fromBase64(encoded, &success);
  EXPECT_TRUE(success);
  Vector<uint8_t> decoded;
  decoded.append_range(decoded_binary.Span());
  EXPECT_EQ(msg, decoded);
}

TEST(ProtocolBinaryTest, InvalidBase64Decode) {
  bool success = true;
  Binary binary = Binary::fromBase64("This is not base64.", &success);
  EXPECT_FALSE(success);
}

TEST(InspectorLayerTreeSnapshotTest, LoadsAndReplaysValidSnapshot) {
  test::TaskEnvironment task_environment;
  DummyPageHolder page_holder;
  auto* agent = MakeGarbageCollected<InspectorLayerTreeAgent>(
      MakeGarbageCollected<InspectedFrames>(&page_holder.GetFrame()), nullptr);

  sk_sp<const SkData> data = SerializeValidPicture();
  ASSERT_TRUE(data);

  String snapshot_id;
  Response load_response =
      agent->loadSnapshot(MakeSingleTileArray(data), &snapshot_id);
  ASSERT_TRUE(load_response.IsSuccess());
  EXPECT_FALSE(snapshot_id.empty());

  String data_url;
  Response replay_response = agent->replaySnapshot(
      snapshot_id, std::nullopt, std::nullopt, 1.0, &data_url);
  EXPECT_TRUE(replay_response.IsSuccess());
  EXPECT_TRUE(data_url.starts_with("data:image/png;base64,"));

  std::unique_ptr<Array<DictionaryValue>> command_log;
  Response log_response = agent->snapshotCommandLog(snapshot_id, &command_log);
  EXPECT_TRUE(log_response.IsSuccess());
  ASSERT_TRUE(command_log);
  EXPECT_FALSE(command_log->empty());

  std::unique_ptr<Array<Array<double>>> timings;
  Response profile_response =
      agent->profileSnapshot(snapshot_id, 2, 0.0, nullptr, &timings);
  EXPECT_TRUE(profile_response.IsSuccess());
  ASSERT_TRUE(timings);
  EXPECT_EQ(2u, timings->size());

  Response release_response = agent->releaseSnapshot(snapshot_id);
  EXPECT_TRUE(release_response.IsSuccess());
}

TEST(InspectorLayerTreeSnapshotTest, RejectsRuntimeShaderInSnapshot) {
  test::TaskEnvironment task_environment;
  DummyPageHolder page_holder;
  auto* agent = MakeGarbageCollected<InspectorLayerTreeAgent>(
      MakeGarbageCollected<InspectedFrames>(&page_holder.GetFrame()), nullptr);

  auto effect_result = SkRuntimeEffect::MakeForShader(
      SkString("half4 main(float2 p) { return half4(0, 1, 0, 1); }"));
  ASSERT_TRUE(effect_result.effect);
  sk_sp<SkShader> shader = effect_result.effect->makeShader(nullptr, {});
  ASSERT_TRUE(shader);

  SkPictureRecorder recorder;
  SkCanvas* canvas = recorder.beginRecording(50, 50);
  SkPaint paint;
  paint.setShader(std::move(shader));
  canvas->drawRect(SkRect::MakeWH(50, 50), paint);
  sk_sp<SkPicture> picture = recorder.finishRecordingAsPicture();
  sk_sp<SkData> data = picture->serialize();
  ASSERT_TRUE(data);
  ASSERT_NE(nullptr, SkPicture::MakeFromData(data.get()));
  EXPECT_EQ(nullptr, PictureSnapshot::DeserializePicture(SkDataSpan(data)));

  String snapshot_id;
  Response response =
      agent->loadSnapshot(MakeSingleTileArray(data), &snapshot_id);
  EXPECT_FALSE(response.IsSuccess());
  EXPECT_TRUE(snapshot_id.empty());
}

TEST(InspectorLayerTreeSnapshotTest, IgnoresEmbeddedTypefaceStreamInSnapshot) {
  test::TaskEnvironment task_environment;
  DummyPageHolder page_holder;
  auto* agent = MakeGarbageCollected<InspectorLayerTreeAgent>(
      MakeGarbageCollected<InspectedFrames>(&page_holder.GetFrame()), nullptr);

  SkCustomTypefaceBuilder builder;
  builder.setGlyph(1, 16.0f, SkPath::Rect(SkRect::MakeWH(16, 16)));
  sk_sp<SkTypeface> custom_typeface = builder.detach();
  ASSERT_TRUE(custom_typeface);

  SkFont font(custom_typeface, 16.0f);
  const uint16_t glyph_id = 1;
  sk_sp<SkTextBlob> blob = SkTextBlob::MakeFromText(
      &glyph_id, sizeof(glyph_id), font, SkTextEncoding::kGlyphID);
  ASSERT_TRUE(blob);

  SkPictureRecorder recorder;
  SkCanvas* canvas = recorder.beginRecording(16, 16);
  SkPaint bg_paint;
  bg_paint.setColor(SK_ColorBLUE);
  canvas->drawRect(SkRect::MakeWH(16, 16), bg_paint);

  SkPaint text_paint;
  text_paint.setColor(SK_ColorRED);
  canvas->drawTextBlob(blob, 0, 0, text_paint);
  sk_sp<SkPicture> picture = recorder.finishRecordingAsPicture();
  sk_sp<SkData> data = picture->serialize();
  ASSERT_TRUE(data);

  sk_sp<SkPicture> deserialized =
      PictureSnapshot::DeserializePicture(SkDataSpan(data));
  ASSERT_TRUE(deserialized);

  SkBitmap bitmap;
  ASSERT_TRUE(bitmap.tryAllocN32Pixels(16, 16));
  bitmap.eraseColor(SK_ColorTRANSPARENT);
  SkCanvas playback_canvas(bitmap, SkSurfaceProps{});
  deserialized->playback(&playback_canvas);
  EXPECT_EQ(SK_ColorBLUE, bitmap.getColor(8, 8));

  String snapshot_id;
  Response load_response =
      agent->loadSnapshot(MakeSingleTileArray(data), &snapshot_id);
  ASSERT_TRUE(load_response.IsSuccess());

  String data_url;
  Response replay_response = agent->replaySnapshot(
      snapshot_id, std::nullopt, std::nullopt, 1.0, &data_url);
  EXPECT_TRUE(replay_response.IsSuccess());
}

TEST(InspectorLayerTreeSnapshotTest, RejectsInvalidBoundsAndParameters) {
  test::TaskEnvironment task_environment;
  DummyPageHolder page_holder;
  auto* agent = MakeGarbageCollected<InspectorLayerTreeAgent>(
      MakeGarbageCollected<InspectedFrames>(&page_holder.GetFrame()), nullptr);

  sk_sp<const SkData> valid_data = SerializeValidPicture();
  ASSERT_TRUE(valid_data);

  String snapshot_id;
  EXPECT_FALSE(
      agent
          ->loadSnapshot(
              MakeSingleTileArray(valid_data,
                                  std::numeric_limits<double>::infinity(), 0.0),
              &snapshot_id)
          .IsSuccess());
  EXPECT_FALSE(agent
                   ->loadSnapshot(MakeSingleTileArray(
                                      valid_data, 0.0,
                                      std::numeric_limits<double>::quiet_NaN()),
                                  &snapshot_id)
                   .IsSuccess());

  // Snapshot with cullRect overflowing int32_t bounds when rounded out.
  {
    SkPictureRecorder recorder;
    SkCanvas* canvas =
        recorder.beginRecording(SkRect::MakeLTRB(-2e9f, -2e9f, 2e9f, 2e9f));
    SkPaint paint;
    canvas->drawRect(SkRect::MakeWH(10, 10), paint);
    sk_sp<SkPicture> picture = recorder.finishRecordingAsPicture();
    sk_sp<SkData> oversized_data = picture->serialize();
    ASSERT_TRUE(oversized_data);
    String oversized_id;
    EXPECT_FALSE(
        agent->loadSnapshot(MakeSingleTileArray(oversized_data), &oversized_id)
            .IsSuccess());
  }

  // Two finite tiles whose union overflows float.
  {
    auto tiles = MakeSingleTileArray(valid_data, -3e38, 0.0);
    tiles->emplace_back(
        LayerTree::PictureTile::create()
            .setX(3e38)
            .setY(0.0)
            .setPicture(Binary::fromSpan(SkDataSpan(valid_data)))
            .build());
    String union_id;
    EXPECT_FALSE(agent->loadSnapshot(std::move(tiles), &union_id).IsSuccess());
  }

  ASSERT_TRUE(agent->loadSnapshot(MakeSingleTileArray(valid_data), &snapshot_id)
                  .IsSuccess());

  String data_url;
  EXPECT_FALSE(
      agent->replaySnapshot(snapshot_id, -1, std::nullopt, 1.0, &data_url)
          .IsSuccess());
  EXPECT_FALSE(
      agent->replaySnapshot(snapshot_id, std::nullopt, -1, 1.0, &data_url)
          .IsSuccess());
  EXPECT_FALSE(agent
                   ->replaySnapshot(snapshot_id, std::nullopt, std::nullopt,
                                    0.0, &data_url)
                   .IsSuccess());
  EXPECT_FALSE(agent
                   ->replaySnapshot(snapshot_id, std::nullopt, std::nullopt,
                                    -1.0, &data_url)
                   .IsSuccess());
  EXPECT_FALSE(agent
                   ->replaySnapshot(snapshot_id, std::nullopt, std::nullopt,
                                    1e10, &data_url)
                   .IsSuccess());
  EXPECT_FALSE(agent
                   ->replaySnapshot(snapshot_id, std::nullopt, std::nullopt,
                                    std::numeric_limits<double>::quiet_NaN(),
                                    &data_url)
                   .IsSuccess());
  EXPECT_FALSE(agent
                   ->replaySnapshot(snapshot_id, std::nullopt, std::nullopt,
                                    std::numeric_limits<double>::infinity(),
                                    &data_url)
                   .IsSuccess());

  std::unique_ptr<Array<Array<double>>> timings;
  EXPECT_FALSE(agent->profileSnapshot(snapshot_id, 0, 0.0, nullptr, &timings)
                   .IsSuccess());
  EXPECT_FALSE(agent->profileSnapshot(snapshot_id, -1, 0.0, nullptr, &timings)
                   .IsSuccess());
  EXPECT_FALSE(agent->profileSnapshot(snapshot_id, 1, -1.0, nullptr, &timings)
                   .IsSuccess());
  EXPECT_FALSE(agent
                   ->profileSnapshot(snapshot_id, 1,
                                     std::numeric_limits<double>::infinity(),
                                     nullptr, &timings)
                   .IsSuccess());
}

}  // namespace
}  // namespace protocol
}  // namespace blink
