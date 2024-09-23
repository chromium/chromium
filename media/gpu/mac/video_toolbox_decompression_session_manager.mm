// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/mac/video_toolbox_decompression_session_manager.h"

#include <Foundation/Foundation.h>

#include <memory>

#include "base/apple/bridging.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "media/base/media_log.h"
#include "media/base/video_types.h"
#include "media/gpu/mac/video_toolbox_decompression_metadata.h"
#include "media/gpu/mac/video_toolbox_decompression_session.h"

using base::apple::CFToNSPtrCast;
using base::apple::NSToCFPtrCast;

namespace media {

VideoToolboxDecompressionSessionManager::
    VideoToolboxDecompressionSessionManager(
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
          base::BindRepeating(
              &VideoToolboxDecompressionSessionManager::OnOutput, weak_this_));
}

VideoToolboxDecompressionSessionManager::
    ~VideoToolboxDecompressionSessionManager() {
  DVLOG(1) << __func__;
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
}

void VideoToolboxDecompressionSessionManager::Decode(
    base::apple::ScopedCFTypeRef<CMSampleBufferRef> sample,
    std::unique_ptr<VideoToolboxDecodeMetadata> metadata) {
  DVLOG(3) << __func__;
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  if (has_error_) {
    return;
  }

  pending_decodes_.emplace(std::move(sample), std::move(metadata));

  if (!Process()) {
    NotifyError(DecoderStatus::Codes::kPlatformDecodeFailure);
    return;
  }
}

void VideoToolboxDecompressionSessionManager::Reset() {
  DVLOG(2) << __func__;
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  if (has_error_) {
    return;
  }

  pending_decodes_ = {};

  // Discard active decodes when they complete. In most cases this is faster
  // than destroying the session.
  for (auto& it : active_decodes_) {
    it.second->discard = true;
  }

  // If we are draining, it means that there was a pending decode with a
  // different format. Since that was erased, there is no need to drain.
  draining_ = false;
}

size_t VideoToolboxDecompressionSessionManager::NumDecodes() {
  DVLOG(4) << __func__;
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  size_t num_decodes = pending_decodes_.size();

  // Only non-discarded decodes are counted because the caller won't be
  // notified when discarded decodes complete.
  for (auto& it : active_decodes_) {
    if (!it.second->discard) {
      ++num_decodes;
    }
  }

  return num_decodes;
}

void VideoToolboxDecompressionSessionManager::NotifyError(
    DecoderStatus status) {
  DVLOG(1) << __func__;
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(!has_error_);

  has_error_ = true;
  pending_decodes_ = {};
  DestroySession();

  // We may still be executing inside Decode() and don't want to make a
  // re-entrant call.
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VideoToolboxDecompressionSessionManager::CallErrorCB,
                     weak_this_, std::move(error_cb_), std::move(status)));
}

void VideoToolboxDecompressionSessionManager::CallErrorCB(
    ErrorCB error_cb,
    DecoderStatus status) {
  DVLOG(4) << __func__;
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  std::move(error_cb).Run(std::move(status));
}

bool VideoToolboxDecompressionSessionManager::Process() {
  DVLOG(4) << __func__;
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(!has_error_);

  if (draining_) {
    return true;
  }

  while (!pending_decodes_.empty()) {
    base::apple::ScopedCFTypeRef<CMSampleBufferRef>& sample =
        pending_decodes_.front().first;
    std::unique_ptr<VideoToolboxDecodeMetadata>& metadata =
        pending_decodes_.front().second;

    CMFormatDescriptionRef format =
        CMSampleBufferGetFormatDescription(sample.get());

    // Handle format changes.
    if (decompression_session_->IsValid() && format != active_format_.get()) {
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
      if (!CreateSession(format, metadata->session_metadata)) {
        return false;
      }
    }

    // Submit the sample for decoding.
    uintptr_t context = reinterpret_cast<uintptr_t>(metadata.get());
    if (!decompression_session_->DecodeFrame(sample.get(), context)) {
      return false;
    }

    // Update state. The pop() must come second because it destructs `metadata`.
    active_decodes_[context] = std::move(metadata);
    pending_decodes_.pop();
  }

  return true;
}

