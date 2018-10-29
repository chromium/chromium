// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_TEST_VIDEO_FRAME_VALIDATOR_H_
#define MEDIA_GPU_TEST_VIDEO_FRAME_VALIDATOR_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/threading/thread_checker.h"
#include "media/base/video_frame.h"
#include "media/base/video_types.h"
#include "media/gpu/test/video_decode_accelerator_unittest_helpers.h"
#include "media/gpu/test/video_frame_mapper.h"
#include "media/gpu/test/video_frame_mapper_factory.h"
#include "ui/gfx/geometry/rect.h"

namespace media {
namespace test {

// VideoFrameValidator validates the pixel content of each video frame.
// It maps a video frame by using VideoFrameMapper, and converts the mapped
// frame to I420 format to resolve layout differences due to different pixel
// layouts/alignments on different platforms.
// Thereafter, it compares md5 values of the mapped and converted buffer with
// golden md5 values. The golden values are prepared in advance and must be
// identical on all platforms.
// Mapping and verification of a frame is a costly operation and will influence
// performance measurements.
class VideoFrameValidator {
 public:
  struct MismatchedFrameInfo {
    size_t frame_index;
    std::string computed_md5;
    std::string expected_md5;
  };

  // |prefix_output_yuv| is the prefix name of saved yuv files.
  // VideoFrameValidator saves all I420 video frames.
  // If |prefix_output_yuv_| is not specified, no yuv file will be saved.
  // |md5_file_path| is the path to the file that contains golden md5 values.
  // The file contains one md5 value per line, listed in display order.
  // Returns nullptr on failure.
  static std::unique_ptr<VideoFrameValidator> CreateVideoFrameValidator(
      const base::FilePath& prefix_output_yuv,
      const base::FilePath& md5_file_path);

  ~VideoFrameValidator();

  // This checks if |video_frame|'s pixel content is as expected.
  // A client of VideoFrameValidator would call this function on each frame in
  // PictureReady().
  // The client MUST pass video frames in display order.
  // TODO(crbug.com/856562): Specify frame index and compare the index-th video
  // frame, so that a client can call this in any order.
  void EvaluateVideoFrame(scoped_refptr<VideoFrame> video_frame);

  // Returns information of frames that don't match golden md5 values.
  // If there is no mismatched frame, returns an empty vector.
  std::vector<MismatchedFrameInfo> GetMismatchedFramesInfo() const;

 private:
  VideoFrameValidator(const base::FilePath& prefix_output_yuv,
                      std::vector<std::string> md5_of_frames,
                      std::unique_ptr<VideoFrameMapper> video_frame_mapper);

  // This maps |video_frame|, converts it to I420 format and computes the MD5
  // value of the converted I420 video frame.
  // |video_frame| is unchanged in this method.
  std::string ComputeMD5FromVideoFrame(
      scoped_refptr<VideoFrame> video_frame) const;

  // Create VideoFrame with I420 format from |src_frame|.
  scoped_refptr<VideoFrame> CreateI420Frame(
      const VideoFrame* const src_frame) const;

  // Helper function to save I420 yuv image.
  bool WriteI420ToFile(size_t frame_index,
                       const VideoFrame* const video_frame) const;

  // The results of invalid frame data.
  std::vector<MismatchedFrameInfo> mismatched_frames_;

  // Current frame index to be evaluated.
  size_t frame_index_ = 0;

  // Prefix of saved yuv files.
  const base::FilePath prefix_output_yuv_;

  // Golden MD5 values.
  const std::vector<std::string> md5_of_frames_;

  const std::unique_ptr<VideoFrameMapper> video_frame_mapper_;

  THREAD_CHECKER(thread_checker_);
  DISALLOW_COPY_AND_ASSIGN(VideoFrameValidator);
};
}  // namespace test
}  // namespace media

#endif  // MEDIA_GPU_TEST_VIDEO_FRAME_VALIDATOR_H_
