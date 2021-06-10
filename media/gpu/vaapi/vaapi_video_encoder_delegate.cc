// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi/vaapi_video_encoder_delegate.h"

#include <va/va.h>

#include "base/memory/ref_counted_memory.h"
#include "media/base/video_frame.h"
#include "media/gpu/codec_picture.h"
#include "media/gpu/gpu_video_encode_accelerator_helpers.h"
#include "media/gpu/vaapi/va_surface.h"
#include "media/gpu/vaapi/vaapi_utils.h"
#include "media/gpu/vaapi/vaapi_wrapper.h"
#include "media/video/video_encode_accelerator.h"

namespace media {

VaapiVideoEncoderDelegate::EncodeJob::EncodeJob(
    scoped_refptr<VideoFrame> input_frame,
    bool keyframe,
    base::OnceClosure execute_cb,
    scoped_refptr<VASurface> input_surface,
    scoped_refptr<CodecPicture> picture,
    std::unique_ptr<ScopedVABuffer> coded_buffer)
    : input_frame_(input_frame),
      timestamp_(input_frame->timestamp()),
      keyframe_(keyframe),
      input_surface_(input_surface),
      picture_(std::move(picture)),
      coded_buffer_(std::move(coded_buffer)),
      execute_callback_(std::move(execute_cb)) {
  DCHECK(input_surface_);
  DCHECK(picture_);
  DCHECK(coded_buffer_);
  DCHECK(!execute_callback_.is_null());
}

VaapiVideoEncoderDelegate::EncodeJob::EncodeJob(
    scoped_refptr<VideoFrame> input_frame,
    bool keyframe,
    base::OnceClosure execute_cb)
    : input_frame_(input_frame),
      timestamp_(input_frame->timestamp()),
      keyframe_(keyframe),
      execute_callback_(std::move(execute_cb)) {
  DCHECK(!execute_callback_.is_null());
}

VaapiVideoEncoderDelegate::EncodeJob::~EncodeJob() = default;

void VaapiVideoEncoderDelegate::EncodeJob::AddSetupCallback(
    base::OnceClosure cb) {
  DCHECK(!cb.is_null());
  setup_callbacks_.push(std::move(cb));
}

void VaapiVideoEncoderDelegate::EncodeJob::AddPostExecuteCallback(
    base::OnceClosure cb) {
  DCHECK(!cb.is_null());
  post_execute_callbacks_.push(std::move(cb));
}

void VaapiVideoEncoderDelegate::EncodeJob::AddReferencePicture(
    scoped_refptr<CodecPicture> ref_pic) {
  DCHECK(ref_pic);
  reference_pictures_.push_back(ref_pic);
}

void VaapiVideoEncoderDelegate::EncodeJob::Execute() {
  while (!setup_callbacks_.empty()) {
    std::move(setup_callbacks_.front()).Run();
    setup_callbacks_.pop();
  }

  std::move(execute_callback_).Run();

  while (!post_execute_callbacks_.empty()) {
    std::move(post_execute_callbacks_.front()).Run();
    post_execute_callbacks_.pop();
  }
}

VABufferID VaapiVideoEncoderDelegate::EncodeJob::coded_buffer_id() const {
  return coded_buffer_->id();
}

const scoped_refptr<VASurface>&
VaapiVideoEncoderDelegate::EncodeJob::input_surface() const {
  return input_surface_;
}

const scoped_refptr<CodecPicture>&
VaapiVideoEncoderDelegate::EncodeJob::picture() const {
  return picture_;
}

VaapiVideoEncoderDelegate::VaapiVideoEncoderDelegate(
    scoped_refptr<VaapiWrapper> vaapi_wrapper,
    base::RepeatingClosure error_cb)
    : vaapi_wrapper_(vaapi_wrapper), error_cb_(error_cb) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

VaapiVideoEncoderDelegate::~VaapiVideoEncoderDelegate() = default;

size_t VaapiVideoEncoderDelegate::GetBitstreamBufferSize() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return GetEncodeBitstreamBufferSize(GetCodedSize());
}

void VaapiVideoEncoderDelegate::BitrateControlUpdate(
    uint64_t encoded_chunk_size_bytes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  NOTREACHED()
      << __func__ << "() is called to on an"
      << "VaapiVideoEncoderDelegate that doesn't support BitrateControl"
      << "::kConstantQuantizationParameter";
}

BitstreamBufferMetadata VaapiVideoEncoderDelegate::GetMetadata(
    EncodeJob* encode_job,
    size_t payload_size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return BitstreamBufferMetadata(
      payload_size, encode_job->IsKeyframeRequested(), encode_job->timestamp());
}

void VaapiVideoEncoderDelegate::SubmitBuffer(
    VABufferType type,
    scoped_refptr<base::RefCountedBytes> buffer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!vaapi_wrapper_->SubmitBuffer(type, buffer->size(), buffer->front()))
    error_cb_.Run();
}

void VaapiVideoEncoderDelegate::SubmitVAEncMiscParamBuffer(
    VAEncMiscParameterType type,
    scoped_refptr<base::RefCountedBytes> buffer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const size_t temp_size = sizeof(VAEncMiscParameterBuffer) + buffer->size();
  std::vector<uint8_t> temp(temp_size);

  auto* const va_buffer =
      reinterpret_cast<VAEncMiscParameterBuffer*>(temp.data());
  va_buffer->type = type;
  memcpy(va_buffer->data, buffer->front(), buffer->size());

  if (!vaapi_wrapper_->SubmitBuffer(VAEncMiscParameterBufferType, temp_size,
                                    temp.data())) {
    error_cb_.Run();
  }
}

}  // namespace media
