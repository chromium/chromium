// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/clients/mojo_video_decoder.h"

#include <atomic>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback_helpers.h"
#include "base/feature_list.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/single_thread_task_runner.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/decoder_buffer.h"
#include "media/base/demuxer_stream.h"
#include "media/base/media_switches.h"
#include "media/base/overlay_info.h"
#include "media/base/video_frame.h"
#include "media/mojo/common/media_type_converters.h"
#include "media/mojo/common/mojo_decoder_buffer_converter.h"
#include "media/mojo/mojom/media_types.mojom.h"
#include "media/video/gpu_video_accelerator_factories.h"
#include "media/video/video_decode_accelerator.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/shared_remote.h"

namespace media {

namespace {
// Number of functional instances of MojoVideoDecoder in the current process.
std::atomic<int>& get_mojo_instance_counter() {
  static base::NoDestructor<std::atomic<int>> gInstanceCounter(0);
  return *gInstanceCounter;
}
}  // namespace

const char kMojoVideoDecoderInitialPlaybackSuccessCodecCounterUMA[] =
    "Media.MojoVideoDecoder.InitialPlaybackSuccessCodecCounter";

const char kMojoVideoDecoderInitialPlaybackErrorCodecCounterUMA[] =
    "Media.MojoVideoDecoder.InitialPlaybackErrorCodecCounter";

const int kMojoDecoderInitialPlaybackFrameCount = 150;

// Provides a thread-safe channel for VideoFrame destruction events.
class MojoVideoFrameHandleReleaser
    : public base::RefCountedThreadSafe<MojoVideoFrameHandleReleaser> {
 public:
  REQUIRE_ADOPTION_FOR_REFCOUNTED_TYPE();

  MojoVideoFrameHandleReleaser(
      mojo::PendingRemote<mojom::VideoFrameHandleReleaser>
          video_frame_handle_releaser_remote,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
    // Connection errors are not handled because we wouldn't do anything
    // differently. ("If a tree falls in a forest...")
    video_frame_handle_releaser_ =
        mojo::SharedRemote<mojom::VideoFrameHandleReleaser>(
            std::move(video_frame_handle_releaser_remote),
            std::move(task_runner));
  }

  void ReleaseVideoFrame(const base::UnguessableToken& release_token,
                         const gpu::SyncToken& release_sync_token) {
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

  DISALLOW_COPY_AND_ASSIGN(MojoVideoFrameHandleReleaser);
};

MojoVideoDecoder::MojoVideoDecoder(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    GpuVideoAcceleratorFactories* gpu_factories,
    MediaLog* media_log,
    mojo::PendingRemote<mojom::VideoDecoder> pending_remote_decoder,
    VideoDecoderImplementation implementation,
    RequestOverlayInfoCB request_overlay_info_cb,
    const gfx::ColorSpace& target_color_space)
    : task_runner_(task_runner),
      pending_remote_decoder_(std::move(pending_remote_decoder)),
      gpu_factories_(gpu_factories),
      timestamps_(128),
      writer_capacity_(
          GetDefaultDecoderBufferConverterCapacity(DemuxerStream::VIDEO)),
      media_log_service_(media_log),
      media_log_receiver_(&media_log_service_),
      request_overlay_info_cb_(std::move(request_overlay_info_cb)),
      target_color_space_(target_color_space),
      video_decoder_implementation_(implementation) {
  DVLOG(1) << __func__;
  weak_this_ = weak_factory_.GetWeakPtr();
}

MojoVideoDecoder::~MojoVideoDecoder() {
  DVLOG(1) << __func__;
  if (remote_decoder_bound_)
    get_mojo_instance_counter()--;
  if (request_overlay_info_cb_ && overlay_info_requested_)
    request_overlay_info_cb_.Run(false, base::NullCallback());
}

bool MojoVideoDecoder::IsPlatformDecoder() const {
  return true;
}

bool MojoVideoDecoder::SupportsDecryption() const {
  // Currently only the android backends support decryption
#if defined(OS_ANDROID)
  return true;
#else
  return false;
#endif
}

std::string MojoVideoDecoder::GetDisplayName() const {
  return "MojoVideoDecoder";
}

void MojoVideoDecoder::FailInit(InitCB init_cb, Status err) {
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
  DCHECK(task_runner_->BelongsToCurrentThread());

  // Fail immediately if we know that the remote side cannot support |config|.
  if (gpu_factories_ && gpu_factories_->IsDecoderConfigSupported(
                            video_decoder_implementation_, config) ==
                            GpuVideoAcceleratorFactories::Supported::kFalse) {
    FailInit(std::move(init_cb), StatusCode::kDecoderUnsupportedConfig);
    return;
  }

  base::Optional<base::UnguessableToken> cdm_id =
      cdm_context ? cdm_context->GetCdmId() : base::nullopt;

  // Fail immediately if the stream is encrypted but |cdm_id| is invalid.
  // This check is needed to avoid unnecessary IPC to the remote process.
  // Note that we do not support unsetting a CDM, so it should never happen
  // that a valid CDM ID is available on first initialization but an invalid
  // is passed for reinitialization.
  if (config.is_encrypted() && !cdm_id) {
    DVLOG(1) << __func__ << ": Invalid CdmContext.";
    FailInit(std::move(init_cb),
             StatusCode::kDecoderMissingCdmForEncryptedContent);
    return;
  }

  if (!remote_decoder_bound_) {
    BindRemoteDecoder();
    get_mojo_instance_counter()++;
  }

  if (has_connection_error_) {
    FailInit(std::move(init_cb), StatusCode::kMojoDecoderNoConnection);
    return;
  }

  initialized_ = false;
  init_cb_ = std::move(init_cb);
  output_cb_ = output_cb;
  waiting_cb_ = waiting_cb;

  remote_decoder_->Initialize(
      config, low_delay, cdm_id,
      base::BindOnce(&MojoVideoDecoder::OnInitializeDone,
                     base::Unretained(this)));
}

void MojoVideoDecoder::OnInitializeDone(const Status& status,
                                        bool needs_bitstream_conversion,
                                        int32_t max_decode_requests) {
  DVLOG(1) << __func__ << ": status = " << std::hex << status.code();
  DCHECK(task_runner_->BelongsToCurrentThread());
  initialized_ = status.is_ok();
  needs_bitstream_conversion_ = needs_bitstream_conversion;
  max_decode_requests_ = max_decode_requests;
  std::move(init_cb_).Run(status);
}

void MojoVideoDecoder::Decode(scoped_refptr<DecoderBuffer> buffer,
                              DecodeCB decode_cb) {
  DVLOG(3) << __func__ << ": " << buffer->AsHumanReadableString();
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (has_connection_error_) {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(decode_cb), DecodeStatus::DECODE_ERROR));
    return;
  }

