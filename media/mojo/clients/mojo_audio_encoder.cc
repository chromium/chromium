// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/clients/mojo_audio_encoder.h"

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "build/build_config.h"
#include "media/base/audio_buffer.h"
#include "media/base/media_switches.h"
#include "media/mojo/common/media_type_converters.h"

#if BUILDFLAG(IS_ANDROID)
#include "media/base/android/media_codec_util.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "base/win/windows_version.h"
#endif

namespace media {

// static
bool MojoAudioEncoder::IsSupported(AudioCodec codec) {
  switch (codec) {
    case AudioCodec::kAAC:
#if BUILDFLAG(IS_ANDROID)
      return base::FeatureList::IsEnabled(media::kPlatformAudioEncoder) &&
             MediaCodecUtil::IsAACEncoderAvailable();
#elif BUILDFLAG(IS_WIN)
      // Windows AAC encoder relies on the MediaFoundation, which is not
      // installed for Windows N Sku.
      return !base::win::OSInfo::GetInstance()->IsWindowsNSku() &&
             base::FeatureList::IsEnabled(media::kPlatformAudioEncoder);
#else
      return base::FeatureList::IsEnabled(media::kPlatformAudioEncoder);
#endif
    default:
      // We only spin up platform AudioEncoders for AAC for now.
      return false;
  }
}

MojoAudioEncoder::MojoAudioEncoder(
    mojo::PendingRemote<mojom::AudioEncoder> remote_encoder)
    : pending_remote_encoder_(std::move(remote_encoder)),
      buffer_pool_(new AudioBufferMemoryPool()),
      runner_(base::SequencedTaskRunner::GetCurrentDefault()) {
  weak_this_ = weak_factory_.GetWeakPtr();
}

MojoAudioEncoder::~MojoAudioEncoder() = default;

void MojoAudioEncoder::Initialize(const Options& options,
                                  OutputCB output_cb,
                                  EncoderStatusCB done_cb) {
  DCHECK(output_cb);
  DCHECK(done_cb);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (remote_encoder_.is_bound() || client_receiver_.is_bound()) {
    PostStatusCallback(std::move(done_cb),
                       EncoderStatus::Codes::kEncoderInitializeTwice);
    return;
  }

  BindRemote();

  if (!remote_encoder_.is_bound() || !remote_encoder_.is_connected()) {
    PostStatusCallback(std::move(done_cb),
                       EncoderStatus::Codes::kEncoderInitializationError);
    return;
  }

  output_cb_ = std::move(output_cb);
  options_ = options;
  remote_encoder_->Initialize(client_receiver_.BindNewEndpointAndPassRemote(),
                              options,
                              WrapCallbackAsPending(std::move(done_cb)));
}

void MojoAudioEncoder::Encode(std::unique_ptr<AudioBus> audio_bus,
                              base::TimeTicks capture_time,
                              EncoderStatusCB done_cb) {
  DCHECK(audio_bus);
  DCHECK(done_cb);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!remote_encoder_.is_bound() || !remote_encoder_.is_connected()) {
    PostStatusCallback(std::move(done_cb),
                       EncoderStatus::Codes::kEncoderFailedEncode);
    return;
  }

  auto buffer = AudioBuffer::CopyFrom(options_.sample_rate,
                                      capture_time - base::TimeTicks(),
                                      audio_bus.get(), buffer_pool_);

  remote_encoder_->Encode(mojom::AudioBuffer::From(*buffer),
                          WrapCallbackAsPending(std::move(done_cb)));
}

void MojoAudioEncoder::Flush(EncoderStatusCB done_cb) {
  DCHECK(done_cb);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!remote_encoder_.is_bound() || !remote_encoder_.is_connected()) {
    PostStatusCallback(std::move(done_cb),
                       EncoderStatus::Codes::kEncoderFailedFlush);
    return;
  }

  remote_encoder_->Flush(WrapCallbackAsPending(std::move(done_cb)));
}

void MojoAudioEncoder::OnEncodedBufferReady(media::EncodedAudioBuffer buffer,
                                            const CodecDescription& desc) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::optional<CodecDescription> opt_desc;
  if (desc.size() > 0)
    opt_desc = desc;
  output_cb_.Run(std::move(buffer), std::move(opt_desc));
}

void MojoAudioEncoder::CallAndReleaseCallback(PendingCallbackHandle handle,
                                              const EncoderStatus& status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!pending_callbacks_.empty()) {
    EncoderStatusCB callback = std::move(*handle);
    pending_callbacks_.erase(handle);
    std::move(callback).Run(status);
  }
}

MojoAudioEncoder::WrappedEncoderStatusCB
MojoAudioEncoder::WrapCallbackAsPending(EncoderStatusCB callback) {
  PendingCallbackHandle handle =
      pending_callbacks_.insert(pending_callbacks_.end(), std::move(callback));
  return base::BindOnce(&MojoAudioEncoder::CallAndReleaseCallback, weak_this_,
                        handle);
}

void MojoAudioEncoder::CallAndReleaseAllPendingCallbacks(EncoderStatus status) {
  for (auto& callback : pending_callbacks_)
    PostStatusCallback(std::move(callback), status);
  pending_callbacks_.clear();
}

void MojoAudioEncoder::BindRemote() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  remote_encoder_.Bind(std::move(pending_remote_encoder_));
  remote_encoder_.set_disconnect_handler(
      base::BindOnce(&MojoAudioEncoder::OnConnectionError, weak_this_));
}

void MojoAudioEncoder::OnConnectionError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!remote_encoder_.is_connected());
  CallAndReleaseAllPendingCallbacks(
      EncoderStatus::Codes::kEncoderMojoConnectionError);
  weak_factory_.InvalidateWeakPtrs();
  remote_encoder_.reset();
}

void MojoAudioEncoder::PostStatusCallback(EncoderStatusCB callback,
                                          EncoderStatus status) {
  runner_->PostTask(FROM_HERE,
                    base::BindOnce(std::move(callback), std::move(status)));
}

}  // namespace media
