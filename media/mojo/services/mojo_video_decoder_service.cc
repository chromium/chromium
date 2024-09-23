// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/mojo_video_decoder_service.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "base/types/optional_util.h"
#include "media/base/decoder_buffer.h"
#include "media/base/media_util.h"
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

base::debug::CrashKeyString* GetNumVideoDecodersCrashKeyString() {
  static base::debug::CrashKeyString* codec_count_crash_key =
      base::debug::AllocateCrashKeyString("num-video-decoders",
                                          base::debug::CrashKeySize::Size32);
  return codec_count_crash_key;
}

}  // namespace

class VideoFrameHandleReleaserImpl final
    : public mojom::VideoFrameHandleReleaser {
 public:
  VideoFrameHandleReleaserImpl() { DVLOG(3) << __func__; }

  VideoFrameHandleReleaserImpl(const VideoFrameHandleReleaserImpl&) = delete;
  VideoFrameHandleReleaserImpl& operator=(const VideoFrameHandleReleaserImpl&) =
      delete;

  ~VideoFrameHandleReleaserImpl() final { DVLOG(3) << __func__; }

  // Register a VideoFrame to receive release callbacks. A reference to |frame|
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
  void ReleaseVideoFrame(
      const base::UnguessableToken& release_token,
      const std::optional<gpu::SyncToken>& release_sync_token) final {
    DVLOG(3) << __func__ << "(" << release_token.ToString() << ")";
    TRACE_EVENT2("media", "VideoFrameHandleReleaserImpl::ReleaseVideoFrame",
                 "release_token", release_token.ToString(),
                 "release_sync_token",
                 release_sync_token
                     ? (release_sync_token->ToDebugString() + ", has_data: " +
                        (release_sync_token->HasData() ? "true" : "false"))
                     : "null");
    auto it = video_frames_.find(release_token);
    if (it == video_frames_.end()) {
      mojo::ReportBadMessage("Unknown |release_token|.");
      return;
    }
    if (it->second->HasReleaseMailboxCB()) {
      if (!release_sync_token) {
        mojo::ReportBadMessage(
            "A SyncToken is required to release frames that have a callback "
            "for releasing mailboxes.");
        return;
      }
      // An empty *|release_sync_token| can be taken as a signal that the
      // about-to-be-released VideoFrame was never used by the client.
      // Therefore, we should let that frame retain whatever SyncToken it has.
      if (release_sync_token->HasData()) {
        SimpleSyncTokenClient client(*release_sync_token);
        it->second->UpdateReleaseSyncToken(&client);
      }
    }
    video_frames_.erase(it);
  }

 private:
  // TODO(sandersd): Also track age, so that an overall limit can be enforced.
  base::flat_map<base::UnguessableToken, scoped_refptr<VideoFrame>>
      video_frames_;
};

MojoVideoDecoderService::MojoVideoDecoderService(
    MojoMediaClient* mojo_media_client,
    MojoCdmServiceContext* mojo_cdm_service_context,
    mojo::PendingRemote<stable::mojom::StableVideoDecoder>
        oop_video_decoder_pending_remote)
    : mojo_media_client_(mojo_media_client),
      mojo_cdm_service_context_(mojo_cdm_service_context),
      oop_video_decoder_pending_remote_(
          std::move(oop_video_decoder_pending_remote)) {
  DVLOG(1) << __func__;
  DCHECK(mojo_media_client_);
  DCHECK(mojo_cdm_service_context_);
  weak_this_ = weak_factory_.GetWeakPtr();
}

MojoVideoDecoderService::~MojoVideoDecoderService() {
  DVLOG(1) << __func__;

  if (init_cb_) {
    OnDecoderInitialized(DecoderStatus::Codes::kInterrupted);
  }

  if (reset_cb_)
    OnDecoderReset();

  if (is_active_instance_) {
    g_num_active_mvd_instances--;
    base::debug::SetCrashKeyString(
        GetNumVideoDecodersCrashKeyString(),
        base::NumberToString(g_num_active_mvd_instances));
  }

  // Destruct the VideoDecoder here so its destruction duration is included by
  // the histogram timer below.
  weak_factory_.InvalidateWeakPtrs();
  decoder_.reset();

  mojo_media_client_ = nullptr;
  mojo_cdm_service_context_ = nullptr;
}

void MojoVideoDecoderService::GetSupportedConfigs(
    GetSupportedConfigsCallback callback) {
  DVLOG(3) << __func__;
  TRACE_EVENT0("media", "MojoVideoDecoderService::GetSupportedConfigs");

  std::move(callback).Run(mojo_media_client_->GetSupportedVideoDecoderConfigs(),
                          mojo_media_client_->GetDecoderImplementationType());
}

