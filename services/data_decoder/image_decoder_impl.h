// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DATA_DECODER_IMAGE_DECODER_IMPL_H_
#define SERVICES_DATA_DECODER_IMAGE_DECODER_IMPL_H_

#include <stdint.h>

#include "services/data_decoder/public/mojom/image_decoder.mojom.h"
#include "ui/gfx/geometry/size.h"

namespace data_decoder {

class ImageDecoderImpl : public mojom::ImageDecoder {
 public:
  ImageDecoderImpl();

  ImageDecoderImpl(const ImageDecoderImpl&) = delete;
  ImageDecoderImpl& operator=(const ImageDecoderImpl&) = delete;

  ~ImageDecoderImpl() override;

  // Overridden from mojom::ImageDecoder:
  void DecodeImage(mojo_base::BigBuffer encoded_data,
                   mojom::ImageCodec codec,
                   bool shrink_to_fit,
                   int64_t max_size_in_bytes,
                   const gfx::Size& desired_image_frame_size,
                   DecodeImageCallback callback) override;
  void DecodeAnimation(mojo_base::BigBuffer encoded_data,
                       bool shrink_to_fit,
                       int64_t max_size_in_bytes,
                       DecodeAnimationCallback callback) override;
};

}  // namespace data_decoder

#endif  // SERVICES_DATA_DECODER_IMAGE_DECODER_IMPL_H_
