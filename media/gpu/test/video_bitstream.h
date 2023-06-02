// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_TEST_VIDEO_BITSTREAM_H_
#define MEDIA_GPU_TEST_VIDEO_BITSTREAM_H_

#include <memory>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/time/time.h"
#include "media/base/video_codecs.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace base {
class MemoryMappedFile;
}  // namespace base

namespace media::test {

// VideoBitstream owns the compressed video data (e.g. h264 and vp9) and
// provides the information about the video.
class VideoBitstream final {
 public:
  // Creates VideoBitstream by reading a compressed video from |file_path| and
  // its metadata from |metadata_file_path|. Returns nullptr on fatal.
  static std::unique_ptr<VideoBitstream> Create(
      const base::FilePath& file_path,
      const base::FilePath& metadata_file_path);

  ~VideoBitstream();
  VideoBitstream(const VideoBitstream&) = delete;
  VideoBitstream& operator=(const VideoBitstream&) = delete;
  VideoBitstream(VideoBitstream&&) = delete;
  VideoBitstream& operator=(VideoBitstream&&) = delete;

  // Returns the compressed video data.
  base::span<const uint8_t> Data() const;
  VideoCodecProfile Profile() const { return metadata_.profile; }
  VideoCodec Codec() const { return metadata_.codec; }
  uint8_t BitDepth() const { return metadata_.bit_depth; }
  uint32_t FrameRate() const { return metadata_.frame_rate; }
  size_t NumFrames() const { return metadata_.num_frames; }
  const gfx::Size& Resolution() const { return metadata_.resolution; }
  const std::vector<std::string>& FrameChecksums() const {
    return metadata_.frame_checksums;
  }
  base::TimeDelta Duration() const {
    return base::Seconds(static_cast<double>(metadata_.num_frames) /
                         static_cast<double>(metadata_.frame_rate));
  }
  // Returns if the video has a resolution change event on non keyframe.
  bool HasKeyFrameLessResolutionChange() const {
    return metadata_.has_keyframeless_resolution_change;
  }

  // Set the default path to the test video data.
  static void SetTestDataPath(const base::FilePath& test_data_path);

 private:
  struct Metadata {
    Metadata();
    ~Metadata();
    Metadata(const Metadata&);
    Metadata& operator=(const Metadata&);

    VideoCodecProfile profile;
    VideoCodec codec;
    uint8_t bit_depth;
    uint32_t frame_rate;
    size_t num_frames;
    gfx::Size resolution;
    std::vector<std::string> frame_checksums;
    bool has_keyframeless_resolution_change;
  };

  VideoBitstream(std::unique_ptr<base::MemoryMappedFile> memory_mapped_file,
                 const Metadata& metadata);

  static base::FilePath ResolveFilePath(const base::FilePath& file_path);
  static bool LoadMetadata(const base::FilePath& json_file_path,
                           Metadata& metadata);

  const std::unique_ptr<base::MemoryMappedFile> memory_mapped_file_;
  const Metadata metadata_;

  static base::FilePath test_data_path_;
};
}  // namespace media::test

#endif  // MEDIA_GPU_TEST_VIDEO_BITSTREAM_H_
