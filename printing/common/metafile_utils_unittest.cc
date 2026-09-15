// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/common/metafile_utils.h"

#include <memory>

#include "base/containers/span.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "skia/ext/font_utils.h"
#include "skia/ext/skia_utils_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkFontStyle.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkStream.h"
#include "third_party/skia/include/core/SkTypeface.h"
#include "ui/gfx/skia_span_util.h"

namespace printing {

namespace {

constexpr SkFourByteTag kFontationsFactoryId =
    SkSetFourByteTag('f', 'n', 't', 'a');

}  // namespace

TEST(MetafileUtilsTest, DeserializeOopTypefaceWithFontationsEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(blink::features::kFontationsPrinting);

  sk_sp<SkTypeface> orig_typeface = skia::DefaultTypeface();
  ASSERT_TRUE(orig_typeface);

  TypefaceSerializationContext serialize_ctx;
  SkSerialProcs serial_procs =
      SerializationProcs(nullptr, &serialize_ctx, nullptr);
  sk_sp<const SkData> data =
      serial_procs.fTypefaceProc(orig_typeface.get(), &serialize_ctx);
  ASSERT_TRUE(data);

  TypefaceDeserializationContext deserialize_ctx;
  SkDeserialProcs deserial_procs =
      DeserializationProcs(nullptr, &deserialize_ctx, nullptr);
  SkMemoryStream stream(data);
  sk_sp<SkTypeface> deserialized_typeface =
      deserial_procs.fTypefaceStreamProc(stream, &deserialize_ctx);
  ASSERT_TRUE(deserialized_typeface);

  EXPECT_EQ(skia::GetTypefaceFactoryIdForTesting(deserialized_typeface.get()),
            kFontationsFactoryId);

  // Deserializing a second time with data_included=false should reuse the
  // cached typeface.
  sk_sp<const SkData> second_data =
      serial_procs.fTypefaceProc(orig_typeface.get(), &serialize_ctx);
  ASSERT_TRUE(second_data);
  SkMemoryStream second_stream(second_data);
  sk_sp<SkTypeface> cached_typeface =
      deserial_procs.fTypefaceStreamProc(second_stream, &deserialize_ctx);
  EXPECT_EQ(cached_typeface, deserialized_typeface);
}

TEST(MetafileUtilsTest, DeserializeOopTypefaceWithFontationsDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(blink::features::kFontationsPrinting);

  sk_sp<SkTypeface> orig_typeface = skia::DefaultTypeface();
  ASSERT_TRUE(orig_typeface);

  TypefaceSerializationContext serialize_ctx;
  SkSerialProcs serial_procs =
      SerializationProcs(nullptr, &serialize_ctx, nullptr);
  sk_sp<const SkData> data =
      serial_procs.fTypefaceProc(orig_typeface.get(), &serialize_ctx);
  ASSERT_TRUE(data);

  TypefaceDeserializationContext deserialize_ctx;
  SkDeserialProcs deserial_procs =
      DeserializationProcs(nullptr, &deserialize_ctx, nullptr);
  SkMemoryStream stream(data);
  sk_sp<SkTypeface> deserialized_typeface =
      deserial_procs.fTypefaceStreamProc(stream, &deserialize_ctx);
  ASSERT_TRUE(deserialized_typeface);
}

TEST(MetafileUtilsTest, DeserializeOopTypefaceMalformedFontData) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(blink::features::kFontationsPrinting);

  sk_sp<SkTypeface> orig_typeface = skia::DefaultTypeface();
  ASSERT_TRUE(orig_typeface);

  TypefaceSerializationContext serialize_ctx;
  SkSerialProcs serial_procs =
      SerializationProcs(nullptr, &serialize_ctx, nullptr);
  sk_sp<const SkData> valid_data =
      serial_procs.fTypefaceProc(orig_typeface.get(), &serialize_ctx);
  ASSERT_TRUE(valid_data);
  ASSERT_GT(valid_data->size(), 100u);

  // Corrupt font bytes in the payload by finding and overwriting the sfnt
  // header.
  sk_sp<SkData> corrupt_data =
      gfx::MakeSkDataFromSpanWithCopy(skia::as_byte_span(*valid_data));
  base::span<uint8_t> corrupt_span = skia::as_writable_byte_span(*corrupt_data);
  bool corrupted = false;
  for (size_t i = 0; i + 4 < corrupt_span.size(); ++i) {
    if ((corrupt_span[i] == 0x00 && corrupt_span[i + 1] == 0x01 &&
         corrupt_span[i + 2] == 0x00 && corrupt_span[i + 3] == 0x00) ||
        (corrupt_span[i] == 'O' && corrupt_span[i + 1] == 'T' &&
         corrupt_span[i + 2] == 'T' && corrupt_span[i + 3] == 'O') ||
        (corrupt_span[i] == 't' && corrupt_span[i + 1] == 't' &&
         corrupt_span[i + 2] == 'c' && corrupt_span[i + 3] == 'f') ||
        (corrupt_span[i] == 't' && corrupt_span[i + 1] == 'r' &&
         corrupt_span[i + 2] == 'u' && corrupt_span[i + 3] == 'e')) {
      corrupt_span[i] = 0xDE;
      corrupt_span[i + 1] = 0xAD;
      corrupt_span[i + 2] = 0xBE;
      corrupt_span[i + 3] = 0xEF;
      corrupted = true;
      break;
    }
  }
  ASSERT_TRUE(corrupted);

  SkMemoryStream stream(corrupt_data);
  TypefaceDeserializationContext deserialize_ctx;
  SkDeserialProcs deserial_procs =
      DeserializationProcs(nullptr, &deserialize_ctx, nullptr);
  sk_sp<SkTypeface> result =
      deserial_procs.fTypefaceStreamProc(stream, &deserialize_ctx);
  EXPECT_FALSE(result);

  // Truncated header data should also cleanly return nullptr.
  const uint8_t kTruncatedHeader[] = {0x01, 0x02};
  SkMemoryStream bad_stream(kTruncatedHeader, sizeof(kTruncatedHeader));
  EXPECT_FALSE(
      deserial_procs.fTypefaceStreamProc(bad_stream, &deserialize_ctx));
}

TEST(MetafileUtilsTest, MakeTypefaceWithFontationsStreamless) {
  sk_sp<SkTypeface> orig_typeface = skia::DefaultTypeface();
  ASSERT_TRUE(orig_typeface);

  // Serialize descriptor without font data.
  sk_sp<SkData> streamless_data =
      orig_typeface->serialize(SkTypeface::SerializeBehavior::kDontIncludeData);
  ASSERT_TRUE(streamless_data);

  SkMemoryStream stream(streamless_data);
  sk_sp<SkTypeface> result = skia::MakeTypefaceWithFontations(&stream);
#if BUILDFLAG(IS_MAC)
  // On macOS, streamless fonts resolve by name from system fonts via CoreText.
#else
  // On Windows, Linux, Android, and other platforms, streamless descriptors
  // are rejected in printing.
  EXPECT_FALSE(result);
#endif
}

TEST(MetafileUtilsTest, MakeTypefaceWithFontationsInvalidStream) {
  // Completely empty or truncated stream.
  const uint8_t kTruncatedBytes[] = {0x00, 0x01};
  SkMemoryStream stream(kTruncatedBytes, sizeof(kTruncatedBytes));
  EXPECT_FALSE(skia::MakeTypefaceWithFontations(&stream));
}

}  // namespace printing
