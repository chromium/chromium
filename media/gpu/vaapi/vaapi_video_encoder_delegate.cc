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
#include "media/gpu/vaapi/vaapi_utils.h"
#include "media/gpu/vaapi/vaapi_wrapper.h"
#include "media/video/video_encode_accelerator.h"

namespace media {

VaapiVideoEncoderDelegate::EncodeJob::EncodeJob(
    bool keyframe,
    base::TimeDelta timestamp,
    uint8_t spatial_index,
    bool end_of_picture,
    VASurfaceID input_surface_id,
    scoped_refptr<CodecPicture> picture,
    std::unique_ptr<ScopedVABuffer> coded_buffer)
    : keyframe_(keyframe),
      timestamp_(timestamp),
      spatial_index_(spatial_index),
      end_of_picture_(end_of_picture),
      input_surface_id_(input_surface_id),
      picture_(std::move(picture)),
      coded_buffer_(std::move(coded_buffer)) {
  DCHECK(picture_);
  DCHECK(coded_buffer_);
}

VaapiVideoEncoderDelegate::EncodeJob::~EncodeJob() = default;

VaapiVideoEncoderDelegate::EncodeResult
VaapiVideoEncoderDelegate::EncodeJob::CreateEncodeResult(
    const BitstreamBufferMetadata& metadata) && {
  return EncodeResult(std::move(coded_buffer_), metadata);
}

base::TimeDelta VaapiVideoEncoderDelegate::EncodeJob::timestamp() const {
  return timestamp_;
}

uint8_t VaapiVideoEncoderDelegate::EncodeJob::spatial_index() const {
  return spatial_index_;
}

bool VaapiVideoEncoderDelegate::EncodeJob::end_of_picture() const {
  return end_of_picture_;
}

VABufferID VaapiVideoEncoderDelegate::EncodeJob::coded_buffer_id() const {
  CHECK(coded_buffer_);
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
  CHECK(coded_buffer_);
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

bool VaapiVideoEncoderDelegate::Encode(EncodeJob& encode_job) {
  TRACE_EVENT0("media,gpu", "VAVEDelegate::Encode");
  PrepareEncodeJobResult result = PrepareEncodeJob(encode_job);
  if (result == PrepareEncodeJobResult::kFail) {
    VLOGF(1) << "Failed preparing an encode job";
    return false;
  }

  if (result == PrepareEncodeJobResult::kDrop) {
    // An encoder must not drop a keyframe.
    CHECK(!encode_job.IsKeyframeRequested());
    DVLOGF(3) << "Drop frame";
    encode_job.DropFrame();
    return true;
  }

  if (!vaapi_wrapper_->ExecuteAndDestroyPendingBuffers(
          encode_job.input_surface_id())) {
    VLOGF(1) << "Failed to execute encode";
    return false;
  }

  return true;
}

std::optional<VaapiVideoEncoderDelegate::EncodeResult>
VaapiVideoEncoderDelegate::GetEncodeResult(
    std::unique_ptr<EncodeJob> encode_job) {
  TRACE_EVENT0("media,gpu", "VAVEDelegate::GetEncodeResult");
  if (encode_job->IsFrameDropped()) {
    return std::make_optional<EncodeResult>(
        nullptr, BitstreamBufferMetadata::CreateForDropFrame(
                     encode_job->timestamp(), encode_job->spatial_index(),
                     encode_job->end_of_picture()));
  }

  const VASurfaceID va_surface_id = encode_job->input_surface_id();
  const uint64_t encoded_chunk_size = vaapi_wrapper_->GetEncodedChunkSize(
      encode_job->coded_buffer_id(), va_surface_id);
  if (encoded_chunk_size == 0) {
    VLOGF(1) << "Invalid encoded chunk size";
    return std::nullopt;
  }

  auto metadata = GetMetadata(*encode_job, encoded_chunk_size);
  BitrateControlUpdate(metadata);
  return std::make_optional<EncodeResult>(
      std::move(*encode_job).CreateEncodeResult(metadata));
}

}  // namespace media
