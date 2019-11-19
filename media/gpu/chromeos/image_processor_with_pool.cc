// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/chromeos/image_processor_with_pool.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "media/gpu/chromeos/gpu_buffer_layout.h"
#include "media/gpu/macros.h"

namespace media {

// static
std::unique_ptr<ImageProcessorWithPool> ImageProcessorWithPool::Create(
    std::unique_ptr<ImageProcessor> image_processor,
    DmabufVideoFramePool* const frame_pool,
    size_t num_frames,
    const scoped_refptr<base::SequencedTaskRunner> task_runner) {
  const ImageProcessor::PortConfig& config = image_processor->output_config();
  base::Optional<GpuBufferLayout> layout = frame_pool->RequestFrames(
      config.fourcc, config.size, gfx::Rect(config.visible_size), config.size,
      num_frames);
  if (!layout || layout->size() != config.size) {
    VLOGF(1) << "Failed to request frame with correct size. "
             << config.size.ToString() << " != "
             << (layout ? layout->size().ToString() : gfx::Size().ToString());
    return nullptr;
  }

  return base::WrapUnique<ImageProcessorWithPool>(new ImageProcessorWithPool(
      std::move(image_processor), frame_pool, std::move(task_runner)));
}

ImageProcessorWithPool::ImageProcessorWithPool(
    std::unique_ptr<ImageProcessor> image_processor,
    DmabufVideoFramePool* const frame_pool,
    const scoped_refptr<base::SequencedTaskRunner> task_runner)
    : image_processor_(std::move(image_processor)),
      frame_pool_(frame_pool),
      task_runner_(std::move(task_runner)) {
  DVLOGF(3);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  weak_this_ = weak_this_factory_.GetWeakPtr();
}

ImageProcessorWithPool::~ImageProcessorWithPool() {
  DVLOGF(3);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void ImageProcessorWithPool::Reset() {
  DVLOGF(4);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  image_processor_->Reset();
  pending_frames_ = {};
  num_frames_in_ip_ = 0;

  // Cancel pending tasks to avoid returning previous frames to the client.
  weak_this_factory_.InvalidateWeakPtrs();
  weak_this_ = weak_this_factory_.GetWeakPtr();
}

bool ImageProcessorWithPool::HasPendingFrames() const {
  DVLOGF(4);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return !pending_frames_.empty() || num_frames_in_ip_ > 0;
}

void ImageProcessorWithPool::Process(scoped_refptr<VideoFrame> frame,
                                     FrameReadyCB ready_cb) {
  DVLOGF(4);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  pending_frames_.push(std::make_pair(std::move(frame), std::move(ready_cb)));
  PumpProcessFrames();
}

void ImageProcessorWithPool::PumpProcessFrames() {
  DVLOGF(4);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  while (!pending_frames_.empty()) {
    scoped_refptr<VideoFrame> output_frame = frame_pool_->GetFrame();
    if (!output_frame) {
      // Notify when pool is available.
      frame_pool_->NotifyWhenFrameAvailable(base::BindOnce(
          base::IgnoreResult(&base::SequencedTaskRunner::PostTask),
          task_runner_, FROM_HERE,
          base::BindOnce(&ImageProcessorWithPool::PumpProcessFrames,
                         weak_this_)));
      break;
    }

    scoped_refptr<VideoFrame> input_frame =
        std::move(pending_frames_.front().first);
    FrameReadyCB ready_cb = std::move(pending_frames_.front().second);
    pending_frames_.pop();

    ++num_frames_in_ip_;
    image_processor_->Process(
        std::move(input_frame), std::move(output_frame),
        base::BindOnce(&ImageProcessorWithPool::OnFrameProcessed, weak_this_,
                       std::move(ready_cb)));
  }
}

void ImageProcessorWithPool::OnFrameProcessed(FrameReadyCB ready_cb,
                                              scoped_refptr<VideoFrame> frame) {
  DVLOGF(4);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_NE(num_frames_in_ip_, 0u);

  --num_frames_in_ip_;
  std::move(ready_cb).Run(std::move(frame));
}

}  // namespace media
