// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/image-decoders/ico/ico_rust_image_decoder.h"

#include "base/check.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/skia/experimental/rust_ico/decoder/SkIcoRustDecoder.h"
#include "third_party/skia/include/core/SkStream.h"

namespace blink {

IcoRustImageDecoder::~IcoRustImageDecoder() = default;

String IcoRustImageDecoder::FilenameExtension() const {
  return "ico";
}

const AtomicString& IcoRustImageDecoder::MimeType() const {
  DEFINE_STATIC_LOCAL(const AtomicString, ico_mime_type,
                      (AtomicString::FromLiteral<"image/x-icon">()));
  return ico_mime_type;
}

gfx::Size IcoRustImageDecoder::FrameSizeAtIndex(wtf_size_t index) const {
  if (!index) {
    // Use the stable default representation for both layout and allocation.
    return Size();
  }
  const SkCodec* sk_codec = codec();
  SkCodec::FrameInfo frame_info;
  if (!sk_codec || !sk_codec->getFrameInfo(index, &frame_info)) {
    return Size();
  }
  return gfx::Size(frame_info.fFrameRect.width(),
                   frame_info.fFrameRect.height());
}

bool IcoRustImageDecoder::HotSpot(gfx::Point& hot_spot) const {
  const auto info = SkIcoRustDecoder::GetDefaultImageInfo(codec());
  if (!info || !info->hotSpot) {
    return false;
  }
  hot_spot = gfx::Point(info->hotSpot->x(), info->hotSpot->y());
  return true;
}

gfx::Size IcoRustImageDecoder::GetSizeFromSkCodec() const {
  const auto info = SkIcoRustDecoder::GetDefaultImageInfo(codec());
  CHECK(info);
  return gfx::Size(info->dimensions.width(), info->dimensions.height());
}

std::unique_ptr<SkCodec> IcoRustImageDecoder::OnCreateSkCodec(
    std::unique_ptr<SkStream> stream,
    SkCodec::Result* result) {
  std::unique_ptr<SkCodec> codec =
      SkIcoRustDecoder::Decode(std::move(stream), result);
  if (codec && !IsAllDataReceived()) {
    const auto info = SkIcoRustDecoder::GetDefaultImageInfo(codec.get());
    CHECK(info);
    if (!info->hasCompatibleFrame) {
      // Retry with more data rather than retaining an unusable snapshot.
      *result = SkCodec::kIncompleteInput;
      return nullptr;
    }
  }
  return codec;
}

}  // namespace blink