void MojoVideoDecoderService::Construct(
    mojo::PendingAssociatedRemote<mojom::VideoDecoderClient> client,
    mojo::PendingRemote<mojom::MediaLog> media_log,
    mojo::PendingReceiver<mojom::VideoFrameHandleReleaser>
        video_frame_handle_releaser_receiver,
    mojo::ScopedDataPipeConsumerHandle decoder_buffer_pipe,
    mojom::CommandBufferIdPtr command_buffer_id,
    const gfx::ColorSpace& target_color_space) {
  DVLOG(1) << __func__;
  TRACE_EVENT0("media", "MojoVideoDecoderService::Construct");

  if (media_log_) {
    mojo::ReportBadMessage("Construct() already called");
    return;
  }

  client_.Bind(std::move(client));

  scoped_refptr<base::SequencedTaskRunner> task_runner =
      base::SequencedTaskRunner::GetCurrentDefault();

  media_log_ =
      std::make_unique<MojoMediaLog>(std::move(media_log), task_runner);

  video_frame_handle_releaser_ = mojo::MakeSelfOwnedReceiver(
      std::make_unique<VideoFrameHandleReleaserImpl>(),
      std::move(video_frame_handle_releaser_receiver));

  mojo_decoder_buffer_reader_ =
      std::make_unique<MojoDecoderBufferReader>(std::move(decoder_buffer_pipe));

  decoder_ = mojo_media_client_->CreateVideoDecoder(
      task_runner, media_log_.get(), std::move(command_buffer_id),
      base::BindRepeating(
          &MojoVideoDecoderService::OnDecoderRequestedOverlayInfo, weak_this_),
      target_color_space, std::move(oop_video_decoder_pending_remote_));
}

void MojoVideoDecoderService::Initialize(
    const VideoDecoderConfig& config,
    bool low_delay,
    const std::optional<base::UnguessableToken>& cdm_id,
    InitializeCallback callback) {
  DVLOG(1) << __func__ << " config = " << config.AsHumanReadableString()
           << ", cdm_id = "
           << CdmContext::CdmIdToString(base::OptionalToPtr(cdm_id));
  DCHECK(!init_cb_);
  DCHECK(callback);

  TRACE_EVENT_ASYNC_BEGIN2(
      "media", kInitializeTraceName, this, "config",
      config.AsHumanReadableString(), "cdm_id",
      CdmContext::CdmIdToString(base::OptionalToPtr(cdm_id)));

  init_cb_ = std::move(callback);

  // Prevent creation of too many hardware decoding instances since it may lead
  // to system instability. Note: This will break decoding entirely for codecs
  // which don't have software fallback, so we use a conservative limit. Most
  // platforms will self-limit and never reach this limit.
  if (!config.is_encrypted() && g_num_active_mvd_instances >= 128) {
    OnDecoderInitialized(DecoderStatus::Codes::kTooManyDecoders);
    return;
  }

  if (!decoder_) {
    OnDecoderInitialized(DecoderStatus::Codes::kFailedToCreateDecoder);
    return;
  }

  // |cdm_context_ref_| must be kept as long as |cdm_context| is used by the
  // |decoder_|. We do NOT support resetting |cdm_context_ref_| because in
  // general we don't support resetting CDM in the media pipeline.
  if (cdm_id) {
    if (!cdm_id_) {
      DCHECK(!cdm_context_ref_);
      cdm_id_ = cdm_id;
      cdm_context_ref_ =
          mojo_cdm_service_context_->GetCdmContextRef(cdm_id.value());
    } else if (cdm_id != cdm_id_) {
      // TODO(xhwang): Replace with mojo::ReportBadMessage().
      NOTREACHED_IN_MIGRATION() << "The caller should not switch CDM";
      OnDecoderInitialized(DecoderStatus::Codes::kUnsupportedEncryptionMode);
      return;
    }
  }

  // Get CdmContext, which could be null.
  CdmContext* cdm_context =
      cdm_context_ref_ ? cdm_context_ref_->GetCdmContext() : nullptr;

  if (config.is_encrypted() && !cdm_context) {
    DVLOG(1) << "CdmContext for "
             << CdmContext::CdmIdToString(base::OptionalToPtr(cdm_id))
             << " not found for encrypted video";
    OnDecoderInitialized(DecoderStatus::Codes::kUnsupportedEncryptionMode);
    return;
  }

  auto gfx_cs = config.color_space_info().ToGfxColorSpace();
  codec_string_ = base::StringPrintf(
      "name=%s:codec=%s:profile=%d:size=%s:cs=[%d,%d,%d,%d]:hdrm=%d",
      GetDecoderName(decoder_->GetDecoderType()).c_str(),
      GetCodecName(config.codec()).c_str(), config.profile(),
      config.coded_size().ToString().c_str(),
      static_cast<int>(gfx_cs.GetPrimaryID()),
      static_cast<int>(gfx_cs.GetTransferID()),
      static_cast<int>(gfx_cs.GetMatrixID()),
      static_cast<int>(gfx_cs.GetRangeID()), config.hdr_metadata().has_value());

  using Self = MojoVideoDecoderService;
  decoder_->Initialize(
      config, low_delay, cdm_context,
      base::BindOnce(&Self::OnDecoderInitialized, weak_this_),
      base::BindRepeating(&Self::OnDecoderOutput, weak_this_),
      base::BindRepeating(&Self::OnDecoderWaiting, weak_this_));
}

