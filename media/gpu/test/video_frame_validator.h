// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_TEST_VIDEO_FRAME_VALIDATOR_H_
#define MEDIA_GPU_TEST_VIDEO_FRAME_VALIDATOR_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread.h"
#include "base/threading/thread_checker.h"
#include "media/base/video_types.h"
#include "media/gpu/test/video_frame_helpers.h"
#include "ui/gfx/geometry/rect.h"

namespace media {

class VideoFrame;
class VideoFrameMapper;

namespace test {

// VideoFrameValidator validates the pixel content of each video frame.
// It maps a video frame by using VideoFrameMapper if needed. If the validation
// fails, the frame is processed by |corrupt_frame_processor_|.
// VideoFrameValidator is created by calling Create() of the child classes in
// below.
// Mapping and verification of a frame is a costly operation and will influence
// performance measurements.
class VideoFrameValidator : public VideoFrameProcessor {
 public:
  // This mode can be specified for PSNR and SSIM VideoFrameValidator.
  enum class ValidationMode {
    kThreshold,  // Each frame's quality need to pass the specified threshold.
    kAverage,    // The average quality needs to pass the specified value.
  };

  // Get the model frame from |frame_index|.
  using GetModelFrameCB =
      base::RepeatingCallback<scoped_refptr<const VideoFrame>(size_t)>;

  ~VideoFrameValidator() override;

  // Prints information of frames on which the validation failed. This function
  // is thread-safe.
  void PrintMismatchedFramesInfo() const;

  // Returns the number of frames on which the validation failed. This function
  // is thread-safe.
  size_t GetMismatchedFramesCount() const;

  // Interface VideoFrameProcessor
  void ProcessVideoFrame(scoped_refptr<const VideoFrame> video_frame,
                         size_t frame_index) final;
  // Wait until all currently scheduled frame validations are done. Returns true
  // if no corrupt frames were found. This function might take a long time to
  // complete, depending on the platform.
  bool WaitUntilDone() final;

 protected:
  struct MismatchedFrameInfo {
    MismatchedFrameInfo(size_t frame_index) : frame_index(frame_index) {}
    virtual ~MismatchedFrameInfo() = default;
    virtual void Print() const = 0;
    size_t frame_index;
  };

  VideoFrameValidator(
      std::unique_ptr<VideoFrameProcessor> corrupt_frame_processor);

  // Start the frame validation thread.
  bool Initialize();

  SEQUENCE_CHECKER(validator_thread_sequence_checker_);

 private:
  // Stop the frame validation thread.
  void Destroy();

  // Validate the |video_frame|'s content on the |frame_validator_thread_|.
  void ProcessVideoFrameTask(scoped_refptr<const VideoFrame> video_frame,
                             size_t frame_index);

  virtual std::unique_ptr<MismatchedFrameInfo> Validate(
      scoped_refptr<const VideoFrame> frame,
      size_t frame_index) = 0;

  // Returns whether the overall validation passed.
  virtual bool Passed() const;

  std::unique_ptr<VideoFrameMapper> video_frame_mapper_;

  // An optional video frame processor that all corrupted frames will be
  // forwarded to. This can be used to e.g. write corrupted frames to disk.
  const std::unique_ptr<VideoFrameProcessor> corrupt_frame_processor_;

  // The number of frames currently queued for validation.
  size_t num_frames_validating_ GUARDED_BY(frame_validator_lock_);
  // The results of invalid frame data.
  std::vector<std::unique_ptr<MismatchedFrameInfo>> mismatched_frames_
      GUARDED_BY(frame_validator_lock_);

  // Thread on which video frame validation is done.
  base::Thread frame_validator_thread_;
  mutable base::Lock frame_validator_lock_;
  mutable base::ConditionVariable frame_validator_cv_;

  SEQUENCE_CHECKER(validator_sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(VideoFrameValidator);
};

// Validate by converting the frame to be validated to |validation_format| to
// resolve pixel format differences on different platforms. Thereafter, it
// compares md5 values of the mapped and converted buffer with golden md5
// values. The golden values are prepared in advance and must be identical on
// all platforms.
class MD5VideoFrameValidator : public VideoFrameValidator {
 public:
  static std::unique_ptr<MD5VideoFrameValidator> Create(
      const std::vector<std::string>& expected_frame_checksums,
      VideoPixelFormat validation_format = PIXEL_FORMAT_I420,
      std::unique_ptr<VideoFrameProcessor> corrupt_frame_processor = nullptr);
  ~MD5VideoFrameValidator() override;

 private:
  struct MD5MismatchedFrameInfo;

  MD5VideoFrameValidator(
      const std::vector<std::string>& expected_frame_checksums,
      VideoPixelFormat validation_format,
      std::unique_ptr<VideoFrameProcessor> corrupt_frame_processor);
  MD5VideoFrameValidator(const MD5VideoFrameValidator&) = delete;
  MD5VideoFrameValidator& operator=(const MD5VideoFrameValidator&) = delete;

