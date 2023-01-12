// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/image_annotation/public/cpp/image_processor.h"

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "services/image_annotation/image_annotation_metrics.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkImage.h"
#include "ui/gfx/codec/jpeg_codec.h"

namespace image_annotation {

namespace {

// Returns a scaled version of the given bitmap.
SkBitmap ScaleImage(const SkBitmap& source, const float scale) {
  // Set up another bitmap to hold the scaled image.
  SkBitmap dest;
  dest.setInfo(source.info().makeWH(static_cast<int>(scale * source.width()),
                                    static_cast<int>(scale * source.height())));
  dest.allocPixels();
  dest.eraseColor(0);

  // Use a canvas to scale the source image onto the new bitmap.
  SkCanvas canvas(dest, SkSurfaceProps{});
  canvas.scale(scale, scale);
  canvas.drawImage(source.asImage(), 0, 0);

  return dest;
}

// Runs the given callback with the image data for a scaled and re-encoded
// version of the given bitmap.
void ScaleAndEncodeImage(scoped_refptr<base::SequencedTaskRunner> task_runner,
                         ImageProcessor::GetJpgImageDataCallback callback,
                         const SkBitmap& image,
                         const int max_pixels,
                         const int jpg_quality) {
  const int num_pixels = image.width() * image.height();
  ReportSourcePixelCount(num_pixels);

  if (num_pixels == 0) {
    task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), std::vector<uint8_t>(), 0, 0));
    return;
  }

  const SkBitmap scaled_image =
      num_pixels <= max_pixels
          ? image
          : ScaleImage(image, std::sqrt(1.0 * max_pixels / num_pixels));

  std::vector<uint8_t> encoded;
  if (!gfx::JPEGCodec::Encode(scaled_image, jpg_quality, &encoded)) {
    ReportEncodedJpegSize(0u);
    task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), std::vector<uint8_t>(), 0, 0));
    return;
  }

  ReportEncodedJpegSize(encoded.size());
  task_runner->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(encoded),
                                scaled_image.width(), scaled_image.height()));
}

}  // namespace

// static
constexpr int ImageProcessor::kMaxPixels;
// static
constexpr int ImageProcessor::kJpgQuality;

ImageProcessor::ImageProcessor(base::RepeatingCallback<SkBitmap()> get_pixels)
    : get_pixels_(std::move(get_pixels)),
      background_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT})) {}

ImageProcessor::~ImageProcessor() = default;

mojo::PendingRemote<mojom::ImageProcessor> ImageProcessor::GetPendingRemote() {
  mojo::PendingRemote<mojom::ImageProcessor> remote;
  receivers_.Add(this, remote.InitWithNewPipeAndPassReceiver());
  return remote;
}

void ImageProcessor::GetJpgImageData(GetJpgImageDataCallback callback) {
  DCHECK(base::SequencedTaskRunner::HasCurrentDefault());

  background_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&ScaleAndEncodeImage,
                                base::SequencedTaskRunner::GetCurrentDefault(),
                                std::move(callback), get_pixels_.Run(),
                                kMaxPixels, kJpgQuality));
}

}  // namespace image_annotation
