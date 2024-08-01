// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/mac/video_toolbox_video_decoder.h"

#include <CoreMedia/CoreMedia.h>
#include <VideoToolbox/VideoToolbox.h>

#include <memory>
#include <utility>

#include "base/apple/scoped_cftyperef.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/scoped_policy.h"
#include "base/task/bind_post_task.h"
#include "media/base/decoder_status.h"
#include "media/base/media_log.h"
#include "media/base/media_switches.h"
#include "media/base/supported_types.h"
#include "media/base/video_frame.h"
#include "media/gpu/accelerated_video_decoder.h"
#include "media/gpu/av1_decoder.h"
#include "media/gpu/h264_decoder.h"
#include "media/gpu/mac/video_toolbox_av1_accelerator.h"
#include "media/gpu/mac/video_toolbox_decompression_metadata.h"
#include "media/gpu/mac/video_toolbox_h264_accelerator.h"
#include "media/gpu/mac/video_toolbox_vp9_accelerator.h"
#include "media/gpu/vp9_decoder.h"
#include "ui/gfx/geometry/size.h"

#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
#include "media/gpu/h265_decoder.h"
#include "media/gpu/mac/video_toolbox_h265_accelerator.h"
#endif  // BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)

namespace media {

namespace {

bool SupportsH264() {
  return VTIsHardwareDecodeSupported(kCMVideoCodecType_H264);
}

bool InitializeVP9() {
#if BUILDFLAG(IS_MAC)
  VTRegisterSupplementalVideoDecoderIfAvailable(kCMVideoCodecType_VP9);
  return VTIsHardwareDecodeSupported(kCMVideoCodecType_VP9);
#else
  // TODO(crbug.com/40269929): Enable VP9 on iOS.
  return false;
#endif
}

bool SupportsVP9() {
  static const bool initialized = InitializeVP9();
  return initialized;
}

bool SupportsAV1() {
  return VTIsHardwareDecodeSupported(kCMVideoCodecType_AV1);
}

#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
bool SupportsHEVC() {
  return base::FeatureList::IsEnabled(media::kPlatformHEVCDecoderSupport);
}
#endif  // BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)

}  // namespace

VideoToolboxVideoDecoder::VideoToolboxVideoDecoder(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    std::unique_ptr<MediaLog> media_log,
    const gpu::GpuDriverBugWorkarounds& gpu_workarounds,
    scoped_refptr<base::SequencedTaskRunner> gpu_task_runner,
    GetCommandBufferStubCB get_stub_cb)
    : task_runner_(std::move(task_runner)),
      media_log_(std::move(media_log)),
      gpu_workarounds_(gpu_workarounds),
      gpu_task_runner_(std::move(gpu_task_runner)),
      get_stub_cb_(std::move(get_stub_cb)),
      video_toolbox_(
          task_runner_,
          media_log_->Clone(),
          base::BindRepeating(&VideoToolboxVideoDecoder::OnVideoToolboxOutput,
                              base::Unretained(this)),
          base::BindRepeating(&VideoToolboxVideoDecoder::OnVideoToolboxError,
                              base::Unretained(this))),
      output_queue_(task_runner_) {
  DVLOG(1) << __func__;
}

VideoToolboxVideoDecoder::~VideoToolboxVideoDecoder() {
  DVLOG(1) << __func__;
}

bool VideoToolboxVideoDecoder::NeedsBitstreamConversion() const {
  DVLOG(4) << __func__;
  return true;
}

int VideoToolboxVideoDecoder::GetMaxDecodeRequests() const {
  DVLOG(4) << __func__;
  // This is kMaxVideoFrames, and it seems to have worked okay so far.
  return 4;
}

VideoDecoderType VideoToolboxVideoDecoder::GetDecoderType() const {
  DVLOG(4) << __func__;
  return VideoDecoderType::kVideoToolbox;
}

void VideoToolboxVideoDecoder::Initialize(const VideoDecoderConfig& config,
                                          bool low_delay,
                                          CdmContext* cdm_context,
                                          InitCB init_cb,
                                          const OutputCB& output_cb,
                                          const WaitingCB& waiting_cb) {
  DVLOG(1) << __func__;
  DCHECK(decode_cbs_.empty());
  DCHECK(config.IsValidConfig());

  if (has_error_) {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(init_cb), DecoderStatus::Codes::kFailed));
    return;
  }

  // TODO(crbug.com/40227557): Distinguish unsupported profile from unsupported
  // codec.
  // TODO(crbug.com/40227557): Make sure that config.profile() matches
  // config.codec().
  // TODO(crbug.com/40227557): Check that the size is supported.
  bool profile_supported = false;
  for (const auto& supported_config :
       GetSupportedVideoDecoderConfigs(gpu_workarounds_)) {
    if (supported_config.profile_min <= config.profile() &&
        config.profile() <= supported_config.profile_max) {
      profile_supported = true;
      break;
    }
  }

  // If we don't have support support for a given codec, try to initialize
  // anyways -- otherwise we're certain to fail playback.
  if (!profile_supported && IsBuiltInVideoCodec(config.codec())) {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(init_cb),
                                  DecoderStatus::Codes::kUnsupportedProfile));
    NotifyError(DecoderStatus::Codes::kUnsupportedProfile);
    return;
  }

  if (config.is_encrypted()) {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(init_cb),
                       DecoderStatus::Codes::kUnsupportedEncryptionMode));
    NotifyError(DecoderStatus::Codes::kUnsupportedEncryptionMode);
    return;
  }

  // If this is a reconfiguration, drop in-flight outputs.
  if (accelerator_) {
    ResetInternal(DecoderStatus::Codes::kAborted);
  }

  // Create a new Accelerator for the configuration.
  auto accelerator_decode_cb = base::BindRepeating(
      &VideoToolboxVideoDecoder::OnAcceleratorDecode, base::Unretained(this));
  auto accelerator_output_cb = base::BindRepeating(
      &VideoToolboxVideoDecoder::OnAcceleratorOutput, base::Unretained(this));

  switch (VideoCodecProfileToVideoCodec(config.profile())) {
    case VideoCodec::kH264:
      accelerator_ = std::make_unique<H264Decoder>(
          std::make_unique<VideoToolboxH264Accelerator>(
              media_log_->Clone(), std::move(accelerator_decode_cb),
              std::move(accelerator_output_cb)),
          config.profile(), config.color_space_info());
      break;

    case VideoCodec::kVP9:
      accelerator_ = std::make_unique<VP9Decoder>(
          std::make_unique<VideoToolboxVP9Accelerator>(
              media_log_->Clone(), config.hdr_metadata(),
              std::move(accelerator_decode_cb),
              std::move(accelerator_output_cb)),
          config.profile(), config.color_space_info());
      break;

    case VideoCodec::kAV1:
      accelerator_ = std::make_unique<AV1Decoder>(
          std::make_unique<VideoToolboxAV1Accelerator>(
              media_log_->Clone(), config.hdr_metadata(),
              std::move(accelerator_decode_cb),
              std::move(accelerator_output_cb)),
          config.profile(), config.color_space_info());
      break;

#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
    case VideoCodec::kHEVC:
      accelerator_ = std::make_unique<H265Decoder>(
          std::make_unique<VideoToolboxH265Accelerator>(
              media_log_->Clone(), std::move(accelerator_decode_cb),
              std::move(accelerator_output_cb)),
          config.profile(), config.color_space_info());
      break;
#endif  // BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)

    default:
      task_runner_->PostTask(
          FROM_HERE, base::BindOnce(std::move(init_cb),
                                    DecoderStatus::Codes::kUnsupportedCodec));
      NotifyError(DecoderStatus::Codes::kUnsupportedCodec);
      return;
  }

  // Save the active configuration.
  config_ = config;
  output_queue_.SetOutputCB(output_cb);

  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(std::move(init_cb), DecoderStatus::Codes::kOk));
}

