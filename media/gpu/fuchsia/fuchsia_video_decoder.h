// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_FUCHSIA_FUCHSIA_VIDEO_DECODER_H_
#define MEDIA_GPU_FUCHSIA_FUCHSIA_VIDEO_DECODER_H_

#include <memory>

#include "media/gpu/media_gpu_export.h"

namespace media {

class VideoDecoder;

// Creates VideoDecoder that uses fuchsia.mediacodec API.
MEDIA_GPU_EXPORT std::unique_ptr<VideoDecoder> CreateFuchsiaVideoDecoder();

}  // namespace media

#endif  // MEDIA_GPU_FUCHSIA_FUCHSIA_VIDEO_DECODER_H_
