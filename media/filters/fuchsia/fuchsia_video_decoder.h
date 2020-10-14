// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_FUCHSIA_FUCHSIA_VIDEO_DECODER_H_
#define MEDIA_FILTERS_FUCHSIA_FUCHSIA_VIDEO_DECODER_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "media/base/media_export.h"

namespace viz {
class RasterContextProvider;
}  // namespace viz

namespace media {

class VideoDecoder;

// Creates VideoDecoder that uses fuchsia.mediacodec API. The returned
// VideoDecoder instance will only try to use hardware video codecs.
MEDIA_EXPORT std::unique_ptr<VideoDecoder> CreateFuchsiaVideoDecoder(
    scoped_refptr<viz::RasterContextProvider> raster_context_provider);

// Same as above, but also allows to enable software codecs. This is useful for
// FuchsiaVideoDecoder tests that run on systems that don't have hardware
// decoder support.
MEDIA_EXPORT std::unique_ptr<VideoDecoder> CreateFuchsiaVideoDecoderForTests(
    scoped_refptr<viz::RasterContextProvider> raster_context_provider,
    bool enable_sw_decoding);

}  // namespace media

#endif  // MEDIA_FILTERS_FUCHSIA_FUCHSIA_VIDEO_DECODER_H_