void VideoToolboxVideoDecoder::Decode(scoped_refptr<DecoderBuffer> buffer,
                                      DecodeCB decode_cb) {
  DVLOG(3) << __func__ << " pts=" << buffer->timestamp().InMilliseconds();

  if (has_error_) {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(decode_cb), DecoderStatus::Codes::kFailed));
    return;
  }

  // Flushes are handled differently from ordinary decodes.
  if (buffer->end_of_stream()) {
    if (!accelerator_->Flush()) {
      task_runner_->PostTask(
          FROM_HERE, base::BindOnce(std::move(decode_cb),
                                    DecoderStatus::Codes::kMalformedBitstream));
      NotifyError(DecoderStatus::Codes::kMalformedBitstream);
      return;
    }
    // Must be called after `accelerator_->Flush()` so that all outputs will
    // have been scheduled already.
    output_queue_.Flush(std::move(decode_cb));
    return;
  }

  decode_cbs_.push(std::move(decode_cb));
  accelerator_->SetStream(-1, *buffer);
  while (true) {
    // `active_decode_` is used in OnAcceleratorDecode() callbacks to look up
    // decode metadata.
    active_decode_ = buffer;
    AcceleratedVideoDecoder::DecodeResult result = accelerator_->Decode();
    active_decode_.reset();

    switch (result) {
      case AcceleratedVideoDecoder::kDecodeError:
      case AcceleratedVideoDecoder::kRanOutOfSurfaces:
      case AcceleratedVideoDecoder::kTryAgain:
        // More specific reasons are logged to the media log.
        NotifyError(DecoderStatus::Codes::kMalformedBitstream);
        return;

      case AcceleratedVideoDecoder::kConfigChange:
        continue;

      case AcceleratedVideoDecoder::kRanOutOfStreamData:
        // The accelerator may not have produced any sample for decoding.
        ReleaseDecodeCallbacks();
        return;
    }
  }
}

