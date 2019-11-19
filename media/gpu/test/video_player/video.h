// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_TEST_VIDEO_PLAYER_VIDEO_H_
#define MEDIA_GPU_TEST_VIDEO_PLAYER_VIDEO_H_

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "media/base/video_codecs.h"
#include "ui/gfx/geometry/size.h"

namespace media {
namespace test {

// The video class provides functionality to load video files and manage their
// properties such as video codec, number of frames, frame checksums,...
// TODO(@dstaessens):
// * Use a file stream rather than loading potentially huge files into memory.
class Video {
 public:
  Video(const base::FilePath& file_path,
        const base::FilePath& metadata_file_path);
  ~Video();

  // Load the video file from disk.
  bool Load();
  // Returns true if the video file was loaded.
  bool IsLoaded() const;

  // Get the video file path.
  const base::FilePath& FilePath() const;
  // Get the video data, will be empty if the video hasn't been loaded yet.
  const std::vector<uint8_t>& Data() const;

  // Get the video's codec.
  VideoCodec Codec() const;
  // Get the video's codec profile.
  VideoCodecProfile Profile() const;
  // Get the video frame rate.
  uint32_t FrameRate() const;
  // Get the number of frames in the video.
  uint32_t NumFrames() const;
  // Get the number of fragments in the video.
  uint32_t NumFragments() const;
  // Get the video resolution.
  gfx::Size Resolution() const;
  // Get the video duration.
  base::TimeDelta GetDuration() const;

  // Get the list of frame checksums.
  const std::vector<std::string>& FrameChecksums() const;
  // Get the list of thumbnail checksums, used by the "RenderThumbnails" test.
  // TODO(crbug.com/933632) Remove once the frame validator is supported on all
  // active platforms.
  const std::vector<std::string>& ThumbnailChecksums() const;

  // Set the default path to the test video data.
  static void SetTestDataPath(const base::FilePath& test_data_path);

 private:
  // Return the profile associated with the |profile| string.
  static VideoCodecProfile ConvertStringtoProfile(const std::string& profile);
  // Return the codec associated with the |profile|.
  static VideoCodec ConvertProfileToCodec(VideoCodecProfile profile);

  // Load metadata from the JSON file associated with the video file.
  bool LoadMetadata();
  // Return true if video metadata is already loaded.
  bool IsMetadataLoaded() const;

  // Resolve the specified |file_path|. The path can be absolute, relative to
  // the current directory, or relative to the test data path. Returns the
  // resolved path if resolving to an existing file was successful.
  base::Optional<base::FilePath> ResolveFilePath(
      const base::FilePath& file_path);

  // The path where all test video files are stored.
  // TODO(dstaessens@) Avoid using a static data path here.
  static base::FilePath test_data_path_;
  // The video file path, can be relative to the test data path.
  base::FilePath file_path_;
  // Video metadata file path, can be relative to the test data path.
  base::FilePath metadata_file_path_;

  // The video's data stream.
  std::vector<uint8_t> data_;

  // Ordered list of video frame checksums.
  std::vector<std::string> frame_checksums_;
  // List of thumbnail checksums.
  std::vector<std::string> thumbnail_checksums_;

  VideoCodecProfile profile_ = VIDEO_CODEC_PROFILE_UNKNOWN;
  VideoCodec codec_ = kUnknownVideoCodec;
  uint32_t frame_rate_ = 0;
  uint32_t num_frames_ = 0;
  uint32_t num_fragments_ = 0;
  gfx::Size resolution_;

  DISALLOW_COPY_AND_ASSIGN(Video);
};

}  // namespace test
}  // namespace media

#endif  // MEDIA_GPU_TEST_VIDEO_PLAYER_VIDEO_H_
