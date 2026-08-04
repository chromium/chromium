// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/image-decoders/ico/ico_rust_image_decoder.h"

#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/skia/experimental/rust_ico/decoder/SkIcoRustDecoder.h"
#include "third_party/skia/include/core/SkStream.h"

namespace blink {

IcoRustImageDecoder::~IcoRustImageDecoder() = default;

String IcoRustImageDecoder::FilenameExtension() const {
  return "ico";
}

const AtomicString& IcoRustImageDecoder::MimeType() const {
  DEFINE_STATIC_LOCAL(const AtomicString, ico_mime_type, ("image/x-icon"));
  return ico_mime_type;
}

std::unique_ptr<SkCodec> IcoRustImageDecoder::OnCreateSkCodec(
    std::unique_ptr<SkStream> stream,
    SkCodec::Result* result) {
  std::unique_ptr<SkCodec> codec =
      SkIcoRustDecoder::Decode(std::move(stream), result);
  return codec;
}

}  // namespace blink