void VideoToolboxVideoDecoder::Reset(base::OnceClosure reset_cb) {
  DVLOG(1) << __func__;

  if (has_error_) {
    task_runner_->PostTask(FROM_HERE, std::move(reset_cb));
    return;
  }

  ResetInternal(DecoderStatus::Codes::kAborted);
  task_runner_->PostTask(FROM_HERE, std::move(reset_cb));
}

void VideoToolboxVideoDecoder::NotifyError(DecoderStatus status) {
  DVLOG(1) << __func__;

  if (has_error_) {
    return;
  }

  has_error_ = true;
  ResetInternal(status);
}

void VideoToolboxVideoDecoder::ResetInternal(DecoderStatus status) {
  DVLOG(4) << __func__;

  while (!decode_cbs_.empty()) {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(decode_cbs_.front()), status));
    decode_cbs_.pop();
  }

  if (accelerator_) {
    accelerator_->Reset();
  }
  video_toolbox_.Reset();
  output_queue_.Reset(status);

  // Drop in-flight conversions.
  converter_weak_this_factory_.InvalidateWeakPtrs();
  num_conversions_ = 0;
}

void VideoToolboxVideoDecoder::ReleaseDecodeCallbacks() {
  DVLOG(4) << __func__;
  DCHECK(!has_error_);

  while (decode_cbs_.size() > video_toolbox_.NumDecodes() + num_conversions_) {
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(std::move(decode_cbs_.front()),
                                          DecoderStatus::Codes::kOk));
    decode_cbs_.pop();
  }
}

void VideoToolboxVideoDecoder::OnAcceleratorDecode(
    base::apple::ScopedCFTypeRef<CMSampleBufferRef> sample,
    VideoToolboxDecompressionSessionMetadata session_metadata,
    scoped_refptr<CodecPicture> picture) {
  DVLOG(4) << __func__
           << " pts=" << active_decode_->timestamp().InMilliseconds();
  DCHECK(active_decode_);

  auto metadata = std::make_unique<VideoToolboxDecodeMetadata>();
  metadata->picture = std::move(picture);
  metadata->timestamp = active_decode_->timestamp();
  metadata->duration = active_decode_->duration();
  metadata->aspect_ratio = config_.aspect_ratio();
  metadata->color_space = accelerator_->GetVideoColorSpace().ToGfxColorSpace();
  if (!metadata->color_space.IsValid()) {
    // Note: It is expected that the accelerated video decoders are already
    // doing something similar, since the config color space is being provided
    // to them.
    metadata->color_space = config_.color_space_info().ToGfxColorSpace();
  }

  metadata->hdr_metadata = accelerator_->GetHDRMetadata();
  if (!metadata->hdr_metadata) {
    // Note: The VP9 accelerator contains this same logic so that the format
    // description can include HDR metadata (there is no in-band HDR metadata
    // in VP9). The other accelerators use only in-band HDR metadata.
    metadata->hdr_metadata = config_.hdr_metadata();
  }

  metadata->session_metadata = session_metadata;

  video_toolbox_.Decode(std::move(sample), std::move(metadata));
}

void VideoToolboxVideoDecoder::OnAcceleratorOutput(
    scoped_refptr<CodecPicture> picture) {
  DVLOG(3) << __func__;
  output_queue_.SchedulePicture(std::move(picture));
}

void VideoToolboxVideoDecoder::OnVideoToolboxOutput(
    base::apple::ScopedCFTypeRef<CVImageBufferRef> image,
    std::unique_ptr<VideoToolboxDecodeMetadata> metadata) {
  DVLOG(4) << __func__ << " pts=" << metadata->timestamp.InMilliseconds();

  if (has_error_) {
    return;
  }

  // Check if the frame was dropped.
  // TODO(crbug.com/40227557): Notify the output queue of dropped frames.
  if (!image) {
    ReleaseDecodeCallbacks();
    return;
  }

  // Lazily create `converter_`.
  if (!converter_) {
    converter_ = base::MakeRefCounted<VideoToolboxFrameConverter>(
        gpu_task_runner_, media_log_->Clone(), std::move(get_stub_cb_));
  }

  gpu_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &VideoToolboxFrameConverter::Convert, converter_, std::move(image),
          std::move(metadata),
          base::BindPostTask(
              task_runner_,
              base::BindOnce(&VideoToolboxVideoDecoder::OnConverterOutput,
                             converter_weak_this_factory_.GetWeakPtr()))));

  ++num_conversions_;
}

