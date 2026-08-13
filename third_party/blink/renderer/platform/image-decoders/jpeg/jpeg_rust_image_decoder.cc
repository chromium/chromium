// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/image-decoders/jpeg/jpeg_rust_image_decoder.h"

#include "third_party/blink/renderer/platform/image-decoders/segment_reader.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/skia/experimental/rust_jpeg/decoder/SkJpegRustDecoder.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkStream.h"
#include "third_party/skia/include/private/SkJpegMetadataDecoder.h"

namespace blink {

JpegRustImageDecoder::~JpegRustImageDecoder() = default;

String JpegRustImageDecoder::FilenameExtension() const {
  return "jpg";
}

const AtomicString& JpegRustImageDecoder::MimeType() const {
  DEFINE_STATIC_LOCAL(const AtomicString, jpeg_mime_type, ("image/jpeg"));
  return jpeg_mime_type;
}

bool JpegRustImageDecoder::GetGainmapInfoAndData(
    SkGainmapInfo& out_gainmap_info,
    scoped_refptr<SegmentReader>& out_gainmap_data) const {
  if (!data_) {
    return false;
  }

  sk_sp<const SkData> encoded_data = data_->GetAsSkData();
  if (!encoded_data) {
    return false;
  }
  auto metadata_decoder = SkJpegMetadataDecoder::Make(encoded_data);
  if (!metadata_decoder) {
    return false;
  }

  auto [gainmap_data, gainmap_info] =
      metadata_decoder->findGainmapImage(encoded_data);
  if (!gainmap_data) {
    return false;
  }
  out_gainmap_info = gainmap_info;

  out_gainmap_data = data_;
  return true;
}

std::unique_ptr<SkCodec> JpegRustImageDecoder::OnCreateSkCodec(
    std::unique_ptr<SkStream> stream,
    SkCodec::Result* result) {
  std::unique_ptr<SkCodec> codec =
      SkJpegRustDecoder::Decode(std::move(stream), result);
  return codec;
}

}  // namespace blink
