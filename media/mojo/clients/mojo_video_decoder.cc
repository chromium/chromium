// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/clients/mojo_video_decoder.h"

#include "base/check.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "media/base/decoder_buffer.h"
#include "media/base/demuxer_stream.h"
#include "media/base/media_switches.h"
#include "media/base/overlay_info.h"
#include "media/base/supported_types.h"
#include "media/base/video_frame.h"
#include "media/media_buildflags.h"
#include "media/mojo/clients/mojo_media_log_service.h"
#include "media/mojo/common/media_type_converters.h"
#include "media/mojo/common/mojo_decoder_buffer_converter.h"
#include "media/mojo/mojom/media_types.mojom.h"
#include "media/video/gpu_video_accelerator_factories.h"
#include "media/video/video_decode_accelerator.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/bindings/shared_remote.h"

namespace media {

// Provides a thread-safe channel for VideoFrame destruction events.
class MojoVideoFrameHandleReleaser
    : public base::RefCountedThreadSafe<MojoVideoFrameHandleReleaser> {
 public:
  REQUIRE_ADOPTION_FOR_REFCOUNTED_TYPE();

  MojoVideoFrameHandleReleaser(
      mojo::PendingRemote<mojom::VideoFrameHandleReleaser>
          video_frame_handle_releaser_remote,
      scoped_refptr<base::SequencedTaskRunner> task_runner) {
    // Connection errors are not handled because we wouldn't do anything
    // differently. ("If a tree falls in a forest...")
    video_frame_handle_releaser_ =
        mojo::SharedRemote<mojom::VideoFrameHandleReleaser>(
            std::move(video_frame_handle_releaser_remote),
            std::move(task_runner));
  }

  MojoVideoFrameHandleReleaser(const MojoVideoFrameHandleReleaser&) = delete;
  MojoVideoFrameHandleReleaser& operator=(const MojoVideoFrameHandleReleaser&) =
      delete;

  void ReleaseVideoFrame(const base::UnguessableToken& release_token,
                         const gpu::SyncToken& release_sync_token) {
    TRACE_EVENT1("media", "MojoVideoFrameHandleReleaser::ReleaseVideoFrame",
                 "release_token", release_token.ToString());
    DVLOG(3) << __func__ << "(" << release_token << ")";
    video_frame_handle_releaser_->ReleaseVideoFrame(release_token,
                                                    release_sync_token);
  }

  // Create a ReleaseMailboxCB that calls Release(). Since the callback holds a
  // reference to |this|, |this| will remain alive as long as there are
  // outstanding VideoFrames.
  VideoFrame::ReleaseMailboxCB CreateReleaseMailboxCB(
      const base::UnguessableToken& release_token) {
    DVLOG(3) << __func__ << "(" << release_token.ToString() << ")";
    return base::BindOnce(&MojoVideoFrameHandleReleaser::ReleaseVideoFrame,
                          this, release_token);
  }

 private:
  friend class base::RefCountedThreadSafe<MojoVideoFrameHandleReleaser>;
  ~MojoVideoFrameHandleReleaser() {}

  mojo::SharedRemote<mojom::VideoFrameHandleReleaser>
      video_frame_handle_releaser_;
};

MojoVideoDecoder::MojoVideoDecoder(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    GpuVideoAcceleratorFactories* gpu_factories,
    MediaLog* media_log,
    mojo::PendingRemote<mojom::VideoDecoder> pending_remote_decoder,
    RequestOverlayInfoCB request_overlay_info_cb,
    const gfx::ColorSpace& target_color_space)
    : task_runner_(task_runner),
      pending_remote_decoder_(std::move(pending_remote_decoder)),
      gpu_factories_(gpu_factories),
      media_log_(media_log),
      timestamps_(128),
      writer_capacity_(
          GetDefaultDecoderBufferConverterCapacity(DemuxerStream::VIDEO)),
      request_overlay_info_cb_(std::move(request_overlay_info_cb)),
      target_color_space_(target_color_space) {
  DVLOG(1) << __func__;
  DETACH_FROM_SEQUENCE(sequence_checker_);
  weak_this_ = weak_factory_.GetWeakPtr();
}

MojoVideoDecoder::~MojoVideoDecoder() {
  DVLOG(1) << __func__;
  if (request_overlay_info_cb_ && overlay_info_requested_)
    request_overlay_info_cb_.Run(false, base::NullCallback());
}

bool MojoVideoDecoder::IsPlatformDecoder() const {
  return true;
}

bool MojoVideoDecoder::SupportsDecryption() const {
  // Currently only the Android backends and specific ChromeOS configurations
  // support decryption.
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(USE_CHROMEOS_PROTECTED_MEDIA)
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kLacrosUseChromeosProtectedMedia)) {
    return false;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
  return true;
#else
  return false;
#endif
}

