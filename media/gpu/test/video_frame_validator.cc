// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/test/video_frame_validator.h"

#include "base/bind.h"
#include "base/files/file.h"
#include "base/hash/md5.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "media/base/video_frame.h"
#include "media/gpu/buildflags.h"
#include "media/gpu/test/video_decode_accelerator_unittest_helpers.h"
#include "media/gpu/video_frame_mapper.h"
#include "media/gpu/video_frame_mapper_factory.h"

namespace media {
namespace test {

VideoFrameValidator::MismatchedFrameInfo::MismatchedFrameInfo(
    size_t frame_index,
    std::string computed_md5,
    std::string expected_md5)
    : validate_mode(ValidateMode::MD5),
      frame_index(frame_index),
      computed_md5(std::move(computed_md5)),
      expected_md5(std::move(expected_md5)) {}

VideoFrameValidator::MismatchedFrameInfo::MismatchedFrameInfo(
    size_t frame_index,
    size_t diff_cnt)
    : validate_mode(ValidateMode::RAW),
      frame_index(frame_index),
      diff_cnt(diff_cnt) {}

VideoFrameValidator::MismatchedFrameInfo::~MismatchedFrameInfo() = default;

// static
std::unique_ptr<VideoFrameValidator> VideoFrameValidator::Create(
    const std::vector<std::string>& expected_frame_checksums,
    const VideoPixelFormat validation_format,
    std::unique_ptr<VideoFrameProcessor> corrupt_frame_processor) {
  auto video_frame_validator = base::WrapUnique(
      new VideoFrameValidator(expected_frame_checksums, validation_format,
                              std::move(corrupt_frame_processor)));
  if (!video_frame_validator->Initialize()) {
    LOG(ERROR) << "Failed to initialize VideoFrameValidator.";
    return nullptr;
  }

  return video_frame_validator;
}

std::unique_ptr<VideoFrameValidator> VideoFrameValidator::Create(
    const std::vector<scoped_refptr<const VideoFrame>> model_frames,
    const uint8_t tolerance,
    std::unique_ptr<VideoFrameProcessor> corrupt_frame_processor) {
  auto video_frame_validator = base::WrapUnique(new VideoFrameValidator(
      std::move(model_frames), tolerance, std::move(corrupt_frame_processor)));
  if (!video_frame_validator->Initialize()) {
    LOG(ERROR) << "Failed to initialize VideoFrameValidator.";
    return nullptr;
  }

  return video_frame_validator;
}

VideoFrameValidator::VideoFrameValidator(
    std::vector<std::string> expected_frame_checksums,
    VideoPixelFormat validation_format,
    std::unique_ptr<VideoFrameProcessor> corrupt_frame_processor)
    : validate_mode_(ValidateMode::MD5),
      expected_frame_checksums_(std::move(expected_frame_checksums)),
      validation_format_(validation_format),
      corrupt_frame_processor_(std::move(corrupt_frame_processor)),
      num_frames_validating_(0),
      frame_validator_thread_("FrameValidatorThread"),
      frame_validator_cv_(&frame_validator_lock_) {
  DETACH_FROM_SEQUENCE(validator_sequence_checker_);
  DETACH_FROM_SEQUENCE(validator_thread_sequence_checker_);
}

VideoFrameValidator::VideoFrameValidator(
    const std::vector<scoped_refptr<const VideoFrame>> model_frames,
    const uint8_t tolerance,
    std::unique_ptr<VideoFrameProcessor> corrupt_frame_processor)
    : validate_mode_(ValidateMode::RAW),
      model_frames_(std::move(model_frames)),
      tolerance_(tolerance),
      corrupt_frame_processor_(std::move(corrupt_frame_processor)),
      num_frames_validating_(0),
      frame_validator_thread_("FrameValidatorThread"),
      frame_validator_cv_(&frame_validator_lock_) {
  DETACH_FROM_SEQUENCE(validator_sequence_checker_);
  DETACH_FROM_SEQUENCE(validator_thread_sequence_checker_);
}

VideoFrameValidator::~VideoFrameValidator() {
  Destroy();
}

bool VideoFrameValidator::Initialize() {
  if (!frame_validator_thread_.Start()) {
    LOG(ERROR) << "Failed to start frame validator thread";
    return false;
  }
  return true;
}

void VideoFrameValidator::Destroy() {
  frame_validator_thread_.Stop();
  base::AutoLock auto_lock(frame_validator_lock_);
  DCHECK_EQ(0u, num_frames_validating_);
}

std::vector<VideoFrameValidator::MismatchedFrameInfo>
VideoFrameValidator::GetMismatchedFramesInfo() const {
  base::AutoLock auto_lock(frame_validator_lock_);
  return mismatched_frames_;
}

size_t VideoFrameValidator::GetMismatchedFramesCount() const {
  base::AutoLock auto_lock(frame_validator_lock_);
  return mismatched_frames_.size();
}

void VideoFrameValidator::ProcessVideoFrame(
    scoped_refptr<const VideoFrame> video_frame,
    size_t frame_index) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(validator_sequence_checker_);

