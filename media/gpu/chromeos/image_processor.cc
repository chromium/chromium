// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/chromeos/image_processor.h"

#include <memory>
#include <ostream>
#include <sstream>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "media/base/video_types.h"
#include "media/gpu/macros.h"

namespace media {

namespace {

using PortConfig = ImageProcessorBackend::PortConfig;

// Verify if the format of |frame| matches |config|.
template <class FrameType>
bool CheckFrameFormat(const ImageProcessor::PortConfig& config,
                      const FrameType& frame) {
  // Because propriatary format fourcc will map to other common VideoPixelFormat
  // with same layout, we convert to VideoPixelFormat to check.
  if (frame.format() != config.fourcc.ToVideoPixelFormat()) {
    VLOGF(1) << "Invalid frame format="
             << VideoPixelFormatToString(frame.format())
             << ", expected=" << config.fourcc.ToString();
    return false;
  }

  if (frame.layout().coded_size() != config.size) {
    VLOGF(1) << "Invalid frame size=" << frame.layout().coded_size().ToString()
             << ", expected=" << config.size.ToString();
    return false;
  }

  if (frame.visible_rect() != config.visible_rect) {
    VLOGF(1) << "Invalid frame visible rectangle="
             << frame.visible_rect().ToString()
             << ", expected=" << config.visible_rect.ToString();
    return false;
  }

  return true;
}

}  // namespace

ImageProcessor::ClientCallback::ClientCallback(FrameReadyCB ready_cb)
    : ready_cb(std::move(ready_cb)) {}
ImageProcessor::ClientCallback::ClientCallback(FrameResourceReadyCB ready_cb)
    : frame_resource_ready_cb(std::move(ready_cb)) {}
ImageProcessor::ClientCallback::ClientCallback(
    LegacyFrameResourceReadyCB legacy_frame_resource_ready_cb)
    : legacy_frame_resource_ready_cb(
          std::move(legacy_frame_resource_ready_cb)) {}
ImageProcessor::ClientCallback::ClientCallback(ClientCallback&&) = default;
ImageProcessor::ClientCallback::~ClientCallback() = default;

// static
std::unique_ptr<ImageProcessor> ImageProcessor::Create(
    CreateBackendCB create_backend_cb,
    const PortConfig& input_config,
    const PortConfig& output_config,
    OutputMode output_mode,
    ErrorCB error_cb,
    scoped_refptr<base::SequencedTaskRunner> client_task_runner) {
  auto wrapped_error_cb = base::BindRepeating(
      base::IgnoreResult(&base::SequencedTaskRunner::PostTask),
      client_task_runner, FROM_HERE, std::move(error_cb));
  std::unique_ptr<ImageProcessorBackend> backend = create_backend_cb.Run(
      input_config, output_config, output_mode, std::move(wrapped_error_cb));
  if (!backend)
    return nullptr;

  scoped_refptr<base::SequencedTaskRunner> backend_task_runner =
      backend->task_runner();
  return base::WrapUnique(new ImageProcessor(std::move(backend),
                                             std::move(client_task_runner),
                                             std::move(backend_task_runner)));
}

ImageProcessor::ImageProcessor(
    std::unique_ptr<ImageProcessorBackend> backend,
    scoped_refptr<base::SequencedTaskRunner> client_task_runner,
    scoped_refptr<base::SequencedTaskRunner> backend_task_runner)
    : backend_(std::move(backend)),
      client_task_runner_(std::move(client_task_runner)),
      backend_task_runner_(std::move(backend_task_runner)),
      needs_linear_output_buffers_(backend_ &&
                                   backend_->needs_linear_output_buffers()) {
  DVLOGF(2);
  DETACH_FROM_SEQUENCE(client_sequence_checker_);

  weak_this_ = weak_this_factory_.GetWeakPtr();
}

ImageProcessor::~ImageProcessor() {
  DVLOGF(3);
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);

  weak_this_factory_.InvalidateWeakPtrs();

  // Delete |backend_| on |backend_task_runner_|.
  backend_task_runner_->DeleteSoon(FROM_HERE, std::move(backend_));
}

bool ImageProcessor::Process(scoped_refptr<VideoFrame> input_frame,
                             scoped_refptr<VideoFrame> output_frame,
                             FrameReadyCB cb) {
  DVLOGF(4);
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);
  DCHECK_EQ(output_mode(), OutputMode::IMPORT);
  DCHECK(input_frame);
  DCHECK(output_frame);

  if (!CheckFrameFormat(input_config(), *input_frame)) {
    LOG(ERROR) << "Unexpected input VideoFrame format "
               << input_frame->AsHumanReadableString()
               << ", expected a compatible one with "
               << input_config().ToString();
    return false;
  }
  if (!CheckFrameFormat(output_config(), *output_frame)) {
    LOG(ERROR) << "Unexpected output VideoFrame format "
               << output_frame->AsHumanReadableString()
               << ", expected a compatible one with "
               << output_config().ToString();
    return false;
  }

  int cb_index = StoreCallback(std::move(cb));
  auto ready_cb = base::BindOnce(&ImageProcessor::OnProcessDoneThunk,
                                 client_task_runner_, weak_this_, cb_index);
  backend_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&ImageProcessorBackend::Process,
                     base::Unretained(backend_.get()), std::move(input_frame),
                     std::move(output_frame), std::move(ready_cb)));
  return true;
}

// static
void ImageProcessor::OnProcessDoneThunk(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    std::optional<base::WeakPtr<ImageProcessor>> weak_this,
    int cb_index,
    scoped_refptr<VideoFrame> frame) {
  DVLOGF(4);
  DCHECK(weak_this);

  task_runner->PostTask(
      FROM_HERE, base::BindOnce(&ImageProcessor::OnProcessDone, *weak_this,
                                cb_index, std::move(frame)));
}

