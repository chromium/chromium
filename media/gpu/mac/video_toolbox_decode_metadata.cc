// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/mac/video_toolbox_decode_metadata.h"

namespace media {

VideoToolboxDecodeMetadata::VideoToolboxDecodeMetadata(
    scoped_refptr<CodecPicture> picture,
    base::TimeDelta timestamp)
    : picture(std::move(picture)), timestamp(timestamp) {}

VideoToolboxDecodeMetadata::~VideoToolboxDecodeMetadata() = default;

}  // namespace media
