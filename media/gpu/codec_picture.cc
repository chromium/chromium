// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/codec_picture.h"

#include "media/base/decoder_buffer.h"
#include "media/base/decoder_buffer_side_data.h"

namespace media {

CodecPicture::CodecPicture() {}
CodecPicture::~CodecPicture() {}

void CodecPicture::SetDynamicHdrMetadata(
    const gfx::HDRMetadata& hdr_metadata_bitstream,
    const DecoderBuffer* decoder_buffer) {
  hdr_metadata_.Reset();
  hdr_metadata_.MergeMetadataFrom(hdr_metadata_bitstream);
  if (decoder_buffer) {
    if (auto* side_data = decoder_buffer->side_data()) {
      hdr_metadata_.MergeMetadataFrom(side_data->hdr_metadata);
    }
  }
}

void CodecPicture::SetDynamicHdrMetadata(const DecoderBuffer* decoder_buffer) {
  hdr_metadata_.Reset();
  if (decoder_buffer) {
    if (auto* side_data = decoder_buffer->side_data()) {
      hdr_metadata_.MergeMetadataFrom(side_data->hdr_metadata);
    }
  }
}

void CodecPicture::CopyCommonFieldsFrom(const CodecPicture& src) {
  bitstream_id_ = src.bitstream_id_;
  visible_rect_ = src.visible_rect_;
  colorspace_ = src.colorspace_;
  hdr_metadata_ = src.hdr_metadata_;
#if BUILDFLAG(ENABLE_PLATFORM_DOLBY_VISION)
  dolby_vision_metadata_ = src.dolby_vision_metadata_;
#endif  // BUILDFLAG(ENABLE_PLATFORM_DOLBY_VISION)
}

}  // namespace media
