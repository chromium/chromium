// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DATA_DECODER_IMAGE_DECODER_IMPL_H_
#define SERVICES_DATA_DECODER_IMAGE_DECODER_IMPL_H_

#include <stdint.h>

#include "base/macros.h"
#include "services/data_decoder/public/mojom/image_decoder.mojom.h"
#include "ui/gfx/geometry/size.h"

namespace data_decoder {

class ImageDecoderImpl : public mojom::ImageDecoder {
 public:
  ImageDecoderImpl();
  ~ImageDecoderImpl() override;

  // Overridden from mojom::ImageDecoder:
  void DecodeImage(const std::vector<uint8_t>& encoded_data,
                   mojom::ImageCodec codec,
                   bool shrink_to_fit,
                   int64_t max_size_in_bytes,
                   const gfx::Size& desired_image_frame_size,
                   DecodeImageCallback callback) override;
  void DecodeAnimation(const std::vector<uint8_t>& encoded_data,
                       bool shrink_to_fit,
                       int64_t max_size_in_bytes,
                       DecodeAnimationCallback callback) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ImageDecoderImpl);
};

}  // namespace data_decoder

#endif  // SERVICES_DATA_DECODER_IMAGE_DECODER_IMPL_H_
