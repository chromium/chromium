// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/clients/mojo_stable_video_decoder.h"

#include "media/base/supported_video_decoder_config.h"
#include "media/gpu/chromeos/frame_resource.h"
#include "media/gpu/chromeos/oop_video_decoder.h"

namespace media {

MojoStableVideoDecoder::MojoStableVideoDecoder(
    scoped_refptr<base::SequencedTaskRunner> media_task_runner,
    GpuVideoAcceleratorFactories* gpu_factories,
    MediaLog* media_log,
    mojo::PendingRemote<stable::mojom::StableVideoDecoder>
        pending_remote_decoder)
    : media_task_runner_(std::move(media_task_runner)),
      gpu_factories_(gpu_factories),
      media_log_(media_log),
      pending_remote_decoder_(std::move(pending_remote_decoder)),
      weak_this_factory_(this) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

MojoStableVideoDecoder::~MojoStableVideoDecoder() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

bool MojoStableVideoDecoder::IsPlatformDecoder() const {
  return true;
}

bool MojoStableVideoDecoder::SupportsDecryption() const {
  // TODO(b/327268445): implement decoding of protected content for GTFO OOP-VD.
  return false;
}

void MojoStableVideoDecoder::Initialize(const VideoDecoderConfig& config,
                                        bool low_delay,
                                        CdmContext* cdm_context,
                                        InitCB init_cb,
                                        const OutputCB& output_cb,
                                        const WaitingCB& waiting_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(b/327268445): consider not constructing a MojoStableVideoDecoder to
  // begin with if there isn't a non-null GpuVideoAcceleratorFactories*
  // available (and then this if can be turned into a CHECK()).
  if (!gpu_factories_) {
    std::move(init_cb).Run(DecoderStatus::Codes::kInvalidArgument);
    return;
  }
  OOPVideoDecoder::NotifySupportKnown(
      std::move(pending_remote_decoder_),
      base::BindOnce(
          &MojoStableVideoDecoder::InitializeOnceSupportedConfigsAreKnown,
          weak_this_factory_.GetWeakPtr(), config, low_delay, cdm_context,
          std::move(init_cb), output_cb, waiting_cb));
}

void MojoStableVideoDecoder::Decode(scoped_refptr<DecoderBuffer> buffer,
                                    DecodeCB decode_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!!oop_video_decoder_);
  oop_video_decoder_->Decode(std::move(buffer), std::move(decode_cb));
}

void MojoStableVideoDecoder::Reset(base::OnceClosure closure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!!oop_video_decoder_);
  oop_video_decoder_->Reset(std::move(closure));
}

bool MojoStableVideoDecoder::NeedsBitstreamConversion() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!!oop_video_decoder_);
  return oop_video_decoder_->NeedsBitstreamConversion();
}

bool MojoStableVideoDecoder::CanReadWithoutStalling() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(b/327268445): finish implementing CanReadWithoutStalling().
  NOTIMPLEMENTED();
  return false;
}

int MojoStableVideoDecoder::GetMaxDecodeRequests() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!!oop_video_decoder_);
  return oop_video_decoder_->GetMaxDecodeRequests();
}

VideoDecoderType MojoStableVideoDecoder::GetDecoderType() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(b/327268445): finish implementing GetDecoderType().
  NOTIMPLEMENTED();
  return VideoDecoderType::kOutOfProcess;
}

void MojoStableVideoDecoder::InitializeOnceSupportedConfigsAreKnown(
    const VideoDecoderConfig& config,
    bool low_delay,
    CdmContext* cdm_context,
    InitCB init_cb,
    const OutputCB& output_cb,
    const WaitingCB& waiting_cb,
    mojo::PendingRemote<stable::mojom::StableVideoDecoder>
        pending_remote_decoder) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // The OOPVideoDecoder initialization path assumes that a higher layer checks
  // that the VideoDecoderConfig is supported. That higher layer is the
  // MojoStableVideoDecoder in this case.
  std::optional<SupportedVideoDecoderConfigs> supported_configs =
      OOPVideoDecoder::GetSupportedConfigs();
  // InitializeOnceSupportedConfigsAreKnown() gets called only once the
  // supported configurations are known.
  CHECK(supported_configs.has_value());
  if (!IsVideoDecoderConfigSupported(supported_configs.value(), config)) {
    std::move(init_cb).Run(DecoderStatus::Codes::kUnsupportedConfig);
    return;
  }

  if (!oop_video_decoder_) {
    // This should correspond to the first MojoStableVideoDecoder::Initialize()
    // call with a supported configuration, so |pending_remote_decoder| and
    // |media_log_| must be valid.
    CHECK(pending_remote_decoder);
    CHECK(media_log_);

    // |media_task_runner_| is expected to correspond to |sequence_checker_| and
    // is the sequence on which |oop_video_decoder_| will be used.
    CHECK(media_task_runner_->RunsTasksInCurrentSequence());

    oop_video_decoder_ = OOPVideoDecoder::Create(
        std::move(pending_remote_decoder), media_log_->Clone(),
        /*decoder_task_runner=*/media_task_runner_,
        /*client=*/nullptr);
    CHECK(oop_video_decoder_);

    media_log_ = nullptr;
  }

  output_cb_ = output_cb;

  oop_video_decoder()->Initialize(
      config, low_delay, cdm_context, std::move(init_cb),
      base::BindRepeating(&MojoStableVideoDecoder::OnFrameResourceDecoded,
                          weak_this_factory_.GetWeakPtr()),
      waiting_cb);
}

void MojoStableVideoDecoder::OnFrameResourceDecoded(
    scoped_refptr<FrameResource> frame_resource) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(b/327268445): convert the |frame_resource| to a gpu::Mailbox-backed
  // VideoFrame in order to call |output_cb_|.
  NOTIMPLEMENTED();
}

OOPVideoDecoder* MojoStableVideoDecoder::oop_video_decoder() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return static_cast<OOPVideoDecoder*>(oop_video_decoder_.get());
}

const OOPVideoDecoder* MojoStableVideoDecoder::oop_video_decoder() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return static_cast<const OOPVideoDecoder*>(oop_video_decoder_.get());
}

}  // namespace media