void MojoVideoDecoderService::Decode(mojom::DecoderBufferPtr buffer,
                                     DecodeCallback callback) {
  DVLOG(3) << __func__ << " pts=" << buffer->timestamp.InMilliseconds();
  DCHECK(callback);

  std::unique_ptr<ScopedDecodeTrace> trace_event;
  if (MediaTraceIsEnabled()) {
    // Because multiple Decode() calls may be in flight, each call needs a
    // unique trace event class to identify it. This scoped event is bound
    // into the OnDecodeDone callback to ensure the trace is always closed.
    trace_event = std::make_unique<ScopedDecodeTrace>(
        kDecodeTraceName, buffer->is_key_frame, buffer->timestamp);
  }

  if (!decoder_) {
    OnDecoderDecoded(std::move(callback), std::move(trace_event),
                     DecoderStatus::Codes::kNotInitialized);
    return;
  }

  if (!is_active_instance_) {
    is_active_instance_ = true;
    g_num_active_mvd_instances++;
    base::UmaHistogramExactLinear("Media.MojoVideoDecoder.ActiveInstances",
                                  g_num_active_mvd_instances, 64);
    base::debug::SetCrashKeyString(
        GetNumVideoDecodersCrashKeyString(),
        base::NumberToString(g_num_active_mvd_instances));

    // This will be overwritten as subsequent decoders are created.
    static auto* last_codec_crash_key = base::debug::AllocateCrashKeyString(
        "last-video-decoder", base::debug::CrashKeySize::Size256);
    base::debug::SetCrashKeyString(last_codec_crash_key, codec_string_);
  }

  mojo_decoder_buffer_reader_->ReadDecoderBuffer(
      std::move(buffer),
      base::BindOnce(&MojoVideoDecoderService::OnReaderRead, weak_this_,
                     mojo::GetBadMessageCallback(), std::move(callback),
                     std::move(trace_event)));
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
  mojo_decoder_buffer_reader_->Flush(
      base::BindOnce(&MojoVideoDecoderService::OnReaderFlushed, weak_this_));
}

void MojoVideoDecoderService::OnDecoderInitialized(DecoderStatus status) {
  DVLOG(1) << __func__;
  DCHECK(!status.is_ok() || decoder_);
  DCHECK(init_cb_);
  TRACE_EVENT_ASYNC_END1("media", kInitializeTraceName, this, "success",
                         status.code());

  if (!status.is_ok()) {
    std::move(init_cb_).Run(
        status, false, 1,
        decoder_ ? decoder_->GetDecoderType() : VideoDecoderType::kUnknown);
    return;
  }
  std::move(init_cb_).Run(status, decoder_->NeedsBitstreamConversion(),
                          decoder_->GetMaxDecodeRequests(),
                          decoder_->GetDecoderType());
}

void MojoVideoDecoderService::OnReaderRead(
    mojo::ReportBadMessageCallback bad_message_callback,
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
                     DecoderStatus::Codes::kFailedToGetDecoderBuffer);
    return;
  }

  if (buffer->end_of_stream() && buffer->next_config() &&
      !absl::holds_alternative<VideoDecoderConfig>(*buffer->next_config())) {
    std::move(bad_message_callback)
        .Run("Invalid DecoderBuffer::next_config() for video.");
    return;
  }

  decoder_->Decode(
      std::move(buffer),
      base::BindOnce(&MojoVideoDecoderService::OnDecoderDecoded, weak_this_,
                     std::move(callback), std::move(trace_event)));
}

void MojoVideoDecoderService::OnReaderFlushed() {
  decoder_->Reset(
      base::BindOnce(&MojoVideoDecoderService::OnDecoderReset, weak_this_));
}

void MojoVideoDecoderService::OnDecoderDecoded(
    DecodeCallback callback,
    std::unique_ptr<ScopedDecodeTrace> trace_event,
    media::DecoderStatus status) {
  DVLOG(3) << __func__;
  if (trace_event) {
    TRACE_EVENT_ASYNC_STEP_PAST0("media", kDecodeTraceName, trace_event.get(),
                                 "Decode");
    trace_event->EndTrace(status);
  }

  std::move(callback).Run(std::move(status));
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
               "video_frame", frame->AsHumanReadableString());

  // All MojoVideoDecoder-based decoders are hardware decoders. If you're the
  // first to implement an out-of-process decoder that is not power efficient,
  // you can remove this DCHECK.
  DCHECK(frame->metadata().power_efficient);

  std::optional<base::UnguessableToken> release_token;
  if ((decoder_->FramesHoldExternalResources() ||
       frame->HasReleaseMailboxCB()) &&
      video_frame_handle_releaser_) {
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
    ProvideOverlayInfoCB provide_overlay_info_cb) {
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
