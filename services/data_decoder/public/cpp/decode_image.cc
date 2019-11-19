// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/data_decoder/public/cpp/decode_image.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace data_decoder {

namespace {

// Helper callback which owns a mojo::Remote<ImageDecoder> until invoked. This
// keeps the ImageDecoder pipe open just long enough to dispatch a reply, at
// which point the reply is forwarded to the wrapped |callback|.
void OnDecodeImage(mojo::Remote<mojom::ImageDecoder> decoder,
                   mojom::ImageDecoder::DecodeImageCallback callback,
                   const SkBitmap& bitmap) {
  std::move(callback).Run(bitmap);
}

void OnDecodeImages(mojo::Remote<mojom::ImageDecoder> decoder,
                    mojom::ImageDecoder::DecodeAnimationCallback callback,
                    std::vector<mojom::AnimationFramePtr> bitmaps) {
  std::move(callback).Run(std::move(bitmaps));
}

}  // namespace

void DecodeImageIsolated(const std::vector<uint8_t>& encoded_bytes,
                         mojom::ImageCodec codec,
                         bool shrink_to_fit,
                         uint64_t max_size_in_bytes,
                         const gfx::Size& desired_image_frame_size,
                         mojom::ImageDecoder::DecodeImageCallback callback) {
  // Create a new DataDecoder that we keep alive until |callback| is invoked.
  auto data_decoder = std::make_unique<DataDecoder>();
  auto* raw_decoder = data_decoder.get();
  auto wrapped_callback = base::BindOnce(
      [](std::unique_ptr<DataDecoder>,
         mojom::ImageDecoder::DecodeImageCallback callback,
         const SkBitmap& bitmap) { std::move(callback).Run(bitmap); },
      std::move(data_decoder), std::move(callback));
  DecodeImage(raw_decoder, encoded_bytes, codec, shrink_to_fit,
              max_size_in_bytes, desired_image_frame_size,
              std::move(wrapped_callback));
}

void DecodeImage(DataDecoder* data_decoder,
                 const std::vector<uint8_t>& encoded_bytes,
                 mojom::ImageCodec codec,
                 bool shrink_to_fit,
                 uint64_t max_size_in_bytes,
                 const gfx::Size& desired_image_frame_size,
                 mojom::ImageDecoder::DecodeImageCallback callback) {
  mojo::Remote<mojom::ImageDecoder> decoder;
  data_decoder->GetService()->BindImageDecoder(
      decoder.BindNewPipeAndPassReceiver());

  // |call_once| runs |callback| on its first invocation.
  auto call_once = base::AdaptCallbackForRepeating(std::move(callback));
  decoder.set_disconnect_handler(base::BindOnce(call_once, SkBitmap()));

  mojom::ImageDecoder* raw_decoder = decoder.get();
  raw_decoder->DecodeImage(
      encoded_bytes, codec, shrink_to_fit, max_size_in_bytes,
      desired_image_frame_size,
      base::BindOnce(&OnDecodeImage, std::move(decoder), std::move(call_once)));
}

void DecodeAnimationIsolated(
    const std::vector<uint8_t>& encoded_bytes,
    bool shrink_to_fit,
    uint64_t max_size_in_bytes,
    mojom::ImageDecoder::DecodeAnimationCallback callback) {
  // Create a new DataDecoder that we keep alive until |callback| is invoked.
  auto decoder = std::make_unique<DataDecoder>();
  auto* raw_decoder = decoder.get();
  auto wrapped_callback = base::BindOnce(
      [](std::unique_ptr<DataDecoder>,
         mojom::ImageDecoder::DecodeAnimationCallback callback,
         std::vector<mojom::AnimationFramePtr> frames) {
        std::move(callback).Run(std::move(frames));
      },
      std::move(decoder), std::move(callback));
  DecodeAnimation(raw_decoder, encoded_bytes, shrink_to_fit, max_size_in_bytes,
                  std::move(wrapped_callback));
}

void DecodeAnimation(DataDecoder* data_decoder,
                     const std::vector<uint8_t>& encoded_bytes,
                     bool shrink_to_fit,
                     uint64_t max_size_in_bytes,
                     mojom::ImageDecoder::DecodeAnimationCallback callback) {
  mojo::Remote<mojom::ImageDecoder> decoder;
  data_decoder->GetService()->BindImageDecoder(
      decoder.BindNewPipeAndPassReceiver());

  // |call_once| runs |callback| on its first invocation.
  auto call_once = base::AdaptCallbackForRepeating(std::move(callback));
  decoder.set_disconnect_handler(base::BindOnce(
      call_once, base::Passed(std::vector<mojom::AnimationFramePtr>())));

  mojom::ImageDecoder* raw_decoder = decoder.get();
  raw_decoder->DecodeAnimation(
      encoded_bytes, shrink_to_fit, max_size_in_bytes,
      base::BindOnce(&OnDecodeImages, std::move(decoder),
                     std::move(call_once)));
}

}  // namespace data_decoder
