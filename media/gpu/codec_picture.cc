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

}  // namespace media