  if (!buffer->end_of_stream()) {
    timestamps_.Put(buffer->timestamp().InMilliseconds(),
                    base::TimeTicks::Now());
  }

  mojom::DecoderBufferPtr mojo_buffer =
      mojo_decoder_buffer_writer_->WriteDecoderBuffer(std::move(buffer));
  if (!mojo_buffer) {
    ReportInitialPlaybackErrorUMA();
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(decode_cb), DecodeStatus::DECODE_ERROR));
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
    const base::Optional<base::UnguessableToken>& release_token) {
  DVLOG(3) << __func__;
  DCHECK(task_runner_->BelongsToCurrentThread());

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
  total_frames_decoded_++;
  if (!initial_playback_outcome_reported_ &&
      total_frames_decoded_ >= kMojoDecoderInitialPlaybackFrameCount) {
    initial_playback_outcome_reported_ = true;
    UMA_HISTOGRAM_COUNTS_100(
        kMojoVideoDecoderInitialPlaybackSuccessCodecCounterUMA,
        get_mojo_instance_counter());
    DVLOG(3)
        << "Report Media.MojoVideoDecoder.InitialPlaybackSuccessCodecCounter:"
        << get_mojo_instance_counter();
  }
}

void MojoVideoDecoder::OnDecodeDone(uint64_t decode_id, const Status& status) {
  DVLOG(3) << __func__;
  DCHECK(task_runner_->BelongsToCurrentThread());

  auto it = pending_decodes_.find(decode_id);
  if (it == pending_decodes_.end()) {
    DLOG(ERROR) << "Decode request " << decode_id << " not found";
    Stop();
    return;
  }

  if (!status.is_ok() && status.code() != StatusCode::kAborted)
    ReportInitialPlaybackErrorUMA();

  DecodeCB decode_cb = std::move(it->second);
  pending_decodes_.erase(it);
  std::move(decode_cb).Run(status);
}

