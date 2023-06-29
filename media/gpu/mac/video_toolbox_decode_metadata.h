// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_MAC_VIDEO_TOOLBOX_DECODE_METADATA_H_
#define MEDIA_GPU_MAC_VIDEO_TOOLBOX_DECODE_METADATA_H_

#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "media/gpu/codec_picture.h"
#include "media/gpu/media_gpu_export.h"

namespace media {

struct MEDIA_GPU_EXPORT VideoToolboxDecodeMetadata {
  VideoToolboxDecodeMetadata(scoped_refptr<CodecPicture> picture,
                             base::TimeDelta timestamp);
  ~VideoToolboxDecodeMetadata();

  scoped_refptr<CodecPicture> picture;
  base::TimeDelta timestamp;
};

}  // namespace media

#endif  // MEDIA_GPU_MAC_VIDEO_TOOLBOX_DECODE_METADATA_H_
