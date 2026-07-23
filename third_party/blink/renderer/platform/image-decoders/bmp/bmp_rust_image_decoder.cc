// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/image-decoders/bmp/bmp_rust_image_decoder.h"

#include "third_party/blink/renderer/platform/image-decoders/segment_reader.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/skia/include/codec/SkBmpRustDecoder.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkStream.h"

namespace blink {

BmpRustImageDecoder::~BmpRustImageDecoder() {
  if (complete_data_stream_) {
    // The complete-data path owns the SegmentStream outside `codec_`; clear the
    // base raw pointer before destroying that stream.
    SetFailed();
  }
}

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
  complete_data_stream_.reset();

  // For complete small BMPs, feed Skia's Rust BMP codec a memory stream. This
  // lets the Rust bridge bypass SkStreamAdapter + BufReader setup, which is the
  // dominant fixed cost for p25/p50-sized BMP decodes. Do not do this for
  // partial data: the codec must keep reading from SegmentStream as more bytes
  // arrive.
  constexpr size_t kInMemoryDecodeThreshold = 4 * 1024;
  if (IsAllDataReceived() && stream && stream->hasLength() &&
      stream->getLength() <= kInMemoryDecodeThreshold) {
    const size_t length = stream->getLength();
    sk_sp<const SkData> data = data_->GetAsSkData();
    if (data && data->size() == length) {
      if (std::unique_ptr<SkCodec> codec =
              SkBmpRustDecoder::Decode(std::move(data), result)) {
        complete_data_stream_ = std::move(stream);
        return codec;
      }
      return nullptr;
    }
  }

  std::unique_ptr<SkCodec> codec =
      SkBmpRustDecoder::Decode(std::move(stream), result);
  return codec;
}

}  // namespace blink
