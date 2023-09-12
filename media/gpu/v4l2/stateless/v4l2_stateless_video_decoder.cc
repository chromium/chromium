// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/v4l2/stateless/v4l2_stateless_video_decoder.h"

#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "media/gpu/gpu_video_decode_accelerator_helpers.h"
#include "media/gpu/macros.h"
#include "media/gpu/v4l2/stateless/utils.h"
#include "media/gpu/v4l2/stateless/vp9_delegate.h"
#include "media/gpu/v4l2/v4l2_status.h"

namespace media {

// static
std::unique_ptr<VideoDecoderMixin> V4L2StatelessVideoDecoder::Create(
    std::unique_ptr<MediaLog> media_log,
    scoped_refptr<base::SequencedTaskRunner> decoder_task_runner,
    base::WeakPtr<VideoDecoderMixin::Client> client) {
  return base::WrapUnique<VideoDecoderMixin>(new V4L2StatelessVideoDecoder(
      std::move(media_log), std::move(decoder_task_runner), std::move(client),
      new StatelessDevice()));
}

V4L2StatelessVideoDecoder::V4L2StatelessVideoDecoder(
    std::unique_ptr<MediaLog> media_log,
    scoped_refptr<base::SequencedTaskRunner> decoder_task_runner,
    base::WeakPtr<VideoDecoderMixin::Client> client,
    scoped_refptr<StatelessDevice> device)
    : VideoDecoderMixin(std::move(media_log),
                        std::move(decoder_task_runner),
                        std::move(client)),
      device_(std::move(device)) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
}

V4L2StatelessVideoDecoder::~V4L2StatelessVideoDecoder() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
}

// static
absl::optional<SupportedVideoDecoderConfigs>
V4L2StatelessVideoDecoder::GetSupportedConfigs() {
  const scoped_refptr<StatelessDevice> device =
      base::MakeRefCounted<StatelessDevice>();
  if (device->Open()) {
    const auto configs = GetSupportedDecodeProfiles(device.get());
    if (configs.empty()) {
      return absl::nullopt;
    }

    return ConvertFromSupportedProfiles(configs, false);
  }

  return absl::nullopt;
}

void V4L2StatelessVideoDecoder::Initialize(const VideoDecoderConfig& config,
                                           bool low_delay,
                                           CdmContext* cdm_context,
                                           InitCB init_cb,
                                           const OutputCB& output_cb,
                                           const WaitingCB& waiting_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DCHECK(config.IsValidConfig());
  DVLOGF(3);

  if (config.is_encrypted()) {
    VLOGF(1) << "Decoder does not support encrypted stream";
    std::move(init_cb).Run(DecoderStatus::Codes::kUnsupportedEncryptionMode);
    return;
  }

  device_->Close();
  if (!device_->Open()) {
    DVLOGF(1) << "Failed to open device.";
    std::move(init_cb).Run(
        DecoderStatus(DecoderStatus::Codes::kNotInitialized)
            .AddCause(V4L2Status(V4L2Status::Codes::kNoDevice)));
    return;
  }

  if (!device_->CheckCapabilities(
          VideoCodecProfileToVideoCodec(config.profile()))) {
    DVLOGF(1) << "Device does not have sufficient capabilities.";
    std::move(init_cb).Run(
        DecoderStatus(DecoderStatus::Codes::kNotInitialized)
            .AddCause(
                V4L2Status(V4L2Status::Codes::kFailedFileCapabilitiesCheck)));
    return;
  }

  if (!CreateDecoder(config.profile(), config.color_space_info())) {
    std::move(init_cb).Run(
        DecoderStatus(DecoderStatus::Codes::kNotInitialized)
            .AddCause(
                V4L2Status(V4L2Status::Codes::kNoDriverSupportForFourcc)));
    return;
  }

  output_cb_ = std::move(output_cb);
  std::move(init_cb).Run(DecoderStatus::Codes::kOk);
}

void V4L2StatelessVideoDecoder::Decode(scoped_refptr<DecoderBuffer> buffer,
                                       DecodeCB decode_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DVLOGF(4) << buffer->AsHumanReadableString(/*verbose=*/false);

  const int32_t bitstream_id =
      bitstream_id_generator_.GenerateNextId().GetUnsafeValue();

  ProcessCompressedBuffer(std::move(buffer), std::move(decode_cb),
                          bitstream_id);
}

void V4L2StatelessVideoDecoder::Reset(base::OnceClosure reset_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  NOTIMPLEMENTED();
}

bool V4L2StatelessVideoDecoder::NeedsBitstreamConversion() const {
  NOTREACHED() << "Our only owner VideoDecoderPipeline never calls here";
  return false;
}

bool V4L2StatelessVideoDecoder::CanReadWithoutStalling() const {
  NOTREACHED() << "Our only owner VideoDecoderPipeline never calls here";
  return false;
}

