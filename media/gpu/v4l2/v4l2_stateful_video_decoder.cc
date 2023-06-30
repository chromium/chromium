// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/v4l2/v4l2_stateful_video_decoder.h"

#include <fcntl.h>
#include <poll.h>
#include <sys/eventfd.h>
#include <sys/ioctl.h>

#include "base/files/file_util.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/posix/eintr_wrapper.h"
#include "media/base/media_log.h"
#include "media/gpu/macros.h"
#include "media/gpu/v4l2/v4l2_queue.h"
#include "media/gpu/v4l2/v4l2_utils.h"
#include "ui/gfx/geometry/size.h"

namespace {
// Numerical value of ioctl() OK return value;
constexpr int kIoctlOk = 0;

int HandledIoctl(int fd, int request, void* arg) {
  return HANDLE_EINTR(ioctl(fd, request, arg));
}

void* Mmap(int fd,
           void* addr,
           unsigned int len,
           int prot,
           int flags,
           unsigned int offset) {
  return mmap(addr, len, prot, flags, fd, offset);
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

  if (config.is_encrypted() || !!cdm_context) {
    std::move(init_cb).Run(DecoderStatus::Codes::kUnsupportedEncryptionMode);
    return;
  }

  // Our caller VideoDecoderPipeline will make sure that |config| is within
  // GetSupportedConfigs() beforehand. CHECK()ing it here seems excessive.

  if (!device_fd_.is_valid()) {
    constexpr char kVideoDeviceDriverPath[] = "/dev/video-dec0";
    CHECK(base::PathExists(base::FilePath(kVideoDeviceDriverPath)));

    device_fd_.reset(HANDLE_EINTR(
        open(kVideoDeviceDriverPath, O_RDWR | O_NONBLOCK | O_CLOEXEC)));
    if (!device_fd_.is_valid()) {
      std::move(init_cb).Run(DecoderStatus::Codes::kFailedToCreateDecoder);
      return;
    }
  }

  // If we've been Initialize()d before, destroy state members.
  if (OUTPUT_queue_) {
    // This will also Deallocate() all buffers and issue a VIDIOC_STREAMOFF.
    OUTPUT_queue_.reset();
  }

  // At this point we initialize the |OUTPUT_queue_| only, following
  // instructions in e.g. [1]. The decoded video frames queue configuration
  // must wait until there are enough encoded chunks fed into said
  // |OUTPUT_queue_| for the driver to know the output details. The driver will
  // let us know that moment via a V4L2_EVENT_SOURCE_CHANGE.
  // [1]
  // https://www.kernel.org/doc/html/v5.15/userspace-api/media/v4l/dev-decoder.html#initialization

  OUTPUT_queue_ = base::WrapRefCounted(
      new V4L2Queue(base::BindRepeating(&HandledIoctl, device_fd_.get()),
                    /*schedule_poll_cb=*/base::DoNothing(),
                    /*mmap_cb=*/base::BindRepeating(&Mmap, device_fd_.get()),
                    V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,
                    /*destroy_cb=*/base::DoNothing()));

  const auto profile_as_v4l2_fourcc =
      VideoCodecProfileToV4L2PixFmt(config.profile(), /*slice_based=*/false);

  // In legacy code this was good for up to 1080p.
  // TODO(mcasas): Increase this by 4x to support 4K decoding, if needed.
  constexpr size_t kInputBufferMaxSize = 1024 * 1024;
  const auto v4l2_format = OUTPUT_queue_->SetFormat(
      profile_as_v4l2_fourcc, gfx::Size(), kInputBufferMaxSize);
  if (!v4l2_format) {
    std::move(init_cb).Run(DecoderStatus::Codes::kFailedToCreateDecoder);
    return;
  }
  DCHECK_EQ(v4l2_format->fmt.pix_mp.pixelformat, profile_as_v4l2_fourcc);

  constexpr size_t kNumInputBuffers = 8;
  if (OUTPUT_queue_->AllocateBuffers(kNumInputBuffers, V4L2_MEMORY_MMAP,
                                     /*incoherent=*/false) < kNumInputBuffers) {
    std::move(init_cb).Run(DecoderStatus::Codes::kFailedToCreateDecoder);
    return;
  }
  if (!OUTPUT_queue_->Streamon()) {
    std::move(init_cb).Run(DecoderStatus::Codes::kFailedToCreateDecoder);
    return;
  }

  // Subscribe to the resolution change event. This is needed for resolution
  // changes mid stream but also to initialize the |CAPTURE_queue|.
  struct v4l2_event_subscription sub = {.type = V4L2_EVENT_SOURCE_CHANGE};
  if (HandledIoctl(device_fd_.get(), VIDIOC_SUBSCRIBE_EVENT, &sub) !=
      kIoctlOk) {
    PLOG(ERROR) << "Failed to subscribe to V4L2_EVENT_SOURCE_CHANGE";
    std::move(init_cb).Run(DecoderStatus::Codes::kFailedToCreateDecoder);
    return;
  }

  profile_ = config.profile();
  std::move(init_cb).Run(DecoderStatus::Codes::kOk);
}

void V4L2StatefulVideoDecoder::Decode(scoped_refptr<DecoderBuffer> buffer,
                                      DecodeCB decode_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(OUTPUT_queue_)
      << "V4L2StatefulVideoDecoder has not been Initialize()d.";
  DVLOGF(3) << buffer->AsHumanReadableString(/*verbose=*/false);

  if (buffer->end_of_stream()) {
    NOTIMPLEMENTED() << "EOS is not supported yet";
    std::move(decode_cb).Run(DecoderStatus::Codes::kAborted);
    return;
  }

  if (profile_ == H264PROFILE_BASELINE || profile_ == H264PROFILE_MAIN ||
      profile_ == H264PROFILE_HIGH || profile_ == HEVCPROFILE_MAIN) {
    // We need to instantiate an InputBufferFragmentSplitter.
    NOTIMPLEMENTED();
    std::move(decode_cb).Run(DecoderStatus::Codes::kAborted);
    return;
  }

  // Ideally we should be able to get some resources back that were queued in
  // during a previous Decode().
  while (OUTPUT_queue_->QueuedBuffersCount()) {
    const auto result = OUTPUT_queue_->DequeueBuffer();
    if (!result.first || !result.second) {
      break;
    }
  }

  absl::optional<V4L2WritableBufferRef> v4l2_buffer =
      OUTPUT_queue_->GetFreeBuffer();
  if (!v4l2_buffer) {
    // |OUTPUT_queue_| has no free slots. Store |buffer| and |decode_cb| and
    // try again later.
    NOTIMPLEMENTED();
    return;
  }

  CHECK_EQ(v4l2_buffer->PlanesCount(), 1u);
  uint8_t* dst = static_cast<uint8_t*>(v4l2_buffer->GetPlaneMapping(0));
  CHECK_GE(v4l2_buffer->GetPlaneSize(/*plane=*/0), buffer->data_size());
  memcpy(dst, buffer->data(), buffer->data_size());
  v4l2_buffer->SetPlaneBytesUsed(0, buffer->data_size());

  if (!std::move(*v4l2_buffer).QueueMMap()) {
    LOG(ERROR) << "Error while queuing input buffer!";
    std::move(decode_cb).Run(DecoderStatus::Codes::kPlatformDecodeFailure);
    return;
  }

  if (PollOnceForResolutionChangeEvent()) {
    VLOGF(3) << "Got a resolution change event.";
    // TODO(mcasas): Initialize |CAPTURE_queue|.
  }

  std::move(decode_cb).Run(DecoderStatus::Codes::kOk);
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
  DCHECK(decoder_task_runner_->RunsTasksInCurrentSequence());
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOGF(1);
}

V4L2StatefulVideoDecoder::~V4L2StatefulVideoDecoder() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOGF(1);
}

