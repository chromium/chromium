// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_TEST_RAW_VIDEO_H_
#define MEDIA_GPU_TEST_RAW_VIDEO_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "media/base/video_frame_layout.h"
#include "media/base/video_types.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace base {
class MemoryMappedFile;
}  // namespace base

namespace media::test {

// RawVideo owns the raw video data (e.g. YUV) and provides the information
// about the video.
class RawVideo final {
 public:
  // The maximum number of read frames if |read_all_frames| is false.
  // If the number of frames in the file is more than this, the 61-th and later
  // frames are just ignored.
  static constexpr size_t kLimitedReadFrames = 60;

  // FrameData serves the access of the frame data.
  struct FrameData {
    FrameData(const std::vector<const uint8_t*>& plane_addrs,
              const std::vector<size_t>& strides,
              std::vector<uint8_t> buffer);
    FrameData(FrameData&& frame_data);
    ~FrameData();

    FrameData& operator=(FrameData&&) = delete;
    FrameData(const FrameData&) = delete;
    FrameData& operator=(const FrameData&) = delete;

    const std::vector<const uint8_t*> plane_addrs;
    const std::vector<size_t> strides;

   private:
    std::vector<uint8_t> buffer;
  };

  ~RawVideo();

  // Creates RawVideo by reading a compressed video from |file_path| and its
  // metadata from |metadata_file_path|. Returns nullptr on fatal.
  static std::unique_ptr<RawVideo> Create(
      const base::FilePath& file_path,
      const base::FilePath& metadata_file_path,
      bool read_all_frames);

  // Create RawVideo by converting |data_| to NV12.
  std::unique_ptr<RawVideo> CreateNV12Video() const;
  // Create RawVideo instance by copying the |data_| to |visible_rect| area and
  // expanding the resolution to |resolution|. This is only supported for
  // RawVideo that has NV12 data.
  std::unique_ptr<RawVideo> CreateExpandedVideo(
      const gfx::Size& resolution,
      const gfx::Rect& visible_rect) const;

  // Get the data of |frame_index|-th frame (0-index).
  FrameData GetFrame(size_t frame_index) const;
  VideoPixelFormat PixelFormat() const {
    return metadata_.frame_layout->format();
  }
  uint32_t FrameRate() const { return metadata_.frame_rate; }
  size_t NumFrames() const { return metadata_.num_frames; }
  const gfx::Size& Resolution() const {
    return metadata_.frame_layout->coded_size();
  }
  const VideoFrameLayout& FrameLayout() const {
    return *metadata_.frame_layout;
  }
  const gfx::Rect& VisibleRect() const { return metadata_.visible_rect; }

  // Set the default path to the test video data.
  static void SetTestDataPath(const base::FilePath& test_data_path);

 private:
  struct Metadata {
    Metadata();
    ~Metadata();
    Metadata(const Metadata&);
    Metadata& operator=(const Metadata&);

    uint32_t frame_rate;
    size_t num_frames;
    std::optional<VideoFrameLayout> frame_layout;
    gfx::Rect visible_rect;
  };
  class VP9Decoder;

  RawVideo(std::unique_ptr<VP9Decoder> vp9_decoder,
           const Metadata& metadata,
           size_t video_frame_size);
  RawVideo(std::unique_ptr<base::MemoryMappedFile> memory_mapped_file,
           const Metadata& metadata,
           size_t video_frame_size);

  static base::FilePath ResolveFilePath(const base::FilePath& file_path);
  static bool LoadMetadata(const base::FilePath& json_file_path,
                           Metadata& metadata,
                           bool& compressed_data);

  const std::unique_ptr<VP9Decoder> vp9_decoder_;
  const std::unique_ptr<base::MemoryMappedFile> memory_mapped_file_;
  const Metadata metadata_;
  const size_t video_frame_size_;

  static base::FilePath test_data_path_;
};
}  // namespace media::test

#endif  // MEDIA_GPU_TEST_RAW_VIDEO_H_
