// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi/vaapi_video_encoder_delegate.h"

#include <va/va.h>

#include "base/memory/ref_counted_memory.h"
#include "base/trace_event/trace_event.h"
#include "media/base/media_util.h"
#include "media/base/video_frame.h"
#include "media/gpu/codec_picture.h"
#include "media/gpu/gpu_video_encode_accelerator_helpers.h"
#include "media/gpu/macros.h"
#include "media/gpu/vaapi/va_surface.h"
#include "media/gpu/vaapi/vaapi_utils.h"
#include "media/gpu/vaapi/vaapi_wrapper.h"
#include "media/video/video_encode_accelerator.h"

namespace media {

VaapiVideoEncoderDelegate::EncodeJob::EncodeJob(
    bool keyframe,
    base::TimeDelta timestamp,
    VASurfaceID input_surface_id,
    scoped_refptr<CodecPicture> picture,
    std::unique_ptr<ScopedVABuffer> coded_buffer)
    : keyframe_(keyframe),
      timestamp_(timestamp),
      input_surface_id_(input_surface_id),
      picture_(std::move(picture)),
      coded_buffer_(std::move(coded_buffer)) {
  DCHECK(picture_);
  DCHECK(coded_buffer_);
}

VaapiVideoEncoderDelegate::EncodeJob::EncodeJob(bool keyframe,
                                                base::TimeDelta timestamp,
                                                VASurfaceID input_surface_id)
    : keyframe_(keyframe),
      timestamp_(timestamp),
      input_surface_id_(input_surface_id) {}

VaapiVideoEncoderDelegate::EncodeJob::~EncodeJob() = default;

VaapiVideoEncoderDelegate::EncodeResult
VaapiVideoEncoderDelegate::EncodeJob::CreateEncodeResult(
    const BitstreamBufferMetadata& metadata) && {
  return EncodeResult(std::move(coded_buffer_), metadata);
}

base::TimeDelta VaapiVideoEncoderDelegate::EncodeJob::timestamp() const {
  return timestamp_;
}

VABufferID VaapiVideoEncoderDelegate::EncodeJob::coded_buffer_id() const {
  return coded_buffer_->id();
}

VASurfaceID VaapiVideoEncoderDelegate::EncodeJob::input_surface_id() const {
  return input_surface_id_;
}

const scoped_refptr<CodecPicture>&
VaapiVideoEncoderDelegate::EncodeJob::picture() const {
  return picture_;
}

VaapiVideoEncoderDelegate::EncodeResult::EncodeResult(
    std::unique_ptr<ScopedVABuffer> coded_buffer,
    const BitstreamBufferMetadata& metadata)
    : coded_buffer_(std::move(coded_buffer)), metadata_(metadata) {}

VaapiVideoEncoderDelegate::EncodeResult::~EncodeResult() = default;

VaapiVideoEncoderDelegate::EncodeResult::EncodeResult(EncodeResult&&) = default;

VaapiVideoEncoderDelegate::EncodeResult&
VaapiVideoEncoderDelegate::EncodeResult::operator=(EncodeResult&&) = default;

VABufferID VaapiVideoEncoderDelegate::EncodeResult::coded_buffer_id() const {
  return coded_buffer_->id();
}

const BitstreamBufferMetadata&
VaapiVideoEncoderDelegate::EncodeResult::metadata() const {
  return metadata_;
}

VaapiVideoEncoderDelegate::VaapiVideoEncoderDelegate(
    scoped_refptr<VaapiWrapper> vaapi_wrapper,
    base::RepeatingClosure error_cb)
    : vaapi_wrapper_(vaapi_wrapper), error_cb_(error_cb) {}

VaapiVideoEncoderDelegate::~VaapiVideoEncoderDelegate() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

size_t VaapiVideoEncoderDelegate::GetBitstreamBufferSize() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return GetEncodeBitstreamBufferSize(GetCodedSize());
}

void VaapiVideoEncoderDelegate::BitrateControlUpdate(
    const BitstreamBufferMetadata& metadata) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

BitstreamBufferMetadata VaapiVideoEncoderDelegate::GetMetadata(
    const EncodeJob& encode_job,
    size_t payload_size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return BitstreamBufferMetadata(payload_size, encode_job.IsKeyframeRequested(),
                                 encode_job.timestamp());
}

bool VaapiVideoEncoderDelegate::Encode(EncodeJob& encode_job) {
  TRACE_EVENT0("media,gpu", "VAVEDelegate::Encode");
  if (!PrepareEncodeJob(encode_job)) {
    VLOGF(1) << "Failed preparing an encode job";
    return false;
  }

  if (!vaapi_wrapper_->ExecuteAndDestroyPendingBuffers(
          encode_job.input_surface_id())) {
    VLOGF(1) << "Failed to execute encode";
    return false;
  }

  return true;
}

absl::optional<VaapiVideoEncoderDelegate::EncodeResult>
VaapiVideoEncoderDelegate::GetEncodeResult(
    std::unique_ptr<EncodeJob> encode_job) {
  TRACE_EVENT0("media,gpu", "VAVEDelegate::GetEncodeResult");
  const VASurfaceID va_surface_id = encode_job->input_surface_id();
  const uint64_t encoded_chunk_size = vaapi_wrapper_->GetEncodedChunkSize(
      encode_job->coded_buffer_id(), va_surface_id);
  if (encoded_chunk_size == 0) {
    VLOGF(1) << "Invalid encoded chunk size";
    return absl::nullopt;
  }

  auto metadata = GetMetadata(*encode_job, encoded_chunk_size);
  BitrateControlUpdate(metadata);
  return absl::make_optional<EncodeResult>(
      std::move(*encode_job).CreateEncodeResult(metadata));
}

}  // namespace media