void VideoToolboxVideoDecoder::OnVideoToolboxError(DecoderStatus status) {
  DVLOG(1) << __func__;
  NotifyError(std::move(status));
}

void VideoToolboxVideoDecoder::OnConverterOutput(
    scoped_refptr<VideoFrame> frame,
    std::unique_ptr<VideoToolboxDecodeMetadata> metadata) {
  DVLOG(4) << __func__ << " pts=" << metadata->timestamp.InMilliseconds();

  if (has_error_) {
    return;
  }

  if (!frame) {
    // More specific reasons are logged to the media log.
    NotifyError(DecoderStatus::Codes::kFailedToGetVideoFrame);
    return;
  }

  CHECK_GT(num_conversions_, 0u);
  --num_conversions_;

  // The output queue expects that all decode callbacks have been called at the
  // time that a flush completes (all outputs are fulfilled), so we must release
  // before fulfilling pictures (at least during a flush).
  //
  // It would be possible to obtain tighter bounds on the backpressure by moving
  // responsibility for releasing callbacks to the output queue implementation.
  ReleaseDecodeCallbacks();

  output_queue_.FulfillPicture(std::move(metadata->picture), std::move(frame));
}

// static
std::vector<SupportedVideoDecoderConfig>
VideoToolboxVideoDecoder::GetSupportedVideoDecoderConfigs(
    const gpu::GpuDriverBugWorkarounds& gpu_workarounds) {
  std::vector<SupportedVideoDecoderConfig> supported;

  // TODO(crbug.com/40227557): Test support for other H.264 profiles.
  // TODO(crbug.com/40227557): Exclude resolutions that are not accelerated.
  // TODO(crbug.com/40227557): Check if higher resolutions are supported.
  if (!gpu_workarounds.disable_accelerated_h264_decode && SupportsH264()) {
    supported.emplace_back(
        /*profile_min=*/H264PROFILE_BASELINE,
        /*profile_max=*/H264PROFILE_HIGH,
        /*coded_size_min=*/gfx::Size(16, 16),
        /*coded_size_max=*/gfx::Size(4096, 4096),
        /*allow_encrypted=*/false,
        /*require_encrypted=*/false);
  }

  if (!gpu_workarounds.disable_accelerated_vp9_decode && SupportsVP9()) {
    supported.emplace_back(
        /*profile_min=*/VP9PROFILE_PROFILE0,
        /*profile_max=*/VP9PROFILE_PROFILE0,
        /*coded_size_min=*/gfx::Size(16, 16),
        /*coded_size_max=*/gfx::Size(4096, 4096),
        /*allow_encrypted=*/false,
        /*require_encrypted=*/false);
    if (!gpu_workarounds.disable_accelerated_vp9_profile2_decode) {
      supported.emplace_back(
          /*profile_min=*/VP9PROFILE_PROFILE2,
          /*profile_max=*/VP9PROFILE_PROFILE2,
          /*coded_size_min=*/gfx::Size(16, 16),
          /*coded_size_max=*/gfx::Size(4096, 4096),
          /*allow_encrypted=*/false,
          /*require_encrypted=*/false);
    }
  }

  if (!gpu_workarounds.disable_accelerated_av1_decode && SupportsAV1()) {
    supported.emplace_back(
        /*profile_min=*/AV1PROFILE_PROFILE_MAIN,
        /*profile_max=*/AV1PROFILE_PROFILE_MAIN,
        /*coded_size_min=*/gfx::Size(16, 16),
        /*coded_size_max=*/gfx::Size(8192, 8192),
        /*allow_encrypted=*/false,
        /*require_encrypted=*/false);
  }

#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
  if (!gpu_workarounds.disable_accelerated_hevc_decode && SupportsHEVC()) {
    supported.emplace_back(
        /*profile_min=*/HEVCPROFILE_MIN,
        /*profile_max=*/HEVCPROFILE_MAX,
        /*coded_size_min=*/gfx::Size(16, 16),
        /*coded_size_max=*/gfx::Size(8192, 8192),
        /*allow_encrypted=*/false,
        /*require_encrypted=*/false);
    supported.emplace_back(
        /*profile_min=*/HEVCPROFILE_REXT,
        /*profile_max=*/HEVCPROFILE_REXT,
        /*coded_size_min=*/gfx::Size(16, 16),
        /*coded_size_max=*/gfx::Size(8192, 8192),
        /*allow_encrypted=*/false,
        /*require_encrypted=*/false);
  }
#endif  // BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)

  return supported;
}

}  // namespace media