void ImageProcessor::OnProcessDone(int cb_index,
                                   scoped_refptr<VideoFrame> frame) {
  DVLOGF(4);
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);

  auto it = pending_cbs_.find(cb_index);
  // Skip if the callback is dropped by Reset().
  if (it == pending_cbs_.end())
    return;

  DCHECK(it->second.ready_cb);
  FrameReadyCB cb = std::move(it->second.ready_cb);
  pending_cbs_.erase(it);

  std::move(cb).Run(std::move(frame));
}

bool ImageProcessor::Process(scoped_refptr<FrameResource> input_frame,
                             scoped_refptr<FrameResource> output_frame,
                             FrameResourceReadyCB cb) {
  DVLOGF(4);
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);
  DCHECK_EQ(output_mode(), OutputMode::IMPORT);
  DCHECK(input_frame);
  DCHECK(output_frame);

  if (!CheckFrameFormat(input_config(), *input_frame)) {
    LOG(ERROR) << "Unexpected input FrameResource format "
               << input_frame->AsHumanReadableString()
               << ", expected a compatible one with "
               << input_config().ToString();
    return false;
  }
  if (!CheckFrameFormat(output_config(), *output_frame)) {
    LOG(ERROR) << "Unexpected output FrameResource format "
               << output_frame->AsHumanReadableString()
               << ", expected a compatible one with "
               << output_config().ToString();
    return false;
  }

  int cb_index = StoreCallback(std::move(cb));
  auto ready_cb =
      base::BindOnce(&ImageProcessor::OnProcessFrameResourceDoneThunk,
                     client_task_runner_, weak_this_, cb_index);
  backend_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&ImageProcessorBackend::ProcessFrame,
                     base::Unretained(backend_.get()), std::move(input_frame),
                     std::move(output_frame), std::move(ready_cb)));
  return true;
}

// static
void ImageProcessor::OnProcessFrameResourceDoneThunk(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    std::optional<base::WeakPtr<ImageProcessor>> weak_this,
    int cb_index,
    scoped_refptr<FrameResource> frame) {
  DVLOGF(4);
  DCHECK(weak_this);

  task_runner->PostTask(
      FROM_HERE, base::BindOnce(&ImageProcessor::OnProcessFrameResourceDone,
                                *weak_this, cb_index, std::move(frame)));
}

void ImageProcessor::OnProcessFrameResourceDone(
    int cb_index,
    scoped_refptr<FrameResource> frame) {
  DVLOGF(4);
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);

  auto it = pending_cbs_.find(cb_index);
  // Skip if the callback is dropped by Reset().
  if (it == pending_cbs_.end()) {
    return;
  }

  DCHECK(it->second.frame_resource_ready_cb);
  FrameResourceReadyCB cb = std::move(it->second.frame_resource_ready_cb);
  pending_cbs_.erase(it);

  std::move(cb).Run(std::move(frame));
}

bool ImageProcessor::Process(scoped_refptr<FrameResource> frame,
                             LegacyFrameResourceReadyCB cb) {
  DVLOGF(4);
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);
  DCHECK_EQ(output_mode(), OutputMode::ALLOCATE);

  int cb_index = StoreCallback(std::move(cb));
  auto ready_cb =
      base::BindOnce(&ImageProcessor::OnProcessFrameResourceLegacyDoneThunk,
                     client_task_runner_, weak_this_, cb_index);
  backend_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&ImageProcessorBackend::ProcessLegacyFrame,
                                base::Unretained(backend_.get()),
                                std::move(frame), std::move(ready_cb)));
  return true;
}

// static
void ImageProcessor::OnProcessFrameResourceLegacyDoneThunk(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    std::optional<base::WeakPtr<ImageProcessor>> weak_this,
    int cb_index,
    size_t buffer_id,
    scoped_refptr<FrameResource> frame) {
  DVLOGF(4);
  DCHECK(weak_this);

  task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&ImageProcessor::OnProcessFrameResourceLegacyDone,
                     *weak_this, cb_index, buffer_id, std::move(frame)));
}

void ImageProcessor::OnProcessFrameResourceLegacyDone(
    int cb_index,
    size_t buffer_id,
    scoped_refptr<FrameResource> frame) {
  DVLOGF(4);
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);

  auto it = pending_cbs_.find(cb_index);
  // Skip if the callback is dropped by Reset().
  if (it == pending_cbs_.end()) {
    return;
  }

  DCHECK(it->second.legacy_frame_resource_ready_cb);
  LegacyFrameResourceReadyCB cb =
      std::move(it->second.legacy_frame_resource_ready_cb);
  pending_cbs_.erase(it);

  std::move(cb).Run(buffer_id, std::move(frame));
}

bool ImageProcessor::Reset() {
  DVLOGF(3);
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);

  backend_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&ImageProcessorBackend::Reset,
                                base::Unretained(backend_.get())));

  // After clearing all pending callbacks, we can guarantee no frame are
  // returned after that.
  pending_cbs_.clear();

  return true;
}

int ImageProcessor::StoreCallback(ClientCallback cb) {
  DVLOGF(4);
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);

  int cb_index = next_cb_index_++;
  pending_cbs_.emplace(cb_index, std::move(cb));
  return cb_index;
}

const PortConfig& ImageProcessor::input_config() const {
  return backend_->input_config();
}

const PortConfig& ImageProcessor::output_config() const {
  return backend_->output_config();
}

}  // namespace media
