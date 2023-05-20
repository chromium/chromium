// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/v4l2/v4l2_stateful_video_decoder.h"

#include <fcntl.h>
#include <sys/ioctl.h>

#include "base/files/file_util.h"
#include "base/memory/ptr_util.h"
#include "base/posix/eintr_wrapper.h"
#include "media/base/media_log.h"
#include "media/gpu/macros.h"
#include "media/gpu/v4l2/v4l2_utils.h"
#include "ui/gfx/geometry/size.h"

namespace {
int HandledIoctl(int fd, int request, void* arg) {
  return HANDLE_EINTR(ioctl(fd, request, arg));
}
}  // namespace

namespace media {

// static
std::unique_ptr<VideoDecoderMixin> V4L2StatefulVideoDecoder::Create(
    std::unique_ptr<MediaLog> media_log,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    base::WeakPtr<VideoDecoderMixin::Client> client) {
  DCHECK(task_runner->RunsTasksInCurrentSequence());
  DCHECK(client);

  return base::WrapUnique<VideoDecoderMixin>(new V4L2StatefulVideoDecoder(
      std::move(media_log), std::move(task_runner), std::move(client)));
}

// static
absl::optional<SupportedVideoDecoderConfigs>
V4L2StatefulVideoDecoder::GetSupportedConfigs() {
  SupportedVideoDecoderConfigs supported_media_configs;

  constexpr char kVideoDeviceDriverPath[] = "/dev/video-dec0";
  CHECK(base::PathExists(base::FilePath(kVideoDeviceDriverPath)));

  base::ScopedFD device_fd(HANDLE_EINTR(
      open(kVideoDeviceDriverPath, O_RDWR | O_NONBLOCK | O_CLOEXEC)));
  if (!device_fd.is_valid()) {
    return absl::nullopt;
  }

  std::vector<uint32_t> v4l2_codecs = EnumerateSupportedPixFmts(
      base::BindRepeating(&HandledIoctl, device_fd.get()),
      V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);

  // V4L2 stateful formats (don't end up with _SLICE or _FRAME) supported.
  constexpr std::array<uint32_t, 4> kSupportedInputCodecs = {
    V4L2_PIX_FMT_H264,
#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
    V4L2_PIX_FMT_HEVC,
#endif  // BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
    V4L2_PIX_FMT_VP8,
    V4L2_PIX_FMT_VP9,
  };
  std::erase_if(v4l2_codecs, [kSupportedInputCodecs](uint32_t v4l2_codec) {
    return !base::Contains(kSupportedInputCodecs, v4l2_codec);
  });

  for (const uint32_t v4l2_codec : v4l2_codecs) {
    const std::vector<VideoCodecProfile> media_codec_profiles =
        EnumerateSupportedProfilesForV4L2Codec(
            base::BindRepeating(&HandledIoctl, device_fd.get()), v4l2_codec);

    gfx::Size min_coded_size;
    gfx::Size max_coded_size;
    GetSupportedResolution(base::BindRepeating(&HandledIoctl, device_fd.get()),
                           v4l2_codec, &min_coded_size, &max_coded_size);

    for (const auto& profile : media_codec_profiles) {
      supported_media_configs.emplace_back(SupportedVideoDecoderConfig(
          profile, profile, min_coded_size, max_coded_size,
          /*allow_encrypted=*/false, /*require_encrypted=*/false));
    }
  }

#if DCHECK_IS_ON()
  for (const auto& config : supported_media_configs) {
    DVLOGF(3) << "Enumerated " << GetProfileName(config.profile_min) << " ("
              << config.coded_size_min.ToString() << "-"
              << config.coded_size_max.ToString() << ")";
  }
#endif

  return supported_media_configs;
}

void V4L2StatefulVideoDecoder::Initialize(const VideoDecoderConfig& config,
                                          bool /*low_delay*/,
                                          CdmContext* cdm_context,
                                          InitCB init_cb,
                                          const OutputCB& output_cb,
                                          const WaitingCB& /*waiting_cb*/) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(config.IsValidConfig());
  DVLOGF(1) << config.AsHumanReadableString();
  NOTIMPLEMENTED();
}

void V4L2StatefulVideoDecoder::Decode(scoped_refptr<DecoderBuffer> buffer,
                                      DecodeCB decode_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOGF(3) << buffer->AsHumanReadableString(/*verbose=*/false);
  NOTIMPLEMENTED();
}

void V4L2StatefulVideoDecoder::Reset(base::OnceClosure closure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOGF(2);
  NOTIMPLEMENTED();
}

bool V4L2StatefulVideoDecoder::NeedsBitstreamConversion() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTREACHED() << "Our only owner VideoDecoderPipeline never calls here";
  return false;
}

bool V4L2StatefulVideoDecoder::CanReadWithoutStalling() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTREACHED() << "Our only owner VideoDecoderPipeline never calls here";
  return true;
}

int V4L2StatefulVideoDecoder::GetMaxDecodeRequests() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTREACHED() << "Our only owner VideoDecoderPipeline never calls here";
  return 4;
}

VideoDecoderType V4L2StatefulVideoDecoder::GetDecoderType() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTREACHED() << "Our only owner VideoDecoderPipeline never calls here";
  return VideoDecoderType::kV4L2;
}

bool V4L2StatefulVideoDecoder::IsPlatformDecoder() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTREACHED() << "Our only owner VideoDecoderPipeline never calls here";
  return true;
}

void V4L2StatefulVideoDecoder::ApplyResolutionChange() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOGF(2);
  NOTIMPLEMENTED();
}

size_t V4L2StatefulVideoDecoder::GetMaxOutputFramePoolSize() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTIMPLEMENTED();
  return 0;
}

void V4L2StatefulVideoDecoder::SetDmaIncoherentV4L2(bool incoherent) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTIMPLEMENTED();
}

V4L2StatefulVideoDecoder::V4L2StatefulVideoDecoder(
    std::unique_ptr<MediaLog> media_log,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    base::WeakPtr<VideoDecoderMixin::Client> client)
    : VideoDecoderMixin(std::move(media_log),
                        std::move(task_runner),
                        std::move(client)) {
  DCHECK(task_runner->RunsTasksInCurrentSequence());
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOGF(1);
}

V4L2StatefulVideoDecoder::~V4L2StatefulVideoDecoder() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOGF(1);
}

}  // namespace media