  if (!video_frame) {
    LOG(ERROR) << "Video frame is nullptr";
    return;
  }

  base::AutoLock auto_lock(frame_validator_lock_);
  num_frames_validating_++;

  // Unretained is safe here, as we should not destroy the validator while there
  // are still frames being validated.
  frame_validator_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&VideoFrameValidator::ProcessVideoFrameTask,
                     base::Unretained(this), video_frame, frame_index));
}

bool VideoFrameValidator::WaitUntilDone() {
  base::AutoLock auto_lock(frame_validator_lock_);
  while (num_frames_validating_ > 0) {
    frame_validator_cv_.Wait();
  }

  if (corrupt_frame_processor_ && !corrupt_frame_processor_->WaitUntilDone())
    return false;

  if (mismatched_frames_.size() > 0u) {
    LOG(ERROR) << mismatched_frames_.size() << " frames failed to validate.";
    return false;
  }
  return true;
}

void VideoFrameValidator::ProcessVideoFrameTask(
    const scoped_refptr<const VideoFrame> video_frame,
    size_t frame_index) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(validator_thread_sequence_checker_);

  scoped_refptr<const VideoFrame> validated_frame = video_frame;
  // If this is a DMABuf-backed memory frame we need to map it before accessing.
#if BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)
  if (validated_frame->storage_type() == VideoFrame::STORAGE_DMABUFS ||
      validated_frame->storage_type() ==
          VideoFrame::STORAGE_GPU_MEMORY_BUFFER) {
    // Create VideoFrameMapper if not yet created. The decoder's output pixel
    // format is not known yet when creating the VideoFrameValidator. We can
    // only create the VideoFrameMapper upon receiving the first video frame.
    if (!video_frame_mapper_) {
      video_frame_mapper_ = VideoFrameMapperFactory::CreateMapper(
          video_frame->format(), video_frame->storage_type());
      ASSERT_TRUE(video_frame_mapper_) << "Failed to create VideoFrameMapper";
    }

    validated_frame = video_frame_mapper_->Map(std::move(validated_frame));
    if (!validated_frame) {
      LOG(ERROR) << "Failed to map video frame";
      return;
    }
  }
#endif  // BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)

  ASSERT_TRUE(validated_frame->IsMappable());

  base::Optional<MismatchedFrameInfo> mismatched_info;
  switch (validate_mode_) {
    case ValidateMode::MD5: {
      if (validated_frame->format() != validation_format_) {
        validated_frame =
            ConvertVideoFrame(validated_frame.get(), validation_format_);
      }
      ASSERT_TRUE(validated_frame);
      mismatched_info = ValidateMD5(*validated_frame, frame_index);
      break;
    }
    case ValidateMode::RAW:
      mismatched_info = ValidateRaw(*validated_frame, frame_index);
      break;
  }

  base::AutoLock auto_lock(frame_validator_lock_);

  if (mismatched_info) {
    mismatched_frames_.push_back(std::move(mismatched_info).value());
    // Perform additional processing on the corrupt video frame if requested.
    if (corrupt_frame_processor_)
      corrupt_frame_processor_->ProcessVideoFrame(validated_frame, frame_index);
  }

  num_frames_validating_--;
  frame_validator_cv_.Signal();
}

std::string VideoFrameValidator::ComputeMD5FromVideoFrame(
    const VideoFrame& video_frame) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(validator_thread_sequence_checker_);
  base::MD5Context context;
  base::MD5Init(&context);
  VideoFrame::HashFrameForTesting(&context, video_frame);
  base::MD5Digest digest;
  base::MD5Final(&digest, &context);
  return MD5DigestToBase16(digest);
}

base::Optional<VideoFrameValidator::MismatchedFrameInfo>
VideoFrameValidator::ValidateMD5(const VideoFrame& validated_frame,
                                 size_t frame_index) {
  std::string computed_md5 = ComputeMD5FromVideoFrame(validated_frame);

  if (expected_frame_checksums_.size() > 0) {
    LOG_IF(FATAL, frame_index >= expected_frame_checksums_.size())
        << "Frame number is over than the number of read md5 values in file.";
    const auto& expected_md5 = expected_frame_checksums_[frame_index];
    if (computed_md5 != expected_md5) {
      return MismatchedFrameInfo{frame_index, computed_md5, expected_md5};
    }
  }
  return base::nullopt;
}

base::Optional<VideoFrameValidator::MismatchedFrameInfo>
VideoFrameValidator::ValidateRaw(const VideoFrame& validated_frame,
                                 size_t frame_index) {
  if (model_frames_.size() > 0) {
    LOG_IF(FATAL, frame_index >= model_frames_.size())
        << "Frame number is over than the number of given frames.";
    size_t diff_cnt = CompareFramesWithErrorDiff(
        validated_frame, *model_frames_[frame_index], tolerance_);
    if (diff_cnt > 0)
      return MismatchedFrameInfo{frame_index, diff_cnt};
  }
  return base::nullopt;
}
}  // namespace test
}  // namespace media