VideoDecoderType MojoVideoDecoder::GetDecoderType() const {
  return decoder_type_;
}

void MojoVideoDecoder::FailInit(InitCB init_cb, DecoderStatus err) {
  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(std::move(init_cb), std::move(err)));
}

void MojoVideoDecoder::Initialize(const VideoDecoderConfig& config,
                                  bool low_delay,
                                  CdmContext* cdm_context,
                                  InitCB init_cb,
                                  const OutputCB& output_cb,
                                  const WaitingCB& waiting_cb) {
  DVLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (gpu_factories_)
    decoder_type_ = gpu_factories_->GetDecoderType();

  // If the codec has software fallback, fail immediately if we know that the
  // remote side cannot support |config|.
  if (gpu_factories_ &&
      gpu_factories_->IsDecoderConfigSupported(config) ==
          GpuVideoAcceleratorFactories::Supported::kFalse &&
      IsBuiltInVideoCodec(config.codec())) {
    FailInit(std::move(init_cb), DecoderStatus::Codes::kUnsupportedConfig);
    return;
  }

  std::optional<base::UnguessableToken> cdm_id =
      cdm_context ? cdm_context->GetCdmId() : std::nullopt;

  // Fail immediately if the stream is encrypted but |cdm_id| is invalid.
  // This check is needed to avoid unnecessary IPC to the remote process.
  // Note that we do not support unsetting a CDM, so it should never happen
  // that a valid CDM ID is available on first initialization but an invalid
  // is passed for reinitialization.
  if (config.is_encrypted() && !cdm_id) {
    DVLOG(1) << __func__ << ": Invalid CdmContext.";
    FailInit(std::move(init_cb),
             DecoderStatus::Codes::kUnsupportedEncryptionMode);
    return;
  }

  initialized_ = false;
  init_cb_ = std::move(init_cb);
  output_cb_ = output_cb;
  waiting_cb_ = waiting_cb;

  if (!remote_decoder_bound_) {
    InitAndBindRemoteDecoder(
        base::BindOnce(&MojoVideoDecoder::InitializeRemoteDecoder, weak_this_,
                       config, low_delay, std::move(cdm_id)));
    return;
  }

  InitializeRemoteDecoder(config, low_delay, std::move(cdm_id));
}

void MojoVideoDecoder::InitializeRemoteDecoder(
    const VideoDecoderConfig& config,
    bool low_delay,
    std::optional<base::UnguessableToken> cdm_id) {
  if (has_connection_error_) {
    DCHECK(init_cb_);
    FailInit(std::move(init_cb_), DecoderStatus::Codes::kDisconnected);
    return;
  }

  remote_decoder_->Initialize(
      config, low_delay, cdm_id,
      base::BindOnce(&MojoVideoDecoder::OnInitializeDone,
                     base::Unretained(this)));
}

void MojoVideoDecoder::OnInitializeDone(const DecoderStatus& status,
                                        bool needs_bitstream_conversion,
                                        int32_t max_decode_requests,
                                        VideoDecoderType decoder_type) {
  DVLOG(1) << __func__ << ": status = " << status.group() << ":"
           << static_cast<int>(status.code());
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  initialized_ = status.is_ok();
  needs_bitstream_conversion_ = needs_bitstream_conversion;
  max_decode_requests_ = max_decode_requests;
  decoder_type_ = decoder_type;
  std::move(init_cb_).Run(status);
}

void MojoVideoDecoder::Decode(scoped_refptr<DecoderBuffer> buffer,
                              DecodeCB decode_cb) {
  DVLOG(3) << __func__ << ": " << buffer->AsHumanReadableString();
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT1("media", "MojoVideoDecoder::Decode", "buffer",
               buffer->AsHumanReadableString());

  if (has_connection_error_) {
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(std::move(decode_cb),
                                          DecoderStatus::Codes::kDisconnected));
    return;
  }

  if (!buffer->end_of_stream()) {
    timestamps_.Put(buffer->timestamp().InMilliseconds(),
                    base::TimeTicks::Now());
  }

  mojom::DecoderBufferPtr mojo_buffer =
      mojo_decoder_buffer_writer_->WriteDecoderBuffer(std::move(buffer));
  if (!mojo_buffer) {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(decode_cb),
                       DecoderStatus::Codes::kFailedToGetDecoderBuffer));
    return;
  }

  uint64_t decode_id = decode_counter_++;
  pending_decodes_[decode_id] = std::move(decode_cb);
  remote_decoder_->Decode(std::move(mojo_buffer),
                          base::BindOnce(&MojoVideoDecoder::OnDecodeDone,
                                         base::Unretained(this), decode_id));
}

