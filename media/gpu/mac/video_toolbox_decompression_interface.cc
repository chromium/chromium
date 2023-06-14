// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/mac/video_toolbox_decompression_interface.h"

#include <memory>
#include <tuple>

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/logging.h"
#include "base/mac/mac_logging.h"
#include "media/base/media_log.h"
#include "media/gpu/mac/video_toolbox_decompression_session.h"

namespace media {

VideoToolboxDecompressionInterface::VideoToolboxDecompressionInterface(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    std::unique_ptr<MediaLog> media_log,
    OutputCB output_cb,
    ErrorCB error_cb)
    : task_runner_(std::move(task_runner)),
      media_log_(std::move(media_log)),
      output_cb_(std::move(output_cb)),
      error_cb_(std::move(error_cb)) {
  DVLOG(1) << __func__;
  DCHECK(error_cb_);
  weak_this_ = weak_this_factory_.GetWeakPtr();
  decompression_session_ =
      std::make_unique<VideoToolboxDecompressionSessionImpl>(
          task_runner_, media_log_->Clone(),
          base::BindRepeating(&VideoToolboxDecompressionInterface::OnOutput,
                              weak_this_));
}

VideoToolboxDecompressionInterface::~VideoToolboxDecompressionInterface() {
  DVLOG(1) << __func__;
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
}

void VideoToolboxDecompressionInterface::Decode(
    base::ScopedCFTypeRef<CMSampleBufferRef> sample,
    void* context) {
  DVLOG(3) << __func__;
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  if (!error_cb_) {
    return;
  }

  pending_decodes_.push(std::make_pair(std::move(sample), context));

  if (!ProcessDecodes()) {
    NotifyError(DecoderStatus::Codes::kPlatformDecodeFailure);
    return;
  }
}

void VideoToolboxDecompressionInterface::Reset() {
  DVLOG(2) << __func__;
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  if (!error_cb_) {
    return;
  }

  pending_decodes_ = {};

  DestroySession();
}

size_t VideoToolboxDecompressionInterface::PendingDecodes() {
  DVLOG(4) << __func__;
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  return pending_decodes_.size() + active_decodes_;
}

void VideoToolboxDecompressionInterface::NotifyError(DecoderStatus status) {
  DVLOG(1) << __func__;
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(error_cb_);

  Reset();

  // We may still be executing inside Decode() and don't want to make a
  // re-entrant call.
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VideoToolboxDecompressionInterface::CallErrorCB,
                     weak_this_, std::move(error_cb_), std::move(status)));
}

void VideoToolboxDecompressionInterface::CallErrorCB(ErrorCB error_cb,
                                                     DecoderStatus status) {
  DVLOG(4) << __func__;
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  std::move(error_cb).Run(std::move(status));
}

bool VideoToolboxDecompressionInterface::ProcessDecodes() {
  DVLOG(4) << __func__;
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(error_cb_);

  if (draining_) {
    return true;
  }

  while (!pending_decodes_.empty()) {
    base::ScopedCFTypeRef<CMSampleBufferRef>& sample =
        pending_decodes_.front().first;
    void* context = pending_decodes_.front().second;

    CMFormatDescriptionRef format = CMSampleBufferGetFormatDescription(sample);

    // Handle format changes.
    if (decompression_session_->IsValid() && format != active_format_) {
      if (decompression_session_->CanAcceptFormat(format)) {
        active_format_.reset(format, base::scoped_policy::RETAIN);
      } else {
        // Destroy the active session so that it can be replaced.
        if (active_decodes_) {
          // Wait for the active session to drain before destroying it.
          draining_ = true;
          return true;
        }
        DestroySession();
      }
    }

    // Create a new session if necessary.
    if (!decompression_session_->IsValid()) {
      if (!CreateSession(format)) {
        return false;
      }
    }

    // Submit the sample for decoding.
    if (!decompression_session_->DecodeFrame(sample, context)) {
      return false;
    }

    pending_decodes_.pop();
    ++active_decodes_;
  }

  return true;
}

bool VideoToolboxDecompressionInterface::CreateSession(
    CMFormatDescriptionRef format) {
  DVLOG(2) << __func__;
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(!decompression_session_->IsValid());

  base::ScopedCFTypeRef<CFMutableDictionaryRef> decoder_config(
      CFDictionaryCreateMutable(kCFAllocatorDefault,
                                1,  // capacity
                                &kCFTypeDictionaryKeyCallBacks,
                                &kCFTypeDictionaryValueCallBacks));
  if (!decoder_config) {
    DLOG(ERROR) << "CFDictionaryCreateMutable() failed";
    MEDIA_LOG(ERROR, media_log_.get()) << "CFDictionaryCreateMutable() failed";
    return false;
  }

#if BUILDFLAG(IS_MAC)
  CFDictionarySetValue(
      decoder_config,
      kVTVideoDecoderSpecification_EnableHardwareAcceleratedVideoDecoder,
      kCFBooleanTrue);
  CFDictionarySetValue(
      decoder_config,
      kVTVideoDecoderSpecification_RequireHardwareAcceleratedVideoDecoder,
      kCFBooleanTrue);
#endif

  if (!decompression_session_->Create(format, decoder_config)) {
    return false;
  }

  active_format_.reset(format, base::scoped_policy::RETAIN);
  return true;
}

void VideoToolboxDecompressionInterface::DestroySession() {
  DVLOG(2) << __func__;
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  if (!decompression_session_->IsValid()) {
    return;
  }

  decompression_session_->Invalidate();
  active_format_.reset();
  active_decodes_ = 0;
  draining_ = false;
}

void VideoToolboxDecompressionInterface::OnOutput(
    void* context,
    OSStatus status,
    VTDecodeInfoFlags flags,
    base::ScopedCFTypeRef<CVImageBufferRef> image) {
  DVLOG(4) << __func__;
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  if (!error_cb_) {
    return;
  }

  if (status != noErr) {
    OSSTATUS_DLOG(ERROR, status) << "VTDecompressionOutputCallback";
    OSSTATUS_MEDIA_LOG(ERROR, status, media_log_.get())
        << "VTDecompressionOutputCallback";
    NotifyError(DecoderStatus::Codes::kPlatformDecodeFailure);
    return;
  }

  if (!image || CFGetTypeID(image) != CVPixelBufferGetTypeID()) {
    DLOG(ERROR) << "Decoded image is not a CVPixelBuffer";
    MEDIA_LOG(ERROR, media_log_.get())
        << "Decoded image is not a CVPixelBuffer";
    // TODO(crbug.com/1331597): Potentially allow intentional dropped frames.
    // (signaled in |flags|). It might make sense to dump without crashing to
    // help track down why this happens.
    NotifyError(DecoderStatus::Codes::kPlatformDecodeFailure);
    return;
  }

  --active_decodes_;
  DCHECK_GE(active_decodes_, 0);

  // If we are draining and the session is now empty, complete the drain.
  if (draining_ && !active_decodes_) {
    DestroySession();
    if (!ProcessDecodes()) {
      NotifyError(DecoderStatus::Codes::kPlatformDecodeFailure);
      return;
    }
  }

  // OnOutput() was posted, so this is never re-entrant.
  output_cb_.Run(std::move(image), context);
}

void VideoToolboxDecompressionInterface::SetDecompressionSessionForTesting(
    std::unique_ptr<VideoToolboxDecompressionSession> decompression_session) {
  decompression_session_ = std::move(decompression_session);
}

}  // namespace media
