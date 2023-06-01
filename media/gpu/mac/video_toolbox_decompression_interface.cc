// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/mac/video_toolbox_decompression_interface.h"

#include <tuple>

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/logging.h"
#include "base/mac/mac_logging.h"
#include "base/threading/platform_thread.h"

namespace media {

namespace {

// VTDecompressionOutputCallback implementation. May be called on any thread.
void OnOutputThunk(void* decompression_output_refcon,
                   void* source_frame_refcon,
                   OSStatus status,
                   VTDecodeInfoFlags info_flags,
                   CVImageBufferRef image_buffer,
                   CMTime presentation_time_stamp,
                   CMTime presentation_duration) {
  DVLOG(4) << __func__;

  VideoToolboxDecompressionInterface* self =
      reinterpret_cast<VideoToolboxDecompressionInterface*>(
          decompression_output_refcon);

  self->OnOutputOnAnyThread(source_frame_refcon, status, info_flags,
                            base::ScopedCFTypeRef<CVImageBufferRef>(
                                image_buffer, base::scoped_policy::RETAIN));
}

}  // namespace

VideoToolboxDecompressionInterface::VideoToolboxDecompressionInterface(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    OutputCB output_cb,
    ErrorCB error_cb)
    : task_runner_(std::move(task_runner)),
      output_cb_(std::move(output_cb)),
      error_cb_(std::move(error_cb)) {
  DVLOG(1) << __func__;
  DCHECK(error_cb_);
  weak_this_ = weak_this_factory_.GetWeakPtr();
}

VideoToolboxDecompressionInterface::~VideoToolboxDecompressionInterface() {
  DVLOG(1) << __func__;
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DestroySession();
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
    NotifyError();
    return;
  }
}

void VideoToolboxDecompressionInterface::Reset() {
  DVLOG(2) << __func__;
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  if (!error_cb_) {
    return;
  }

  std::ignore = std::move(pending_decodes_);
  DestroySession();
}

size_t VideoToolboxDecompressionInterface::PendingDecodes() {
  DVLOG(4) << __func__;
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  return pending_decodes_.size() + active_decodes_;
}

void VideoToolboxDecompressionInterface::NotifyError() {
  DVLOG(1) << __func__;
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(error_cb_);

  Reset();

  // We may still be executing inside Decode() and don't want to make a
  // re-entrant call.
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VideoToolboxDecompressionInterface::CallErrorCB,
                     weak_this_, std::move(error_cb_)));
}

void VideoToolboxDecompressionInterface::CallErrorCB(ErrorCB error_cb) {
  DVLOG(4) << __func__;
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  std::move(error_cb).Run();
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
    if (active_session_ && format != active_format_) {
      if (VTDecompressionSessionCanAcceptFormatDescription(active_session_,
                                                           format)) {
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
    if (!active_session_) {
      if (!CreateSession(format)) {
        return false;
      }
    }

    // Submit the sample for decoding.
    VTDecodeFrameFlags decode_flags =
        kVTDecodeFrame_EnableAsynchronousDecompression;
    OSStatus status =
        VTDecompressionSessionDecodeFrame(active_session_,
                                          sample,        // sample_buffer
                                          decode_flags,  // decode_flags
                                          context,       // source_frame_refcon
                                          nullptr);      // info_flags_out
    if (status != noErr) {
      OSSTATUS_DLOG(ERROR, status) << "VTDecompressionSessionDecodeFrame()";
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
  DCHECK(!active_session_);

  base::ScopedCFTypeRef<CFMutableDictionaryRef> decoder_config(
      CFDictionaryCreateMutable(kCFAllocatorDefault,
                                1,  // capacity
                                &kCFTypeDictionaryKeyCallBacks,
                                &kCFTypeDictionaryValueCallBacks));
  if (!decoder_config) {
    DLOG(ERROR) << "CFDictionaryCreateMutable() failed";
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

  VTDecompressionOutputCallbackRecord callback = {OnOutputThunk, this};

  OSStatus status = VTDecompressionSessionCreate(
      kCFAllocatorDefault,
      format,          // video_format_description
      decoder_config,  // video_decoder_specification
      nullptr,         // destination_image_buffer_attributes
      &callback,       // output_callback
      active_session_.InitializeInto());
  if (status != noErr) {
    OSSTATUS_DLOG(ERROR, status) << "VTDecompressionSessionCreate()";
    return false;
  }

  active_format_.reset(format, base::scoped_policy::RETAIN);
  return true;
}

void VideoToolboxDecompressionInterface::DestroySession() {
  DVLOG(2) << __func__;
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  if (!active_session_) {
    return;
  }

  // Untested assumption: after VTDecompressionSessionInvalidate() returns,
  // OnOutputThunk() will not be called again.
  VTDecompressionSessionInvalidate(active_session_);

  // Drop in-flight OnOutput() tasks. Reassignment of |weak_this_| is safe
  // because OnOutputOnAnyThread() will not be called again until we create a
  // new session.
  weak_this_factory_.InvalidateWeakPtrs();
  weak_this_ = weak_this_factory_.GetWeakPtr();

  active_session_.reset();
  active_format_.reset();
  active_decodes_ = 0;
  draining_ = false;
}

void VideoToolboxDecompressionInterface::OnOutputOnAnyThread(
    void* context,
    OSStatus status,
    VTDecodeInfoFlags info_flags,
    base::ScopedCFTypeRef<CVImageBufferRef> image) {
  DVLOG(4) << __func__ << " tid=" << base::PlatformThread::CurrentId();
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VideoToolboxDecompressionInterface::OnOutput, weak_this_,
                     context, status, info_flags, std::move(image)));
}

void VideoToolboxDecompressionInterface::OnOutput(
    void* context,
    OSStatus status,
    VTDecodeInfoFlags info_flags,
    base::ScopedCFTypeRef<CVImageBufferRef> image) {
  DVLOG(4) << __func__ << " tid=" << base::PlatformThread::CurrentId();
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  if (!error_cb_) {
    return;
  }

  if (status != noErr) {
    OSSTATUS_DLOG(ERROR, status) << "VTDecompressionOutputCallback";
    NotifyError();
    return;
  }

  if (!image || CFGetTypeID(image) != CVPixelBufferGetTypeID()) {
    DLOG(ERROR) << "Decoded image is not a CVPixelBuffer";
    // TODO(crbug.com/1331597): Potentially allow intentional dropped frames.
    // (signaled in |info_flags|). It might make sense to dump without crashing
    // to help track down why this happens.
    NotifyError();
    return;
  }

  --active_decodes_;
  DCHECK_GE(active_decodes_, 0);

  // If we are draining and the session is now empty, complete the drain.
  if (draining_ && !active_decodes_) {
    DestroySession();
    if (!ProcessDecodes()) {
      NotifyError();
      return;
    }
  }

  // OnOutput() was posted, so this is never re-entrant.
  output_cb_.Run(std::move(image), context);
}

}  // namespace media