int V4L2StatelessVideoDecoder::GetMaxDecodeRequests() const {
  NOTREACHED() << "Our only owner VideoDecoderPipeline never calls here";
  return -1;
}

VideoDecoderType V4L2StatelessVideoDecoder::GetDecoderType() const {
  NOTREACHED() << "Our only owner VideoDecoderPipeline never calls here";
  return VideoDecoderType::kV4L2;
}

bool V4L2StatelessVideoDecoder::IsPlatformDecoder() const {
  NOTREACHED() << "Our only owner VideoDecoderPipeline never calls here";
  return true;
}

void V4L2StatelessVideoDecoder::ApplyResolutionChange() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  NOTIMPLEMENTED();
}

size_t V4L2StatelessVideoDecoder::GetMaxOutputFramePoolSize() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  NOTIMPLEMENTED();
  return 0;
}

scoped_refptr<V4L2DecodeSurface> V4L2StatelessVideoDecoder::CreateSurface() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  NOTIMPLEMENTED();
  return nullptr;
}

void V4L2StatelessVideoDecoder::SurfaceReady(
    scoped_refptr<V4L2DecodeSurface> dec_surface,
    int32_t bitstream_id,
    const gfx::Rect& visible_rect,
    const VideoColorSpace& color_space) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  NOTREACHED();
  NOTIMPLEMENTED();
}

bool V4L2StatelessVideoDecoder::CreateDecoder(VideoCodecProfile profile,
                                              VideoColorSpace color_space) {
  DVLOGF(3);

  switch (VideoCodecProfileToVideoCodec(profile)) {
    case VideoCodec::kVP9:
      decoder_ = std::make_unique<VP9Decoder>(
          std::make_unique<VP9Delegate>(
              this, device_->IsCompressedVP9HeaderSupported()),
          profile, color_space);
      break;
    default:
      DVLOGF(1) << GetCodecName(VideoCodecProfileToVideoCodec(profile))
                << " not supported.";
      return false;
  }

  return true;
}

void V4L2StatelessVideoDecoder::ProcessCompressedBuffer(
    scoped_refptr<DecoderBuffer> compressed_buffer,
    VideoDecoder::DecodeCB decode_cb,
    int32_t bitstream_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DVLOGF(4);
  DCHECK(decoder_);

  // The |decoder_| does not own the |compressed_buffer|. The
  // |compressed_buffer| needs to be held onto until |Decode| returns
  // AcceleratedVideoDecoder::kRanOutOfStreamData. Multiple calls to |Decode|
  // can process the same |compressed_buffer|. This function can not return
  // until the |decoder_| no longer needs to use that data.
  AcceleratedVideoDecoder::DecodeResult decode_result = decoder_->Decode();

  // This function expects that the decoder will be in a state ready to
  // receive compressed data. Because the lifetime of the |compressed_buffer|
  // is only for this function every time through the decoder should
  // be requesting more data.
  CHECK_EQ(decode_result, AcceleratedVideoDecoder::kRanOutOfStreamData);

  if (!compressed_buffer->end_of_stream()) {
    decoder_->SetStream(bitstream_id, *compressed_buffer);

    do {
      decode_result = decoder_->Decode();
      switch (decode_result) {
        case AcceleratedVideoDecoder::kConfigChange:
          VLOGF(2) << "AcceleratedVideoDecoder::kConfigChange";
          NOTIMPLEMENTED();
          break;
        case AcceleratedVideoDecoder::kColorSpaceChange:
          VLOGF(2) << "AcceleratedVideoDecoder::kColorSpaceChange";
          NOTIMPLEMENTED();
          break;
        case AcceleratedVideoDecoder::kRanOutOfStreamData:
          VLOGF(2) << "AcceleratedVideoDecoder::kRanOutOfStreamData";
          // Handled on first entry to function.
          break;
        case AcceleratedVideoDecoder::kRanOutOfSurfaces:
          VLOGF(2) << "AcceleratedVideoDecoder::kRanOutOfSurfaces";
          NOTIMPLEMENTED();
          break;
        case AcceleratedVideoDecoder::kNeedContextUpdate:
          VLOGF(2) << "AcceleratedVideoDecoder::kNeedContextUpdate";
          NOTIMPLEMENTED();
          break;
        case AcceleratedVideoDecoder::kDecodeError:
          VLOGF(2) << "AcceleratedVideoDecoder::kDecodeError";
          NOTIMPLEMENTED();
          break;
        case AcceleratedVideoDecoder::kTryAgain:
          VLOGF(2) << "AcceleratedVideoDecoder::kTryAgain";
          NOTIMPLEMENTED();
          break;
      }
    } while (AcceleratedVideoDecoder::kRanOutOfStreamData != decode_result);
  }

  // TODO: This PostTask is blindly sending a positive status. Errors need
  // to be handled correctly.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(decode_cb), DecoderStatus::Codes::kOk));
}

}  // namespace media
