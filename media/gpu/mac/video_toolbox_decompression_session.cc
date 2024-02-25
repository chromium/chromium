// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/mac/video_toolbox_decompression_session.h"

#include "base/logging.h"
#include "media/base/media_log.h"

namespace media {

namespace {

void OnOutputThunk(void* decompression_output_refcon,
                   void* source_frame_refcon,
                   OSStatus status,
                   VTDecodeInfoFlags info_flags,
                   CVImageBufferRef image_buffer,
                   CMTime presentation_time_stamp,
                   CMTime presentation_duration) {
  VideoToolboxDecompressionSessionImpl* vtdsi =
      static_cast<VideoToolboxDecompressionSessionImpl*>(
          decompression_output_refcon);
  vtdsi->OnOutputOnAnyThread(reinterpret_cast<uintptr_t>(source_frame_refcon),
                             status, info_flags,
                             base::apple::ScopedCFTypeRef<CVImageBufferRef>(
                                 image_buffer, base::scoped_policy::RETAIN));
}

}  // namespace

VideoToolboxDecompressionSessionImpl::VideoToolboxDecompressionSessionImpl(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    std::unique_ptr<MediaLog> media_log,
    OutputCB output_cb)
    : task_runner_(std::move(task_runner)),
      media_log_(std::move(media_log)),
      output_cb_(std::move(output_cb)) {
  DVLOG(1) << __func__;
  weak_this_ = weak_this_factory_.GetWeakPtr();
}

VideoToolboxDecompressionSessionImpl::~VideoToolboxDecompressionSessionImpl() {
  DVLOG(1) << __func__;
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  Invalidate();
}

bool VideoToolboxDecompressionSessionImpl::Create(
    CMFormatDescriptionRef format,
    CFDictionaryRef decoder_config,
    CFDictionaryRef image_config) {
  DVLOG(2) << __func__;
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(!IsValid());

  VTDecompressionOutputCallbackRecord callback = {&OnOutputThunk,
                                                  static_cast<void*>(this)};

  OSStatus status = VTDecompressionSessionCreate(
      /*allocator=*/kCFAllocatorDefault,
      /*videoFormatDescription=*/format,
      /*videoDecoderSpecification=*/decoder_config,
      /*destinationImageBufferAttributes=*/image_config,
      /*outputCallback=*/&callback, session_.InitializeInto());
  if (status != noErr) {
    OSSTATUS_MEDIA_LOG(ERROR, status, media_log_.get())
        << "VTDecompressionSessionCreate()";
    DCHECK(!session_);
    return false;
  }

  DCHECK(session_);
  return true;
}

void VideoToolboxDecompressionSessionImpl::Invalidate() {
  DVLOG(2) << __func__;
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  if (!session_) {
    return;
  }

  VTDecompressionSessionInvalidate(session_.get());
  session_.reset();

  // Drop in-flight OnOutput() tasks. Reassignment of |weak_this_| is safe
  // because OnOutputOnAnyThread() will not be called again until we create a
  // new session.
  weak_this_factory_.InvalidateWeakPtrs();
  weak_this_ = weak_this_factory_.GetWeakPtr();
}

bool VideoToolboxDecompressionSessionImpl::IsValid() {
  DVLOG(4) << __func__;
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  return session_.get();
}

bool VideoToolboxDecompressionSessionImpl::CanAcceptFormat(
    CMFormatDescriptionRef format) {
  DVLOG(4) << __func__;
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  CHECK(session_);
  return VTDecompressionSessionCanAcceptFormatDescription(session_.get(),
                                                          format);
}

bool VideoToolboxDecompressionSessionImpl::DecodeFrame(CMSampleBufferRef sample,
                                                       uintptr_t context) {
  DVLOG(3) << __func__;
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  CHECK(session_);

  VTDecodeFrameFlags decode_flags =
      kVTDecodeFrame_EnableAsynchronousDecompression;

  OSStatus status = VTDecompressionSessionDecodeFrame(
      session_.get(), sample, decode_flags, reinterpret_cast<void*>(context),
      nullptr);
  if (status != noErr) {
    OSSTATUS_MEDIA_LOG(ERROR, status, media_log_.get())
        << "VTDecompressionSessionDecodeFrame()";
    return false;
  }

  return true;
}

void VideoToolboxDecompressionSessionImpl::OnOutputOnAnyThread(
    uintptr_t context,
    OSStatus status,
    VTDecodeInfoFlags flags,
    base::apple::ScopedCFTypeRef<CVImageBufferRef> image) {
  DVLOG(4) << __func__;
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VideoToolboxDecompressionSessionImpl::OnOutput,
                     weak_this_, context, status, flags, std::move(image)));
}

void VideoToolboxDecompressionSessionImpl::OnOutput(
    uintptr_t context,
    OSStatus status,
    VTDecodeInfoFlags flags,
    base::apple::ScopedCFTypeRef<CVImageBufferRef> image) {
  DVLOG(3) << __func__;
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  CHECK(session_);
  output_cb_.Run(context, status, flags, std::move(image));
}

}  // namespace media
