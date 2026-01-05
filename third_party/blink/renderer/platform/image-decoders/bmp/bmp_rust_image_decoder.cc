// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/image-decoders/bmp/bmp_rust_image_decoder.h"

#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/skia/experimental/rust_bmp/decoder/SkBmpRustDecoder.h"
#include "third_party/skia/include/core/SkStream.h"

namespace blink {

BmpRustImageDecoder::~BmpRustImageDecoder() = default;

String BmpRustImageDecoder::FilenameExtension() const {
  return "bmp";
}

const AtomicString& BmpRustImageDecoder::MimeType() const {
  DEFINE_STATIC_LOCAL(const AtomicString, bmp_mime_type, ("image/bmp"));
  return bmp_mime_type;
}

std::unique_ptr<SkCodec> BmpRustImageDecoder::OnCreateSkCodec(
    std::unique_ptr<SkStream> stream,
    SkCodec::Result* result) {
  std::unique_ptr<SkCodec> codec =
      SkBmpRustDecoder::Decode(std::move(stream), result);
  return codec;
}

}  // namespace blink
