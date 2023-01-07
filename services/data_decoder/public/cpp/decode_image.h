// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DATA_DECODER_PUBLIC_CPP_DECODE_IMAGE_H_
#define SERVICES_DATA_DECODER_PUBLIC_CPP_DECODE_IMAGE_H_

#include <stdint.h>

#include "base/containers/span.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/data_decoder/public/mojom/data_decoder_service.mojom.h"
#include "services/data_decoder/public/mojom/image_decoder.mojom.h"

namespace gfx {
class Size;
}

namespace data_decoder {

class DataDecoder;

const uint64_t kDefaultMaxSizeInBytes = 128 * 1024 * 1024;

// DecodeImageIsolated and DecodeImage functions communicate their results over
// DecodeImageCallback.  The SkBitmap will be null on failure and non-null on
// success.
using DecodeImageCallback =
    base::OnceCallback<void(const SkBitmap& decoded_bitmap)>;

// Helper function to decode an image via the data_decoder service. For images
// with multiple frames (e.g. ico files), a frame with a size as close as
// possible to |desired_image_frame_size| is chosen (tries to take one in larger
// size if there's no precise match). Passing gfx::Size() as
// |desired_image_frame_size| is also supported and will result in chosing the
// smallest available size.
//
// Upon completion, |callback| is invoked on the calling thread TaskRunner with
// an SkBitmap argument. The SkBitmap will be null on failure and non-null on
// success.
//
// This always uses an isolated instance of the Data Decoder service. To use a
// shared instance, call the signature below which takes a DataDecoder.
void DecodeImageIsolated(base::span<const uint8_t> encoded_bytes,
                         mojom::ImageCodec codec,
                         bool shrink_to_fit,
                         uint64_t max_size_in_bytes,
                         const gfx::Size& desired_image_frame_size,
                         DecodeImageCallback callback);

// Same as above but uses |data_decoder| to potentially share a service instance
// with other operations. |callback| will only be invoked if |data_decoder| is
// still alive by the time the decode operation is complete.
void DecodeImage(DataDecoder* data_decoder,
                 base::span<const uint8_t> encoded_bytes,
                 mojom::ImageCodec codec,
                 bool shrink_to_fit,
                 uint64_t max_size_in_bytes,
                 const gfx::Size& desired_image_frame_size,
                 DecodeImageCallback callback);

// Helper function to decode an animation via the data_decoder service. Any
// image with multiple frames is considered an animation, so long as the frames
// are all the same size.
//
// This always uses an isolated instance of the Data Decoder service. To use a
// shared instance, call the signature below which takes a DataDecoder.
void DecodeAnimationIsolated(
    base::span<const uint8_t> encoded_bytes,
    bool shrink_to_fit,
    uint64_t max_size_in_bytes,
    mojom::ImageDecoder::DecodeAnimationCallback callback);

// Same as above but uses |data_decoder| to potentially share a service instance
// with other operations. |callback| will only be invoked if |data_decoder| is
// still alive by the time the decode operation is complete.
void DecodeAnimation(DataDecoder* data_decoder,
                     base::span<const uint8_t> encoded_bytes,
                     bool shrink_to_fit,
                     uint64_t max_size_in_bytes,
                     mojom::ImageDecoder::DecodeAnimationCallback callback);

}  // namespace data_decoder

#endif  // SERVICES_DATA_DECODER_PUBLIC_CPP_DECODE_IMAGE_H_