bool VideoToolboxDecompressionSessionManager::CreateSession(
    CMFormatDescriptionRef format,
    const VideoToolboxDecompressionSessionMetadata& session_metadata) {
  DVLOG(2) << __func__;
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(!decompression_session_->IsValid());

  // Build video decoder specification.
  NSDictionary* decoder_config = nil;
#if BUILDFLAG(IS_MAC)
  decoder_config = @{
    CFToNSPtrCast(
        kVTVideoDecoderSpecification_EnableHardwareAcceleratedVideoDecoder) :
        @YES,
    CFToNSPtrCast(
        kVTVideoDecoderSpecification_RequireHardwareAcceleratedVideoDecoder) :
            !session_metadata.allow_software_decoding
        ? @YES
        : @NO
  };
#else
  decoder_config = @{};
#endif

  // Build destination image buffer attributes.
  // TODO(crbug.com/40227557): Also set size using the visible rect.

  // It is possible to create a decompression session with no destination image
  // buffer attributes, but then we must be able to handle any kind of pixel
  // format that VideoToolbox can produce, and there is no definitive list.
  //
  // Some formats that have been seen include:
  //   - 12-bit YUV: 'tv20', 'tv22', 'tv44'
  //   - 10-bit YUV: 'p420', 'p422', 'p444'
  //   - 8-bit YUV: '420v', '422v', '444v'
  //
  // Other plausible formats include RGB, monochrome, and versions of the above
  // with alpha (eg. 'v0a8') and/or full-range (eg. '420f').
  //
  // Rather than explicitly handling every possible format in
  // VideoToolboxFrameConverter, it may be possible to introspect the IOSurfaces
  // at run time and map them to viz formats.
  //
  // TODO(crbug.com/40227557): Do not create an image config for known-supported
  // formats, and add full-range versions as supported formats.
  FourCharCode pixel_format;

  if (session_metadata.chroma_sampling == VideoChromaSampling::k444) {
    pixel_format = session_metadata.bit_depth > 8
                       ? kCVPixelFormatType_444YpCbCr10BiPlanarVideoRange
                       : kCVPixelFormatType_444YpCbCr8BiPlanarVideoRange;
  } else if (session_metadata.chroma_sampling == VideoChromaSampling::k422) {
    pixel_format = session_metadata.bit_depth > 8
                       ? kCVPixelFormatType_422YpCbCr10BiPlanarVideoRange
                       : kCVPixelFormatType_422YpCbCr8BiPlanarVideoRange;
  } else {
    pixel_format = session_metadata.bit_depth > 8
                       ? kCVPixelFormatType_420YpCbCr10BiPlanarVideoRange
                       : kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange;
  }

  if (session_metadata.has_alpha) {
    pixel_format = kCVPixelFormatType_420YpCbCr8VideoRange_8A_TriPlanar;
  }

  NSDictionary* image_config =
      @{CFToNSPtrCast(kCVPixelBufferPixelFormatTypeKey) : @(pixel_format)};

  // Create the session.
  if (!decompression_session_->Create(format, NSToCFPtrCast(decoder_config),
                                      NSToCFPtrCast(image_config))) {
    return false;
  }

  // Update saved state.
  active_format_.reset(format, base::scoped_policy::RETAIN);

  return true;
}

void VideoToolboxDecompressionSessionManager::DestroySession() {
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

void VideoToolboxDecompressionSessionManager::OnOutput(
    uintptr_t context,
    OSStatus status,
    VTDecodeInfoFlags flags,
    base::apple::ScopedCFTypeRef<CVImageBufferRef> image) {
  DVLOG(4) << __func__;
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  if (!error_cb_) {
    return;
  }

  if (status != noErr) {
    OSSTATUS_MEDIA_LOG(ERROR, status, media_log_.get())
        << "VTDecompressionOutputCallback";
    NotifyError(DecoderStatus::Codes::kPlatformDecodeFailure);
    return;
  }

  if (flags & kVTDecodeInfo_FrameDropped) {
    CHECK(!image);
  } else if (!image || CFGetTypeID(image.get()) != CVPixelBufferGetTypeID()) {
    MEDIA_LOG(ERROR, media_log_.get())
        << "Decoded image is not a CVPixelBuffer";
    NotifyError(DecoderStatus::Codes::kPlatformDecodeFailure);
    return;
  }

  auto metadata_it = active_decodes_.find(context);
  if (metadata_it == active_decodes_.end()) {
    MEDIA_LOG(ERROR, media_log_.get()) << "Unknown decode context";
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

  if (!metadata->discard) {
    // OnOutput() was posted, so this is never re-entrant.
    output_cb_.Run(std::move(image), std::move(metadata));
  }
}

void VideoToolboxDecompressionSessionManager::SetDecompressionSessionForTesting(
    std::unique_ptr<VideoToolboxDecompressionSession> decompression_session) {
  decompression_session_ = std::move(decompression_session);
}

}  // namespace media