bool V4L2StatefulVideoDecoder::PollOnceForResolutionChangeEvent() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  struct pollfd pollfds[1] = {
      {.fd = device_fd_.get(), .events = POLLIN | POLLOUT | POLLERR | POLLPRI}};
  if (HANDLE_EINTR(poll(pollfds, std::size(pollfds), /*timeout=*/0)) <
      kIoctlOk) {
    PLOG(ERROR) << "Polling for events failed";
    return false;
  }
  if (!(pollfds[0].revents & POLLPRI)) {
    return false;
  }

  // There is an event: dequeue it.
  struct v4l2_event event;
  memset(&event, 0, sizeof(event));  // Must do: v4l2_event has a union.
  if (HandledIoctl(device_fd_.get(), VIDIOC_DQEVENT, &event) != kIoctlOk) {
    PLOG(ERROR) << "Failed dequeing an event";
    return false;
  }
  // If we get an event, it must be an V4L2_EVENT_SOURCE_CHANGE since it's the
  // only one we're subscribed to.
  DCHECK_EQ(event.type, static_cast<unsigned int>(V4L2_EVENT_SOURCE_CHANGE));
  DCHECK(event.u.src_change.changes & V4L2_EVENT_SRC_CH_RESOLUTION);
  return true;
}

}  // namespace media