void MojoVideoDecoder::Reset(base::OnceClosure reset_cb) {
  DVLOG(2) << __func__;
  DCHECK(task_runner_->BelongsToCurrentThread());

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
  DCHECK(task_runner_->BelongsToCurrentThread());
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

void MojoVideoDecoder::BindRemoteDecoder() {
  DVLOG(3) << __func__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(!remote_decoder_bound_);

  remote_decoder_.Bind(std::move(pending_remote_decoder_));
  remote_decoder_bound_ = true;

  remote_decoder_.set_disconnect_handler(
      base::BindOnce(&MojoVideoDecoder::Stop, base::Unretained(this)));

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

  // Generate |command_buffer_id|.
  media::mojom::CommandBufferIdPtr command_buffer_id;
  if (gpu_factories_) {
    base::UnguessableToken channel_token = gpu_factories_->GetChannelToken();
    if (channel_token) {
      command_buffer_id = media::mojom::CommandBufferId::New();
      command_buffer_id->channel_token = std::move(channel_token);
      command_buffer_id->route_id = gpu_factories_->GetCommandBufferRouteId();
    }
  }

  remote_decoder_->Construct(
      client_receiver_.BindNewEndpointAndPassRemote(),
      media_log_receiver_.BindNewEndpointAndPassRemote(),
      std::move(video_frame_handle_releaser_receiver),
      std::move(remote_consumer_handle), std::move(command_buffer_id),
      video_decoder_implementation_, target_color_space_);
}

void MojoVideoDecoder::OnWaiting(WaitingReason reason) {
  DVLOG(2) << __func__;
  DCHECK(task_runner_->BelongsToCurrentThread());

  waiting_cb_.Run(reason);
}

void MojoVideoDecoder::RequestOverlayInfo(bool restart_for_transitions) {
  DVLOG(2) << __func__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(request_overlay_info_cb_);

  overlay_info_requested_ = true;
  request_overlay_info_cb_.Run(
      restart_for_transitions,
      BindToCurrentLoop(base::BindRepeating(
          &MojoVideoDecoder::OnOverlayInfoChanged, weak_this_)));
}

void MojoVideoDecoder::OnOverlayInfoChanged(const OverlayInfo& overlay_info) {
  DVLOG(2) << __func__;
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (has_connection_error_)
    return;
  remote_decoder_->OnOverlayInfoChanged(overlay_info);
}

void MojoVideoDecoder::Stop() {
  DVLOG(2) << __func__;
  DCHECK(task_runner_->BelongsToCurrentThread());

  has_connection_error_ = true;
  ReportInitialPlaybackErrorUMA();

  // |init_cb_| is likely to reentrantly destruct |this|, so we check for that
  // using an on-stack WeakPtr.
  // TODO(sandersd): Update the VideoDecoder API to be explicit about what
  // reentrancy is allowed, and therefore which callbacks must be posted.
  base::WeakPtr<MojoVideoDecoder> weak_this = weak_this_;

  if (init_cb_)
    std::move(init_cb_).Run(StatusCode::kMojoDecoderStoppedBeforeInitDone);

  if (!weak_this)
    return;

  for (auto& pending_decode : pending_decodes_) {
    std::move(pending_decode.second).Run(DecodeStatus::DECODE_ERROR);
    if (!weak_this)
      return;
  }
  pending_decodes_.clear();

  if (reset_cb_)
    std::move(reset_cb_).Run();
}

void MojoVideoDecoder::ReportInitialPlaybackErrorUMA() {
  if (initial_playback_outcome_reported_)
    return;

  DCHECK(get_mojo_instance_counter() > 0);
  DVLOG(3) << "Report Media.MojoVideoDecoder.InitialPlaybackErrorCodecCounter:"
           << get_mojo_instance_counter();

  UMA_HISTOGRAM_COUNTS_100(kMojoVideoDecoderInitialPlaybackErrorCodecCounterUMA,
                           get_mojo_instance_counter());
  initial_playback_outcome_reported_ = true;
}

}  // namespace media
