// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_FUCHSIA_FUCHSIA_VIDEO_DECODER_H_
#define MEDIA_FILTERS_FUCHSIA_FUCHSIA_VIDEO_DECODER_H_

#include <memory>

#include "media/base/media_export.h"

namespace gpu {
class ContextSupport;
class SharedImageInterface;
}  // namespace gpu

namespace media {

class VideoDecoder;

// Creates VideoDecoder that uses fuchsia.mediacodec API. The returned
// VideoDecoder instance will only try to use hardware video codecs.
// |shared_image_interface| and |gpu_context_support| must outlive the decoder.
MEDIA_EXPORT std::unique_ptr<VideoDecoder> CreateFuchsiaVideoDecoder(
    gpu::SharedImageInterface* shared_image_interface,
    gpu::ContextSupport* gpu_context_support);

// Same as above, but also allows to enable software codecs. This is useful for
// FuchsiaVideoDecoder tests that run on systems that don't have hardware
// decoder support.
MEDIA_EXPORT std::unique_ptr<VideoDecoder> CreateFuchsiaVideoDecoderForTests(
    gpu::SharedImageInterface* shared_image_interface,
    gpu::ContextSupport* gpu_context_support,
    bool enable_sw_decoding);

}  // namespace media

#endif  // MEDIA_FILTERS_FUCHSIA_FUCHSIA_VIDEO_DECODER_H_