void MojoVideoDecoder::OnVideoFrameDecoded(
    const scoped_refptr<VideoFrame>& frame,
    bool can_read_without_stalling,
    const std::optional<base::UnguessableToken>& release_token) {
  DVLOG(3) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT2("media", "MojoVideoDecoder::OnVideoFrameDecoded", "frame",
               frame->AsHumanReadableString(), "release_token",
               release_token ? release_token->ToString() : "null");
  // TODO(sandersd): Prove that all paths read this value again after running
  // |output_cb_|. In practice this isn't very important, since all decoders
  // running via MojoVideoDecoder currently use a static value.
  can_read_without_stalling_ = can_read_without_stalling;

  if (release_token) {
    frame->SetReleaseMailboxCB(
        mojo_video_frame_handle_releaser_->CreateReleaseMailboxCB(
            release_token.value()));
  }
  const int64_t timestamp = frame->timestamp().InMilliseconds();
  const auto timestamp_it = timestamps_.Peek(timestamp);
  if (timestamp_it != timestamps_.end()) {
    const auto decode_start_time = timestamp_it->second;
    const auto decode_end_time = base::TimeTicks::Now();

    TRACE_EVENT_NESTABLE_ASYNC_BEGIN_WITH_TIMESTAMP0(
        "media", "MojoVideoDecoder::Decode", timestamp, decode_start_time);
    TRACE_EVENT_NESTABLE_ASYNC_END_WITH_TIMESTAMP1(
        "media", "MojoVideoDecoder::Decode", timestamp, decode_end_time,
        "timestamp", timestamp);
    UMA_HISTOGRAM_TIMES("Media.MojoVideoDecoder.Decode",
                        decode_end_time - decode_start_time);
  }

  output_cb_.Run(frame);
}

void MojoVideoDecoder::OnDecodeDone(uint64_t decode_id,
                                    const DecoderStatus& status) {
  DVLOG(3) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto it = pending_decodes_.find(decode_id);
  if (it == pending_decodes_.end()) {
    DLOG(ERROR) << "Decode request " << decode_id << " not found";
    Stop();
    return;
  }

  DecodeCB decode_cb = std::move(it->second);
  pending_decodes_.erase(it);
  std::move(decode_cb).Run(status);
}

void MojoVideoDecoder::Reset(base::OnceClosure reset_cb) {
  DVLOG(2) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!reset_cb_);

  if (has_connection_error_) {
    task_runner_->PostTask(FROM_HERE, std::move(reset_cb));
    return;
  }

  reset_cb_ = std::move(reset_cb);
  remote_decoder_->Reset(
      base::BindOnce(&MojoVideoDecoder::OnResetDone, base::Unretained(this)));
}

void MojoVideoDecoder::OnResetDone() {
  DVLOG(2) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(pending_decodes_.empty());
  can_read_without_stalling_ = true;
  std::move(reset_cb_).Run();
}

bool MojoVideoDecoder::NeedsBitstreamConversion() const {
  DVLOG(3) << __func__;
  DCHECK(initialized_);
  return needs_bitstream_conversion_;
}

bool MojoVideoDecoder::CanReadWithoutStalling() const {
  DVLOG(3) << __func__;
  return can_read_without_stalling_;
}

int MojoVideoDecoder::GetMaxDecodeRequests() const {
  DVLOG(3) << __func__;
  DCHECK(initialized_);
  return max_decode_requests_;
}

void MojoVideoDecoder::InitAndBindRemoteDecoder(base::OnceClosure complete_cb) {
  DVLOG(3) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!remote_decoder_bound_);

  remote_decoder_.Bind(std::move(pending_remote_decoder_));
  remote_decoder_bound_ = true;

  remote_decoder_.set_disconnect_handler(
      base::BindOnce(&MojoVideoDecoder::Stop, base::Unretained(this)));

  // Generate |command_buffer_id|.
  media::mojom::CommandBufferIdPtr command_buffer_id;

  if (gpu_factories_) {
    DCHECK(complete_cb);
    gpu_factories_->GetChannelToken(
        base::BindOnce(&MojoVideoDecoder::OnChannelTokenReady, weak_this_,
                       std::move(command_buffer_id), std::move(complete_cb)));
    return;
  }

  DCHECK(complete_cb);
  InitAndConstructRemoteDecoder(std::move(command_buffer_id),
                                std::move(complete_cb));
}