  // VideoFrameValidator implementation.
  std::unique_ptr<MismatchedFrameInfo> Validate(
      scoped_refptr<const VideoFrame> frame,
      size_t frame_index) override;

  // Returns md5 values of video frame represented by |video_frame|.
  std::string ComputeMD5FromVideoFrame(const VideoFrame& video_frame) const;

  const std::vector<std::string> expected_frame_checksums_;
  const VideoPixelFormat validation_format_;
};

// Validate by comparing each byte of visible area of the frame to be validated.
// An absolute difference equal to or less than |torelance_| is allowed on
// the comparison.
class RawVideoFrameValidator : public VideoFrameValidator {
 public:
  constexpr static uint8_t kDefaultTolerance = 4;

  static std::unique_ptr<RawVideoFrameValidator> Create(
      const GetModelFrameCB& get_model_frame_cb,
      std::unique_ptr<VideoFrameProcessor> corrupt_frame_processor = nullptr,
      uint8_t tolerance = kDefaultTolerance);
  ~RawVideoFrameValidator() override;

 private:
  struct RawMismatchedFrameInfo;

  RawVideoFrameValidator(
      const GetModelFrameCB& get_model_frame_cb,
      std::unique_ptr<VideoFrameProcessor> corrupt_frame_processor,
      uint8_t tolerance);

  std::unique_ptr<MismatchedFrameInfo> Validate(
      scoped_refptr<const VideoFrame> frame,
      size_t frame_index) override;

  const GetModelFrameCB get_model_frame_cb_;
  const uint8_t tolerance_;
};

// Validate by computing PSNR from the frame to be validated and the model frame
// acquired by |get_model_frame_cb_|. If the PSNR value is equal to or more than
// |tolerance_|, the validation on the frame passes.
class PSNRVideoFrameValidator : public VideoFrameValidator {
 public:
  constexpr static double kDefaultTolerance = 20.0;

  static std::unique_ptr<PSNRVideoFrameValidator> Create(
      const GetModelFrameCB& get_model_frame_cb,
      std::unique_ptr<VideoFrameProcessor> corrupt_frame_processor = nullptr,
      ValidationMode validation_mode = ValidationMode::kThreshold,
      double tolerance = kDefaultTolerance);
  const std::map<size_t, double>& GetPSNRValues() const { return psnr_; }
  ~PSNRVideoFrameValidator() override;

 private:
  struct PSNRMismatchedFrameInfo;

  PSNRVideoFrameValidator(
      const GetModelFrameCB& get_model_frame_cb,
      std::unique_ptr<VideoFrameProcessor> corrupt_frame_processor,
      ValidationMode validation_mode,
      double tolerance);

  std::unique_ptr<MismatchedFrameInfo> Validate(
      scoped_refptr<const VideoFrame> frame,
      size_t frame_index) override;

  bool Passed() const override;

  const GetModelFrameCB get_model_frame_cb_;
  const double tolerance_;
  const ValidationMode validation_mode_;
  std::map<size_t, double> psnr_;
};

// Validate by computing SSIM from the frame to be validated and the model frame
// acquired by |get_model_frame_cb_|. If the SSIM value is equal to or more than
// |tolerance_|, the validation on the frame passes.
class SSIMVideoFrameValidator : public VideoFrameValidator {
 public:
  constexpr static double kDefaultTolerance = 0.70;

  static std::unique_ptr<SSIMVideoFrameValidator> Create(
      const GetModelFrameCB& get_model_frame_cb,
      std::unique_ptr<VideoFrameProcessor> corrupt_frame_processor = nullptr,
      ValidationMode validation_mode = ValidationMode::kThreshold,
      double tolerance = kDefaultTolerance);
  const std::map<size_t, double>& GetSSIMValues() const { return ssim_; }
  ~SSIMVideoFrameValidator() override;

 private:
  struct SSIMMismatchedFrameInfo;

  SSIMVideoFrameValidator(
      const GetModelFrameCB& get_model_frame_cb,
      std::unique_ptr<VideoFrameProcessor> corrupt_frame_processor,
      ValidationMode validation_mode,
      double tolerance);

  std::unique_ptr<MismatchedFrameInfo> Validate(
      scoped_refptr<const VideoFrame> frame,
      size_t frame_index) override;

  bool Passed() const override;

  const GetModelFrameCB get_model_frame_cb_;
  const double tolerance_;
  const ValidationMode validation_mode_;
  std::map<size_t, double> ssim_;
};
}  // namespace test
}  // namespace media

#endif  // MEDIA_GPU_TEST_VIDEO_FRAME_VALIDATOR_H_
