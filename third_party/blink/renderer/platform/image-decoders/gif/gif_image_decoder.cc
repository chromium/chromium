/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/image-decoders/gif/gif_image_decoder.h"

#include "third_party/blink/renderer/platform/image-decoders/segment_stream.h"
#include "third_party/skia/include/codec/SkEncodedImageFormat.h"
#include "third_party/skia/include/codec/SkGifDecoder.h"
#include "third_party/skia/include/core/SkStream.h"

namespace blink {

GIFImageDecoder::~GIFImageDecoder() = default;

String GIFImageDecoder::FilenameExtension() const {
  return "gif";
}

const AtomicString& GIFImageDecoder::MimeType() const {
  DEFINE_STATIC_LOCAL(const AtomicString, gif_mime_type, ("image/gif"));
  return gif_mime_type;
}

std::unique_ptr<SkCodec> GIFImageDecoder::OnCreateSkCodec(
    std::unique_ptr<SegmentStream> stream,
    SkCodec::Result* result) {
  std::unique_ptr<SkCodec> codec =
      SkGifDecoder::Decode(std::move(stream), result);
  if (codec) {
    CHECK_EQ(codec->getEncodedFormat(), SkEncodedImageFormat::kGIF);
  }
  return codec;
}

}  // namespace blink
