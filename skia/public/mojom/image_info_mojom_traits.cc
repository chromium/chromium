// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "skia/public/mojom/image_info_mojom_traits.h"

namespace mojo {

namespace {

SkColorType MojoColorTypeToSk(skia::mojom::ColorType type) {
  switch (type) {
    case skia::mojom::ColorType::UNKNOWN:
      return kUnknown_SkColorType;
    case skia::mojom::ColorType::ALPHA_8:
      return kAlpha_8_SkColorType;
    case skia::mojom::ColorType::RGB_565:
      return kRGB_565_SkColorType;
    case skia::mojom::ColorType::ARGB_4444:
      return kARGB_4444_SkColorType;
    case skia::mojom::ColorType::RGBA_8888:
      return kRGBA_8888_SkColorType;
    case skia::mojom::ColorType::BGRA_8888:
      return kBGRA_8888_SkColorType;
    case skia::mojom::ColorType::GRAY_8:
      return kGray_8_SkColorType;
    case skia::mojom::ColorType::INDEX_8:
      // no longer supported
      break;
  }
  NOTREACHED();
  return kUnknown_SkColorType;
}

SkAlphaType MojoAlphaTypeToSk(skia::mojom::AlphaType type) {
  switch (type) {
    case skia::mojom::AlphaType::UNKNOWN:
      return kUnknown_SkAlphaType;
    case skia::mojom::AlphaType::ALPHA_TYPE_OPAQUE:
      return kOpaque_SkAlphaType;
    case skia::mojom::AlphaType::PREMUL:
      return kPremul_SkAlphaType;
    case skia::mojom::AlphaType::UNPREMUL:
      return kUnpremul_SkAlphaType;
  }
  NOTREACHED();
  return kUnknown_SkAlphaType;
}

skia::mojom::ColorType SkColorTypeToMojo(SkColorType type) {
  switch (type) {
    case kUnknown_SkColorType:
      return skia::mojom::ColorType::UNKNOWN;
    case kAlpha_8_SkColorType:
      return skia::mojom::ColorType::ALPHA_8;
    case kRGB_565_SkColorType:
      return skia::mojom::ColorType::RGB_565;
    case kARGB_4444_SkColorType:
      return skia::mojom::ColorType::ARGB_4444;
    case kRGBA_8888_SkColorType:
      return skia::mojom::ColorType::RGBA_8888;
    case kBGRA_8888_SkColorType:
      return skia::mojom::ColorType::BGRA_8888;
    case kGray_8_SkColorType:
      return skia::mojom::ColorType::GRAY_8;
    default:
      // Skia has color types not used by Chrome.
      break;
  }
  NOTREACHED();
  return skia::mojom::ColorType::UNKNOWN;
}

skia::mojom::AlphaType SkAlphaTypeToMojo(SkAlphaType type) {
  switch (type) {
    case kUnknown_SkAlphaType:
      return skia::mojom::AlphaType::UNKNOWN;
    case kOpaque_SkAlphaType:
      return skia::mojom::AlphaType::ALPHA_TYPE_OPAQUE;
    case kPremul_SkAlphaType:
      return skia::mojom::AlphaType::PREMUL;
    case kUnpremul_SkAlphaType:
      return skia::mojom::AlphaType::UNPREMUL;
  }
  NOTREACHED();
  return skia::mojom::AlphaType::UNKNOWN;
}

}  // namespace

// static
skia::mojom::ColorType
StructTraits<skia::mojom::ImageInfoDataView, SkImageInfo>::color_type(
    const SkImageInfo& info) {
  return SkColorTypeToMojo(info.colorType());
}

// static
skia::mojom::AlphaType
StructTraits<skia::mojom::ImageInfoDataView, SkImageInfo>::alpha_type(
    const SkImageInfo& info) {
  return SkAlphaTypeToMojo(info.alphaType());
}

// static
std::vector<uint8_t>
StructTraits<skia::mojom::ImageInfoDataView,
             SkImageInfo>::serialized_color_space(const SkImageInfo& info) {
  std::vector<uint8_t> serialized_color_space;
  if (auto* sk_color_space = info.colorSpace()) {
    serialized_color_space.resize(sk_color_space->writeToMemory(nullptr));
    // Assumption 1: Since a "null" SkColorSpace is represented as an empty byte
    // array, the serialization of a non-null SkColorSpace should produce at
    // least one byte.
    CHECK_GT(serialized_color_space.size(), 0u);
    // Assumption 2: Serialized data should be reasonably small, since
    // SkImageInfo should efficiently pass through mojo message pipes. As of
    // this writing, the max would be 80 bytes. However, that could change in
    // the future. So, set an upper-bound of 1 KB here.
    CHECK_LE(serialized_color_space.size(), 1024u);
    sk_color_space->writeToMemory(serialized_color_space.data());
  } else {
    // Represent the "null" color space as an empty byte vector.
  }
  return serialized_color_space;
}

// static
uint32_t StructTraits<skia::mojom::ImageInfoDataView, SkImageInfo>::width(
    const SkImageInfo& info) {
  return info.width() < 0 ? 0 : static_cast<uint32_t>(info.width());
}

// static
uint32_t StructTraits<skia::mojom::ImageInfoDataView, SkImageInfo>::height(
    const SkImageInfo& info) {
  return info.height() < 0 ? 0 : static_cast<uint32_t>(info.height());
}

// static
bool StructTraits<skia::mojom::ImageInfoDataView, SkImageInfo>::Read(
    skia::mojom::ImageInfoDataView data,
    SkImageInfo* info) {
  mojo::ArrayDataView<uint8_t> serialized_color_space;
  data.GetSerializedColorSpaceDataView(&serialized_color_space);
  sk_sp<SkColorSpace> sk_color_space;
  if (serialized_color_space.size() != 0u) {
    sk_color_space = SkColorSpace::Deserialize(serialized_color_space.data(),
                                               serialized_color_space.size());
    // Deserialize() returns nullptr on invalid input.
    if (!sk_color_space)
      return false;
  } else {
    // Empty byte array is interpreted as "null."
  }

  *info = SkImageInfo::Make(
      data.width(), data.height(), MojoColorTypeToSk(data.color_type()),
      MojoAlphaTypeToSk(data.alpha_type()), std::move(sk_color_space));
  return true;
}

}  // namespace mojo
