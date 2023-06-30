// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/mac/video_toolbox_decompression_interface.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/mac/mac_logging.h"
#include "media/base/media_log.h"
#include "media/gpu/mac/video_toolbox_decode_metadata.h"
#include "media/gpu/mac/video_toolbox_decompression_session.h"

#define MEDIA_DLOG_ERROR(msg)                  \
  do {                                         \
    DLOG(ERROR) << msg;                        \
    MEDIA_LOG(ERROR, media_log_.get()) << msg; \
  } while (0)

#define OSSTATUS_MEDIA_DLOG_ERROR(status, msg)                  \
  do {                                                          \
    OSSTATUS_DLOG(ERROR, status) << msg;                        \
    OSSTATUS_MEDIA_LOG(ERROR, status, media_log_.get()) << msg; \
  } while (0)

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
    std::unique_ptr<VideoToolboxDecodeMetadata> metadata) {
  DVLOG(3) << __func__;
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  if (!error_cb_) {
    return;
  }

  pending_decodes_.push(std::make_pair(std::move(sample), std::move(metadata)));

  if (!Process()) {
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

size_t VideoToolboxDecompressionInterface::NumDecodes() {
  DVLOG(4) << __func__;
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  return pending_decodes_.size() + active_decodes_.size();
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

bool VideoToolboxDecompressionInterface::Process() {
  DVLOG(4) << __func__;
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(error_cb_);

  if (draining_) {
    return true;
  }

  while (!pending_decodes_.empty()) {
    base::ScopedCFTypeRef<CMSampleBufferRef>& sample =
        pending_decodes_.front().first;
    std::unique_ptr<VideoToolboxDecodeMetadata>& metadata =
        pending_decodes_.front().second;

    CMFormatDescriptionRef format = CMSampleBufferGetFormatDescription(sample);

    // Handle format changes.
    if (decompression_session_->IsValid() && format != active_format_) {
      if (decompression_session_->CanAcceptFormat(format)) {
        active_format_.reset(format, base::scoped_policy::RETAIN);
      } else {
        // Destroy the active session so that it can be replaced.
        if (!active_decodes_.empty()) {
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
    void* context = static_cast<void*>(metadata.get());
    if (!decompression_session_->DecodeFrame(sample, context)) {
      return false;
    }

    // Update state. The pop() must come second because it destructs `metadata`.
    active_decodes_[context] = std::move(metadata);
    pending_decodes_.pop();
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
    MEDIA_DLOG_ERROR("CFDictionaryCreateMutable() failed");
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
  active_decodes_.clear();
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
    OSSTATUS_MEDIA_DLOG_ERROR(status, "VTDecompressionOutputCallback");
    NotifyError(DecoderStatus::Codes::kPlatformDecodeFailure);
    return;
  }

  if (flags & kVTDecodeInfo_FrameDropped) {
    CHECK(!image);
  } else if (!image || CFGetTypeID(image) != CVPixelBufferGetTypeID()) {
    MEDIA_DLOG_ERROR("Decoded image is not a CVPixelBuffer");
    NotifyError(DecoderStatus::Codes::kPlatformDecodeFailure);
    return;
  }

  auto metadata_it = active_decodes_.find(context);
  if (metadata_it == active_decodes_.end()) {
    MEDIA_DLOG_ERROR("Unknown decode context");
    NotifyError(DecoderStatus::Codes::kPlatformDecodeFailure);
    return;
  }

  std::unique_ptr<VideoToolboxDecodeMetadata> metadata =
      std::move(metadata_it->second);

  active_decodes_.erase(metadata_it);

  // If we are draining and the session is now empty, complete the drain. This
  // happens before output so that we don't need to consider what the output
  // callback might do synchronously.
  if (draining_ && active_decodes_.empty()) {
    DestroySession();
    if (!Process()) {
      NotifyError(DecoderStatus::Codes::kPlatformDecodeFailure);
      return;
    }
  }

  // OnOutput() was posted, so this is never re-entrant.
  output_cb_.Run(std::move(image), std::move(metadata));
}

void VideoToolboxDecompressionInterface::SetDecompressionSessionForTesting(
    std::unique_ptr<VideoToolboxDecompressionSession> decompression_session) {
  decompression_session_ = std::move(decompression_session);
}

}  // namespace media
