// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/data_decoder/public/cpp/decode_image.h"

#include <utility>

#include "base/debug/dump_without_crashing.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "skia/ext/skia_utils_base.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace data_decoder {

namespace {

// Helper which wraps the original `callback` while also:
// 1) owning a mojo::Remote<ImageDecoder>
// 2) measuring and recording the end-to-end duration
// 3) calculating and recording the process+ipc overhead
void OnDecodeImage(mojo::Remote<mojom::ImageDecoder> decoder,
                   DecodeImageCallback callback,
                   const std::string& uma_name_prefix,
                   base::ElapsedTimer timer,
                   base::TimeDelta image_decoding_time,
                   const SkBitmap& bitmap) {
  base::UmaHistogramMediumTimes("Security.DataDecoder.Image.DecodingTime",
                                image_decoding_time);

  base::TimeDelta end_to_end_time = timer.Elapsed();
  base::UmaHistogramMediumTimes(uma_name_prefix + ".EndToEndTime",
                                end_to_end_time);

  base::TimeDelta process_overhead = end_to_end_time - image_decoding_time;
  base::UmaHistogramMediumTimes(uma_name_prefix + ".ProcessOverhead",
                                process_overhead);

  std::move(callback).Run(bitmap);
}

// Helper which wraps the original `callback` while also owning and keeping
// alive a mojo::Remote<ImageDecoder>.
void OnDecodeImages(mojo::Remote<mojom::ImageDecoder> decoder,
                    mojom::ImageDecoder::DecodeAnimationCallback callback,
                    std::vector<mojom::AnimationFramePtr> bitmaps) {
  std::move(callback).Run(std::move(bitmaps));
}

void DecodeImageUsingServiceProcess(DataDecoder* data_decoder,
                                    base::span<const uint8_t> encoded_bytes,
                                    mojom::ImageCodec codec,
                                    bool shrink_to_fit,
                                    uint64_t max_size_in_bytes,
                                    const gfx::Size& desired_image_frame_size,
                                    DecodeImageCallback callback,
                                    const std::string& uma_name_prefix,
                                    base::ElapsedTimer timer) {
  mojo::Remote<mojom::ImageDecoder> decoder;
  data_decoder->GetService()->BindImageDecoder(
      decoder.BindNewPipeAndPassReceiver());

  // `callback` will be run exactly once. Disconnect implies no response, and
  // OnDecodeImage promptly discards the decoder preventing further disconnect
  // calls.
  auto callback_pair = base::SplitOnceCallback(std::move(callback));
  decoder.set_disconnect_handler(
      base::BindOnce(std::move(callback_pair.first), SkBitmap()));

  mojom::ImageDecoder* raw_decoder = decoder.get();
  raw_decoder->DecodeImage(encoded_bytes, codec, shrink_to_fit,
                           max_size_in_bytes, desired_image_frame_size,
                           base::BindOnce(&OnDecodeImage, std::move(decoder),
                                          std::move(callback_pair.second),
                                          uma_name_prefix, std::move(timer)));
}

}  // namespace

void DecodeImageIsolated(base::span<const uint8_t> encoded_bytes,
                         mojom::ImageCodec codec,
                         bool shrink_to_fit,
                         uint64_t max_size_in_bytes,
                         const gfx::Size& desired_image_frame_size,
                         DecodeImageCallback callback) {
  base::ElapsedTimer timer;

  // Create a new DataDecoder that we keep alive until |callback| is invoked.
  auto data_decoder = std::make_unique<DataDecoder>();
  auto* raw_decoder = data_decoder.get();
  auto wrapped_callback = base::BindOnce(
      [](std::unique_ptr<DataDecoder>, DecodeImageCallback callback,
         const SkBitmap& bitmap) { std::move(callback).Run(bitmap); },
      std::move(data_decoder), std::move(callback));

  DecodeImageUsingServiceProcess(
      raw_decoder, encoded_bytes, codec, shrink_to_fit, max_size_in_bytes,
      desired_image_frame_size, std::move(wrapped_callback),
      "Security.DataDecoder.Image.Isolated", std::move(timer));
}

void DecodeImage(DataDecoder* data_decoder,
                 base::span<const uint8_t> encoded_bytes,
                 mojom::ImageCodec codec,
                 bool shrink_to_fit,
                 uint64_t max_size_in_bytes,
                 const gfx::Size& desired_image_frame_size,
                 DecodeImageCallback callback) {
  DecodeImageUsingServiceProcess(
      data_decoder, encoded_bytes, codec, shrink_to_fit, max_size_in_bytes,
      desired_image_frame_size, std::move(callback),
      "Security.DataDecoder.Image.Reusable", base::ElapsedTimer());
}

void DecodeAnimationIsolated(
    base::span<const uint8_t> encoded_bytes,
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
                     base::span<const uint8_t> encoded_bytes,
                     bool shrink_to_fit,
                     uint64_t max_size_in_bytes,
                     mojom::ImageDecoder::DecodeAnimationCallback callback) {
  mojo::Remote<mojom::ImageDecoder> decoder;
  data_decoder->GetService()->BindImageDecoder(
      decoder.BindNewPipeAndPassReceiver());

  // `callback` will be run exactly once. Disconnect implies no response, and
  // OnDecodeImages promptly discards the decoder preventing further disconnect
  // calls.
  auto callback_pair = base::SplitOnceCallback(std::move(callback));
  decoder.set_disconnect_handler(base::BindOnce(
      std::move(callback_pair.first), std::vector<mojom::AnimationFramePtr>()));

  mojom::ImageDecoder* raw_decoder = decoder.get();
  raw_decoder->DecodeAnimation(
      encoded_bytes, shrink_to_fit, max_size_in_bytes,
      base::BindOnce(&OnDecodeImages, std::move(decoder),
                     std::move(callback_pair.second)));
}

}  // namespace data_decoder
