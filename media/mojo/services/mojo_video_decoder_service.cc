// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/mojo_video_decoder_service.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/optional.h"
#include "base/threading/thread_task_runner_handle.h"
#include "media/base/cdm_context.h"
#include "media/base/decoder_buffer.h"
#include "media/base/simple_sync_token_client.h"
#include "media/base/video_decoder.h"
#include "media/base/video_decoder_config.h"
#include "media/base/video_frame.h"
#include "media/mojo/common/media_type_converters.h"
#include "media/mojo/common/mojo_decoder_buffer_converter.h"
#include "media/mojo/services/mojo_cdm_service_context.h"
#include "media/mojo/services/mojo_media_client.h"
#include "media/mojo/services/mojo_media_log.h"
#include "mojo/public/c/system/types.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/system/buffer.h"
#include "mojo/public/cpp/system/handle.h"

namespace media {

namespace {

// Number of active (Decode() was called at least once)
// MojoVideoDecoderService instances that are alive.
//
// Since MojoVideoDecoderService is constructed only by the MediaFactory,
// this will only ever be accessed from a single thread.
static int32_t g_num_active_mvd_instances = 0;

const char kInitializeTraceName[] = "MojoVideoDecoderService::Initialize";
const char kDecodeTraceName[] = "MojoVideoDecoderService::Decode";
const char kResetTraceName[] = "MojoVideoDecoderService::Reset";

}  // namespace

class VideoFrameHandleReleaserImpl final
    : public mojom::VideoFrameHandleReleaser {
 public:
  VideoFrameHandleReleaserImpl() { DVLOG(3) << __func__; }

  ~VideoFrameHandleReleaserImpl() final { DVLOG(3) << __func__; }

  // Register a VideoFrame to recieve release callbacks. A reference to |frame|
  // will be held until the remote client calls ReleaseVideoFrame() or is
  // disconnected.
  //
  // Returns an UnguessableToken which the client must use to release the
  // VideoFrame.
  base::UnguessableToken RegisterVideoFrame(scoped_refptr<VideoFrame> frame) {
    base::UnguessableToken token = base::UnguessableToken::Create();
    DVLOG(3) << __func__ << " => " << token.ToString();
    video_frames_[token] = std::move(frame);
    return token;
  }

  // mojom::MojoVideoFrameHandleReleaser implementation
  void ReleaseVideoFrame(const base::UnguessableToken& release_token,
                         const gpu::SyncToken& release_sync_token) final {
    DVLOG(3) << __func__ << "(" << release_token.ToString() << ")";
    auto it = video_frames_.find(release_token);
    if (it == video_frames_.end()) {
      mojo::ReportBadMessage("Unknown |release_token|.");
      return;
    }
    SimpleSyncTokenClient client(release_sync_token);
    it->second->UpdateReleaseSyncToken(&client);
    video_frames_.erase(it);
  }

 private:
  // TODO(sandersd): Also track age, so that an overall limit can be enforced.
  std::map<base::UnguessableToken, scoped_refptr<VideoFrame>> video_frames_;

  DISALLOW_COPY_AND_ASSIGN(VideoFrameHandleReleaserImpl);
};

MojoVideoDecoderService::MojoVideoDecoderService(
    MojoMediaClient* mojo_media_client,
    MojoCdmServiceContext* mojo_cdm_service_context)
    : mojo_media_client_(mojo_media_client),
      mojo_cdm_service_context_(mojo_cdm_service_context) {
  DVLOG(1) << __func__;
  DCHECK(mojo_media_client_);
  DCHECK(mojo_cdm_service_context_);
  weak_this_ = weak_factory_.GetWeakPtr();
}

MojoVideoDecoderService::~MojoVideoDecoderService() {
  DVLOG(1) << __func__;

  if (init_cb_)
    OnDecoderInitialized(false);
  if (reset_cb_)
    OnDecoderReset();

  if (is_active_instance_)
    g_num_active_mvd_instances--;
}

void MojoVideoDecoderService::GetSupportedConfigs(
    GetSupportedConfigsCallback callback) {
  DVLOG(3) << __func__;
  TRACE_EVENT0("media", "MojoVideoDecoderService::GetSupportedConfigs");

  std::move(callback).Run(
      mojo_media_client_->GetSupportedVideoDecoderConfigs());
}

void MojoVideoDecoderService::Construct(
    mojo::PendingAssociatedRemote<mojom::VideoDecoderClient> client,
    mojo::PendingAssociatedRemote<mojom::MediaLog> media_log,
    mojo::PendingReceiver<mojom::VideoFrameHandleReleaser>
        video_frame_handle_releaser_receiver,
    mojo::ScopedDataPipeConsumerHandle decoder_buffer_pipe,
    mojom::CommandBufferIdPtr command_buffer_id,
    VideoDecoderImplementation implementation,
    const gfx::ColorSpace& target_color_space) {
  DVLOG(1) << __func__;
  TRACE_EVENT0("media", "MojoVideoDecoderService::Construct");

  if (decoder_) {
    mojo::ReportBadMessage("Construct() already called");
    return;
  }

  client_.Bind(std::move(client));

  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      base::ThreadTaskRunnerHandle::Get();

  media_log_ =
      std::make_unique<MojoMediaLog>(std::move(media_log), task_runner);

  video_frame_handle_releaser_ = mojo::MakeSelfOwnedReceiver(
      std::make_unique<VideoFrameHandleReleaserImpl>(),
      std::move(video_frame_handle_releaser_receiver));

  mojo_decoder_buffer_reader_.reset(
      new MojoDecoderBufferReader(std::move(decoder_buffer_pipe)));

  decoder_ = mojo_media_client_->CreateVideoDecoder(
      task_runner, media_log_.get(), std::move(command_buffer_id),
      implementation,
      base::BindRepeating(
          &MojoVideoDecoderService::OnDecoderRequestedOverlayInfo, weak_this_),
      target_color_space);
}

void MojoVideoDecoderService::Initialize(const VideoDecoderConfig& config,
                                         bool low_delay,
                                         int32_t cdm_id,
                                         InitializeCallback callback) {
  DVLOG(1) << __func__ << " config = " << config.AsHumanReadableString()
           << ", cdm_id = " << cdm_id;
  DCHECK(!init_cb_);
  DCHECK(callback);

  TRACE_EVENT_ASYNC_BEGIN2("media", kInitializeTraceName, this, "config",
                           config.AsHumanReadableString(), "cdm_id", cdm_id);

  init_cb_ = std::move(callback);

  if (!decoder_) {
    OnDecoderInitialized(false);
    return;
  }

  // Get CdmContext from |cdm_id|, which could be null.
  CdmContext* cdm_context = nullptr;
  if (cdm_id != CdmContext::kInvalidCdmId) {
    auto cdm_context_ref = mojo_cdm_service_context_->GetCdmContextRef(cdm_id);
    if (cdm_context_ref) {
      // |cdm_context_ref_| must be kept as long as |cdm_context| is used by the
      // |decoder_|.
      cdm_context_ref_ = std::move(cdm_context_ref);
      cdm_context = cdm_context_ref_->GetCdmContext();
      DCHECK(cdm_context);
    }
  }

  if (config.is_encrypted() && !cdm_context) {
    DVLOG(1) << "CdmContext for " << cdm_id << " not found for encrypted video";
    OnDecoderInitialized(false);
    return;
  }

  using Self = MojoVideoDecoderService;
  decoder_->Initialize(
      config, low_delay, cdm_context,
      base::BindRepeating(&Self::OnDecoderInitialized, weak_this_),
      base::BindRepeating(&Self::OnDecoderOutput, weak_this_),
      base::BindRepeating(&Self::OnDecoderWaiting, weak_this_));
}

void MojoVideoDecoderService::Decode(mojom::DecoderBufferPtr buffer,
                                     DecodeCallback callback) {
  DVLOG(3) << __func__ << " pts=" << buffer->timestamp.InMilliseconds();
  DCHECK(callback);

  std::unique_ptr<ScopedDecodeTrace> trace_event;
  if (ScopedDecodeTrace::IsEnabled()) {
    // Because multiple Decode() calls may be in flight, each call needs a
    // unique trace event class to identify it. This scoped event is bound
    // into the OnDecodeDone callback to ensure the trace is always closed.
    trace_event = std::make_unique<ScopedDecodeTrace>(
        kDecodeTraceName, buffer->is_key_frame, buffer->timestamp);
  }

  if (!decoder_) {
    OnDecoderDecoded(std::move(callback), std::move(trace_event),
                     DecodeStatus::DECODE_ERROR);
    return;
  }

  if (!is_active_instance_) {
    is_active_instance_ = true;
    g_num_active_mvd_instances++;
    UMA_HISTOGRAM_EXACT_LINEAR("Media.MojoVideoDecoder.ActiveInstances",
                               g_num_active_mvd_instances, 64);
  }

  mojo_decoder_buffer_reader_->ReadDecoderBuffer(
      std::move(buffer),
      base::BindOnce(&MojoVideoDecoderService::OnReaderRead, weak_this_,
                     std::move(callback), std::move(trace_event)));
}

void MojoVideoDecoderService::Reset(ResetCallback callback) {
  DVLOG(2) << __func__;
  TRACE_EVENT_ASYNC_BEGIN0("media", kResetTraceName, this);
  DCHECK(callback);
  DCHECK(!reset_cb_);

  reset_cb_ = std::move(callback);

  if (!decoder_) {
    OnDecoderReset();
    return;
  }

  // Flush the reader so that pending decodes will be dispatched first.
  mojo_decoder_buffer_reader_->Flush(base::BindRepeating(
      &MojoVideoDecoderService::OnReaderFlushed, weak_this_));
}

void MojoVideoDecoderService::OnDecoderInitialized(bool success) {
  DVLOG(1) << __func__;
  DCHECK(!success || decoder_);
  DCHECK(init_cb_);
  TRACE_EVENT_ASYNC_END1("media", kInitializeTraceName, this, "success",
                         success);

  if (!success)
    cdm_context_ref_.reset();

  std::move(init_cb_).Run(
      success, success ? decoder_->NeedsBitstreamConversion() : false,
      success ? decoder_->GetMaxDecodeRequests() : 1);
}

void MojoVideoDecoderService::OnReaderRead(
    DecodeCallback callback,
    std::unique_ptr<ScopedDecodeTrace> trace_event,
    scoped_refptr<DecoderBuffer> buffer) {
  DVLOG(3) << __func__;
  if (trace_event) {
    TRACE_EVENT_ASYNC_STEP_PAST1(
        "media", kDecodeTraceName, trace_event.get(), "ReadDecoderBuffer",
        "decoder_buffer", buffer ? buffer->AsHumanReadableString() : "null");
  }

  if (!buffer) {
    OnDecoderDecoded(std::move(callback), std::move(trace_event),
                     DecodeStatus::DECODE_ERROR);
    return;
  }

  decoder_->Decode(
      buffer, base::BindRepeating(&MojoVideoDecoderService::OnDecoderDecoded,
                                  weak_this_, base::Passed(&callback),
                                  base::Passed(&trace_event)));
}

void MojoVideoDecoderService::OnReaderFlushed() {
  decoder_->Reset(base::BindRepeating(&MojoVideoDecoderService::OnDecoderReset,
                                      weak_this_));
}

void MojoVideoDecoderService::OnDecoderDecoded(
    DecodeCallback callback,
    std::unique_ptr<ScopedDecodeTrace> trace_event,
    DecodeStatus status) {
  DVLOG(3) << __func__;
  if (trace_event) {
    TRACE_EVENT_ASYNC_STEP_PAST0("media", kDecodeTraceName, trace_event.get(),
                                 "Decode");
    trace_event->EndTrace(status);
  }

  std::move(callback).Run(status);
}

void MojoVideoDecoderService::OnDecoderReset() {
  DVLOG(2) << __func__;
  DCHECK(reset_cb_);
  TRACE_EVENT_ASYNC_END0("media", kResetTraceName, this);
  std::move(reset_cb_).Run();
}

void MojoVideoDecoderService::OnDecoderOutput(scoped_refptr<VideoFrame> frame) {
  DVLOG(3) << __func__ << " pts=" << frame->timestamp().InMilliseconds();
  DCHECK(client_);
  DCHECK(decoder_);
  TRACE_EVENT1("media", "MojoVideoDecoderService::OnDecoderOutput",
               "video_frame", frame->AsHumanReadableString())

  // All MojoVideoDecoder-based decoders are hardware decoders. If you're the
  // first to implement an out-of-process decoder that is not power efficent,
  // you can remove this DCHECK.
  DCHECK(frame->metadata()->IsTrue(VideoFrameMetadata::POWER_EFFICIENT));

  base::Optional<base::UnguessableToken> release_token;
  if (frame->HasReleaseMailboxCB() && video_frame_handle_releaser_) {
    // |video_frame_handle_releaser_| is explicitly constructed with a
    // VideoFrameHandleReleaserImpl in Construct().
    VideoFrameHandleReleaserImpl* releaser =
        static_cast<VideoFrameHandleReleaserImpl*>(
            video_frame_handle_releaser_->impl());
    release_token = releaser->RegisterVideoFrame(frame);
  }

  client_->OnVideoFrameDecoded(std::move(frame),
                               decoder_->CanReadWithoutStalling(),
                               std::move(release_token));
}

void MojoVideoDecoderService::OnDecoderWaiting(WaitingReason reason) {
  DVLOG(3) << __func__;
  DCHECK(client_);
  TRACE_EVENT1("media", "MojoVideoDecoderService::OnDecoderWaiting", "reason",
               static_cast<int>(reason));
  client_->OnWaiting(reason);
}

void MojoVideoDecoderService::OnOverlayInfoChanged(
    const OverlayInfo& overlay_info) {
  DVLOG(2) << __func__;
  DCHECK(client_);
  DCHECK(decoder_);
  DCHECK(provide_overlay_info_cb_);
  TRACE_EVENT0("media", "MojoVideoDecoderService::OnOverlayInfoChanged");
  provide_overlay_info_cb_.Run(overlay_info);
}

void MojoVideoDecoderService::OnDecoderRequestedOverlayInfo(
    bool restart_for_transitions,
    const ProvideOverlayInfoCB& provide_overlay_info_cb) {
  DVLOG(2) << __func__;
  DCHECK(client_);
  DCHECK(decoder_);
  DCHECK(!provide_overlay_info_cb_);
  TRACE_EVENT0("media",
               "MojoVideoDecoderService::OnDecoderRequestedOverlayInfo");

  provide_overlay_info_cb_ = std::move(provide_overlay_info_cb);
  client_->RequestOverlayInfo(restart_for_transitions);
}

}  // namespace media