void MojoVideoDecoder::OnChannelTokenReady(
    media::mojom::CommandBufferIdPtr command_buffer_id,
    base::OnceClosure complete_cb,
    const base::UnguessableToken& channel_token) {
  if (channel_token) {
    command_buffer_id = media::mojom::CommandBufferId::New();
    command_buffer_id->channel_token = std::move(channel_token);
    command_buffer_id->route_id = gpu_factories_->GetCommandBufferRouteId();
  }
  InitAndConstructRemoteDecoder(std::move(command_buffer_id),
                                std::move(complete_cb));
}

void MojoVideoDecoder::InitAndConstructRemoteDecoder(
    media::mojom::CommandBufferIdPtr command_buffer_id,
    base::OnceClosure complete_cb) {
  // Create |video_frame_handle_releaser| interface receiver, and bind
  // |mojo_video_frame_handle_releaser_| to it.
  mojo::PendingRemote<mojom::VideoFrameHandleReleaser>
      video_frame_handle_releaser_pending_remote;
  mojo::PendingReceiver<mojom::VideoFrameHandleReleaser>
      video_frame_handle_releaser_receiver =
          video_frame_handle_releaser_pending_remote
              .InitWithNewPipeAndPassReceiver();
  mojo_video_frame_handle_releaser_ =
      base::MakeRefCounted<MojoVideoFrameHandleReleaser>(
          std::move(video_frame_handle_releaser_pending_remote), task_runner_);

  mojo::ScopedDataPipeConsumerHandle remote_consumer_handle;
  mojo_decoder_buffer_writer_ = MojoDecoderBufferWriter::Create(
      writer_capacity_, &remote_consumer_handle);

  // Use `mojo::MakeSelfOwnedReceiver` for MediaLog so logs may go through even
  // after `MojoVideoDecoder` is destructed.
  mojo::PendingReceiver<mojom::MediaLog> media_log_pending_receiver;
  auto media_log_pending_remote =
      media_log_pending_receiver.InitWithNewPipeAndPassRemote();
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<MojoMediaLogService>(media_log_->Clone()),
      std::move(media_log_pending_receiver));

  remote_decoder_->Construct(client_receiver_.BindNewEndpointAndPassRemote(),
                             std::move(media_log_pending_remote),
                             std::move(video_frame_handle_releaser_receiver),
                             std::move(remote_consumer_handle),
                             std::move(command_buffer_id), target_color_space_);
  std::move(complete_cb).Run();
}

void MojoVideoDecoder::OnWaiting(WaitingReason reason) {
  DVLOG(2) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  waiting_cb_.Run(reason);
}

void MojoVideoDecoder::RequestOverlayInfo(bool restart_for_transitions) {
  DVLOG(2) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(request_overlay_info_cb_);

  overlay_info_requested_ = true;
  request_overlay_info_cb_.Run(
      restart_for_transitions,
      base::BindPostTaskToCurrentDefault(base::BindRepeating(
          &MojoVideoDecoder::OnOverlayInfoChanged, weak_this_)));
}

void MojoVideoDecoder::OnOverlayInfoChanged(const OverlayInfo& overlay_info) {
  DVLOG(2) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (has_connection_error_)
    return;
  remote_decoder_->OnOverlayInfoChanged(overlay_info);
}

void MojoVideoDecoder::Stop() {
  DVLOG(2) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  has_connection_error_ = true;

  // |init_cb_| is likely to reentrantly destruct |this|, so we check for that
  // using an on-stack WeakPtr.
  // TODO(sandersd): Update the VideoDecoder API to be explicit about what
  // reentrancy is allowed, and therefore which callbacks must be posted.
  base::WeakPtr<MojoVideoDecoder> weak_this = weak_this_;

  if (init_cb_)
    std::move(init_cb_).Run(DecoderStatus::Codes::kDisconnected);

  if (!weak_this)
    return;

  for (auto& pending_decode : pending_decodes_) {
    // It would be ideal if we could get a reason for the interruption.
    std::move(pending_decode.second).Run(DecoderStatus::Codes::kDisconnected);
    if (!weak_this)
      return;
  }
  pending_decodes_.clear();

  if (reset_cb_)
    std::move(reset_cb_).Run();

  // Drop any outstanding callbacks.
  weak_factory_.InvalidateWeakPtrs();
}

}  // namespace media
