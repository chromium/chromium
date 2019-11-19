// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_TEST_VIDEO_FRAME_VALIDATOR_H_
#define MEDIA_GPU_TEST_VIDEO_FRAME_VALIDATOR_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

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
// It maps a video frame by using VideoFrameMapper, and converts the mapped
// frame to |validation_format| to resolve pixel format differences on different
// platforms. Thereafter, it compares md5 values of the mapped and converted
// buffer with golden md5 values. The golden values are prepared in advance and
// must be identical on all platforms.
// Mapping and verification of a frame is a costly operation and will influence
// performance measurements.
class VideoFrameValidator : public VideoFrameProcessor {
 public:
  static constexpr uint8_t kDefaultTolerance = 4;
  // TODO(hiroh): Support a validation by PSNR and SSIM.
  enum class ValidateMode {
    MD5,  // Check if md5sum value of coded area of a given VideoFrame on
          // ProcessVideoFrame() matches the expected md5sum.
    RAW,  // Compare each byte of visible area of a given VideoFrame on
          // ProcessVideoFrame() with a expected VideoFrame. An absolute
          // difference equal to or less than |tolerance_| is allowed on
          // comparison.
  };

  struct MismatchedFrameInfo {
    MismatchedFrameInfo(size_t frame_index,
                        std::string computed_md5,
                        std::string expected_md5);
    MismatchedFrameInfo(size_t frame_index, size_t diff_cnt);
    ~MismatchedFrameInfo();

    ValidateMode validate_mode;
    size_t frame_index = 0;

    // variables for ValidateMode::MD5 mode.
    std::string computed_md5;
    std::string expected_md5;
    // variables for ValidateMode::ERRORDIFF mode.
    size_t diff_cnt = 0;
  };

  // Create an instance of the video frame validator. The calculated checksums
  // will be compared to the values in |expected_frame_checksums|. If no
  // checksums are provided only checksum calculation will be done.
  // |validation_format| specifies the pixel format used when calculating the
  // checksum. Pixel format conversion will be performed if required. The
  // |corrupt_frame_processor| is an optional video frame processor that will be
  // called on each frame that fails validation. Ownership of the corrupt frame
  // processor will be transferred to the frame validator.
  static std::unique_ptr<VideoFrameValidator> Create(
      const std::vector<std::string>& expected_frame_checksums,
      const VideoPixelFormat validation_format = PIXEL_FORMAT_I420,
      std::unique_ptr<VideoFrameProcessor> corrupt_frame_processor = nullptr);

  static std::unique_ptr<VideoFrameValidator> Create(
      const std::vector<scoped_refptr<const VideoFrame>> model_frames,
      const uint8_t tolerance = kDefaultTolerance,
      std::unique_ptr<VideoFrameProcessor> corrupt_frame_processor = nullptr);

  ~VideoFrameValidator() override;

  // Returns information of frames that don't match golden md5 values.
  // If there is no mismatched frame, returns an empty vector. This function is
  // thread-safe.
  std::vector<MismatchedFrameInfo> GetMismatchedFramesInfo() const;

  // Returns the number of frames that didn't match the golden md5 values. This
  // function is thread-safe.
  size_t GetMismatchedFramesCount() const;

  // Interface VideoFrameProcessor
  void ProcessVideoFrame(scoped_refptr<const VideoFrame> video_frame,
                         size_t frame_index) override;
  // Wait until all currently scheduled frame validations are done. Returns true
  // if no corrupt frames were found. This function might take a long time to
  // complete, depending on the platform.
  bool WaitUntilDone() override;

 private:
  VideoFrameValidator(
      std::vector<std::string> expected_frame_checksums,
      VideoPixelFormat validation_format,
      std::unique_ptr<VideoFrameProcessor> corrupt_frame_processor);

  VideoFrameValidator(
      const std::vector<scoped_refptr<const VideoFrame>> model_frames,
      const uint8_t tolerance,
      std::unique_ptr<VideoFrameProcessor> corrupt_frame_processor);

  // Start the frame validation thread.
  bool Initialize();
  // Stop the frame validation thread.
  void Destroy();

  // Validate the |video_frame|'s content on the |frame_validator_thread_|.
  void ProcessVideoFrameTask(const scoped_refptr<const VideoFrame> video_frame,
                             size_t frame_index);

  // Returns md5 values of video frame represented by |video_frame|.
  std::string ComputeMD5FromVideoFrame(const VideoFrame& video_frame) const;

  base::Optional<MismatchedFrameInfo> ValidateMD5(
      const VideoFrame& validated_frame,
      size_t frame_index);
  base::Optional<MismatchedFrameInfo> ValidateRaw(
      const VideoFrame& validated_frame,
      size_t frame_index);

  const ValidateMode validate_mode_;

  // Values used only if |validate_mode_| is MD5.
  // The list of expected MD5 frame checksums.
  const std::vector<std::string> expected_frame_checksums_;
  // VideoPixelFormat the VideoFrame will be converted to for validation.
  const VideoPixelFormat validation_format_ = PIXEL_FORMAT_UNKNOWN;

  // Values used only if |validate_mode_| is RAW.
  // The list of expected frames
  const std::vector<scoped_refptr<const VideoFrame>> model_frames_;
  const uint8_t tolerance_ = 0;

  std::unique_ptr<VideoFrameMapper> video_frame_mapper_;

  // An optional video frame processor that all corrupted frames will be
  // forwarded to. This can be used to e.g. write corrupted frames to disk.
  const std::unique_ptr<VideoFrameProcessor> corrupt_frame_processor_;

  // The number of frames currently queued for validation.
  size_t num_frames_validating_ GUARDED_BY(frame_validator_lock_);
  // The results of invalid frame data.
  std::vector<MismatchedFrameInfo> mismatched_frames_
      GUARDED_BY(frame_validator_lock_);

  // Thread on which video frame validation is done.
  base::Thread frame_validator_thread_;
  mutable base::Lock frame_validator_lock_;
  mutable base::ConditionVariable frame_validator_cv_;

  SEQUENCE_CHECKER(validator_sequence_checker_);
  SEQUENCE_CHECKER(validator_thread_sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(VideoFrameValidator);
};
}  // namespace test
}  // namespace media

#endif  // MEDIA_GPU_TEST_VIDEO_FRAME_VALIDATOR_H_
