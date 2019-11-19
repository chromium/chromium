// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/test/video_player/video_player_test_environment.h"

#include <utility>

#include "base/system/sys_info.h"
#include "media/base/video_types.h"
#include "media/gpu/test/video_player/video.h"

namespace media {
namespace test {

// Default video to be used if no test video was specified.
constexpr base::FilePath::CharType kDefaultTestVideoPath[] =
    FILE_PATH_LITERAL("test-25fps.h264");

// static
VideoPlayerTestEnvironment* VideoPlayerTestEnvironment::Create(
    const base::FilePath& video_path,
    const base::FilePath& video_metadata_path,
    bool enable_validator,
    bool use_vd,
    const base::FilePath& output_folder,
    const FrameOutputConfig& frame_output_config) {
  auto video = std::make_unique<media::test::Video>(
      video_path.empty() ? base::FilePath(kDefaultTestVideoPath) : video_path,
      video_metadata_path);
  if (!video->Load()) {
    LOG(ERROR) << "Failed to load " << video_path;
    return nullptr;
  }

  return new VideoPlayerTestEnvironment(std::move(video), enable_validator,
                                        use_vd, output_folder,
                                        frame_output_config);
}

VideoPlayerTestEnvironment::VideoPlayerTestEnvironment(
    std::unique_ptr<media::test::Video> video,
    bool enable_validator,
    bool use_vd,
    const base::FilePath& output_folder,
    const FrameOutputConfig& frame_output_config)
    : video_(std::move(video)),
      enable_validator_(enable_validator),
      use_vd_(use_vd),
      frame_output_config_(frame_output_config),
      output_folder_(output_folder),
      gpu_memory_buffer_factory_(
          gpu::GpuMemoryBufferFactory::CreateNativeType(nullptr)) {}

VideoPlayerTestEnvironment::~VideoPlayerTestEnvironment() = default;

void VideoPlayerTestEnvironment::SetUp() {
  VideoTestEnvironment::SetUp();

  // TODO(dstaessens): Remove this check once all platforms support import mode.
  // Some older platforms do not support importing buffers, but need to allocate
  // buffers internally in the decoder.
  // Note: buddy, guado and rikku support import mode for H.264 and VP9, but for
  // VP8 they use a different video decoder (V4L2 instead of VAAPI) and don't
  // support import mode.
#if defined(OS_CHROMEOS)
  constexpr const char* kImportModeBlacklist[] = {
      "buddy",      "guado", "guado-kernelnext", "nyan_big", "nyan_blaze",
      "nyan_kitty", "rikku"};
  const std::string board = base::SysInfo::GetLsbReleaseBoard();
  import_supported_ = (std::find(std::begin(kImportModeBlacklist),
                                 std::end(kImportModeBlacklist),
                                 board) == std::end(kImportModeBlacklist));
#endif  // defined(OS_CHROMEOS)

  // VideoDecoders always require import mode to be supported.
  DCHECK(!use_vd_ || import_supported_);
}

const media::test::Video* VideoPlayerTestEnvironment::Video() const {
  return video_.get();
}

gpu::GpuMemoryBufferFactory*
VideoPlayerTestEnvironment::GetGpuMemoryBufferFactory() const {
  return gpu_memory_buffer_factory_.get();
}

bool VideoPlayerTestEnvironment::IsValidatorEnabled() const {
  return enable_validator_;
}

bool VideoPlayerTestEnvironment::UseVD() const {
  return use_vd_;
}

FrameOutputMode VideoPlayerTestEnvironment::GetFrameOutputMode() const {
  return frame_output_config_.output_mode;
}

VideoFrameFileWriter::OutputFormat
VideoPlayerTestEnvironment::GetFrameOutputFormat() const {
  return frame_output_config_.output_format;
}

uint64_t VideoPlayerTestEnvironment::GetFrameOutputLimit() const {
  return frame_output_config_.output_limit;
}

const base::FilePath& VideoPlayerTestEnvironment::OutputFolder() const {
  return output_folder_;
}

bool VideoPlayerTestEnvironment::ImportSupported() const {
  return import_supported_;
}

}  // namespace test
}  // namespace media
