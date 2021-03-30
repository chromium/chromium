// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/test/video_player/video_player_test_environment.h"

#include <utility>

#include "base/system/sys_info.h"
#include "build/chromeos_buildflags.h"
#include "media/base/media_switches.h"
#include "media/base/video_types.h"
#include "media/gpu/buildflags.h"
#include "media/gpu/test/video.h"
#include "media/gpu/test/video_player/video_decoder_client.h"

namespace media {
namespace test {

// Default video to be used if no test video was specified.
constexpr base::FilePath::CharType kDefaultTestVideoPath[] =
    FILE_PATH_LITERAL("test-25fps.h264");

// static
VideoPlayerTestEnvironment* VideoPlayerTestEnvironment::Create(
    const base::FilePath& video_path,
    const base::FilePath& video_metadata_path,
    ValidatorType validator_type,
    const DecoderImplementation implementation,
    const base::FilePath& output_folder,
    const FrameOutputConfig& frame_output_config) {
  auto video = std::make_unique<media::test::Video>(
      video_path.empty() ? base::FilePath(kDefaultTestVideoPath) : video_path,
      video_metadata_path);
  if (!video->Load()) {
    LOG(ERROR) << "Failed to load " << video_path;
    return nullptr;
  }

  return new VideoPlayerTestEnvironment(std::move(video), validator_type,
                                        implementation, output_folder,
                                        frame_output_config);
}

VideoPlayerTestEnvironment::VideoPlayerTestEnvironment(
    std::unique_ptr<media::test::Video> video,
    ValidatorType validator_type,
    const DecoderImplementation implementation,
    const base::FilePath& output_folder,
    const FrameOutputConfig& frame_output_config)
    : VideoTestEnvironment(
          /*enabled_features=*/
          {
#if BUILDFLAG(USE_VAAPI)
            // TODO(b/172217032): remove once enabled by default.
            media::kVaapiAV1Decoder,
#endif
          },
          /*disabled_features=*/
          {
#if BUILDFLAG(USE_VAAPI)
            // Disable this feature so that the decoder test can test a
            // resolution which is denied for the sake of performance. See
            // b/171041334.
            kVaapiEnforceVideoMinMaxResolution,
#endif
          }),
      video_(std::move(video)),
      validator_type_(validator_type),
      implementation_(implementation),
      frame_output_config_(frame_output_config),
      output_folder_(output_folder),
      gpu_memory_buffer_factory_(
          gpu::GpuMemoryBufferFactory::CreateNativeType(nullptr)) {
}

VideoPlayerTestEnvironment::~VideoPlayerTestEnvironment() = default;

void VideoPlayerTestEnvironment::SetUp() {
  VideoTestEnvironment::SetUp();

  // TODO(dstaessens): Remove this check once all platforms support import mode.
  // Some older platforms do not support importing buffers, but need to allocate
  // buffers internally in the decoder.
  // Note: buddy, guado and rikku support import mode for H.264 and VP9, but for
  // VP8 they use a different video decoder (V4L2 instead of VAAPI) and don't
  // support import mode.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  constexpr const char* kImportModeBlocklist[] = {
      "buddy",      "guado",      "guado-cfm", "guado-kernelnext", "nyan_big",
      "nyan_blaze", "nyan_kitty", "rikku",     "rikku-cfm"};
  const std::string board = base::SysInfo::GetLsbReleaseBoard();
  import_supported_ = (std::find(std::begin(kImportModeBlocklist),
                                 std::end(kImportModeBlocklist),
                                 board) == std::end(kImportModeBlocklist));
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // VideoDecoders always require import mode to be supported.
  DCHECK(import_supported_ || implementation_ == DecoderImplementation::kVDA);
}

const media::test::Video* VideoPlayerTestEnvironment::Video() const {
  return video_.get();
}

gpu::GpuMemoryBufferFactory*
VideoPlayerTestEnvironment::GetGpuMemoryBufferFactory() const {
  return gpu_memory_buffer_factory_.get();
}

bool VideoPlayerTestEnvironment::IsValidatorEnabled() const {
  return validator_type_ != ValidatorType::kNone;
}

VideoPlayerTestEnvironment::ValidatorType
VideoPlayerTestEnvironment::GetValidatorType() const {
  return validator_type_;
}

DecoderImplementation VideoPlayerTestEnvironment::GetDecoderImplementation()
    const {
  return implementation_;
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
