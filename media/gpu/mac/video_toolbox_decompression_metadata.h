// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_MAC_VIDEO_TOOLBOX_DECOMPRESSION_METADATA_H_
#define MEDIA_GPU_MAC_VIDEO_TOOLBOX_DECOMPRESSION_METADATA_H_

#include <optional>

#include "base/apple/scoped_cftyperef.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "media/base/timestamp_constants.h"
#include "media/base/video_aspect_ratio.h"
#include "media/base/video_types.h"
#include "media/gpu/codec_picture.h"
#include "media/gpu/media_gpu_export.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/hdr_metadata.h"

namespace media {

// Metadata used when creating a VideoToolbox session.
struct MEDIA_GPU_EXPORT VideoToolboxDecompressionSessionMetadata {
  // Enables platform software decoders.
  bool allow_software_decoding = false;
  // Selects a pixel format based on bit depth.
  uint8_t bit_depth = 8;
  // Selects a pixel format based on chroma sampling.
  VideoChromaSampling chroma_sampling = VideoChromaSampling::k420;
  // Selects a pixel format with alpha.
  bool has_alpha = false;
  // Selects the output image size.
  gfx::Rect visible_rect;
};

// Metadata used when decoding a frame.
struct MEDIA_GPU_EXPORT VideoToolboxDecodeMetadata {
  VideoToolboxDecodeMetadata();
  ~VideoToolboxDecodeMetadata();

  scoped_refptr<CodecPicture> picture;

  base::TimeDelta timestamp = kNoTimestamp;
  base::TimeDelta duration = kNoTimestamp;
  VideoAspectRatio aspect_ratio;
  gfx::ColorSpace color_space;
  std::optional<gfx::HDRMetadata> hdr_metadata;

  // The frame should be dropped after decoding. Used to implement Reset().
  bool discard = false;

  // Session metadata is included in case the decoder needs to be reconfigured.
  // TODO(crbug.com/40227557): Pass separately, maybe even independently.
  VideoToolboxDecompressionSessionMetadata session_metadata;
};

}  // namespace media

#endif  // MEDIA_GPU_MAC_VIDEO_TOOLBOX_DECOMPRESSION_METADATA_H_
