// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/stable_video_decoder_service.h"

#include "base/notreached.h"
#include "media/gpu/chromeos/mailbox_frame_registry.h"
#include "media/mojo/common/media_type_converters.h"

#if BUILDFLAG(IS_CHROMEOS_ASH) && BUILDFLAG(USE_VAAPI)
#include "media/gpu/vaapi/vaapi_wrapper.h"
#endif

namespace media {

namespace {

// GetGpuMemoryBufferHandle() is a helper function that gets or creates a
// GpuMemoryBufferHandle from |media_frame|. For decoders that use VDA, the
// storage type is STORAGE_GPU_MEMORY_BUFFER. For decoders that use VD directly,
// the storage type is STORAGE_OPAQUE.
gfx::GpuMemoryBufferHandle GetGpuMemoryBufferHandle(
    scoped_refptr<VideoFrame> media_frame,
    scoped_refptr<const MailboxFrameRegistry> mailbox_frame_registry) {
  switch (media_frame->storage_type()) {
    case VideoFrame::STORAGE_GPU_MEMORY_BUFFER:
      CHECK(media_frame->HasMappableGpuBuffer());
      return media_frame->GetGpuMemoryBufferHandle();
    case VideoFrame::STORAGE_OPAQUE: {
      CHECK(mailbox_frame_registry);
      CHECK(media_frame->HasOOPVDMailbox());
      auto frame_resource =
          mailbox_frame_registry->AccessFrame(media_frame->oopvd_mailbox());
      CHECK(frame_resource);
      return frame_resource->CreateGpuMemoryBufferHandle();
    }
    default:
      NOTREACHED();
  }
}

stable::mojom::VideoFramePtr MediaVideoFrameToMojoVideoFrame(
    scoped_refptr<VideoFrame> media_frame,
    scoped_refptr<const MailboxFrameRegistry> mailbox_frame_registry) {
  CHECK(!media_frame->metadata().end_of_stream);

  stable::mojom::VideoFramePtr mojo_frame = stable::mojom::VideoFrame::New();
  CHECK(mojo_frame);

  static_assert(
      std::is_same<decltype(media_frame->format()),
                   decltype(stable::mojom::VideoFrame::format)>::value,
      "Unexpected type for media::VideoFrame::format(). If you "
      "need to change this assertion, please contact "
      "chromeos-gfx-video@google.com.");
  mojo_frame->format = media_frame->format();

  static_assert(
      std::is_same<decltype(media_frame->coded_size()),
                   std::add_lvalue_reference<std::add_const<
                       decltype(stable::mojom::VideoFrame::coded_size)>::type>::
                       type>::value,
      "Unexpected type for media::VideoFrame::coded_size(). If you "
      "need to change this assertion, please contact "
      "chromeos-gfx-video@google.com.");
  mojo_frame->coded_size = media_frame->coded_size();

  static_assert(
      std::is_same<
          decltype(media_frame->visible_rect()),
          std::add_lvalue_reference<std::add_const<
              decltype(stable::mojom::VideoFrame::visible_rect)>::type>::type>::
          value,
      "Unexpected type for media::VideoFrame::visible_rect(). If you "
      "need to change this assertion, please contact "
      "chromeos-gfx-video@google.com.");
  mojo_frame->visible_rect = media_frame->visible_rect();

  static_assert(
      std::is_same<
          decltype(media_frame->natural_size()),
          std::add_lvalue_reference<std::add_const<
              decltype(stable::mojom::VideoFrame::natural_size)>::type>::type>::
          value,
      "Unexpected type for media::VideoFrame::natural_size(). If you "
      "need to change this assertion, please contact "
      "chromeos-gfx-video@google.com.");
  mojo_frame->natural_size = media_frame->natural_size();

  static_assert(
      std::is_same<decltype(media_frame->timestamp()),
                   decltype(stable::mojom::VideoFrame::timestamp)>::value,
      "Unexpected type for media::VideoFrame::timestamp(). If you "
      "need to change this assertion, please contact "
      "chromeos-gfx-video@google.com.");
  mojo_frame->timestamp = media_frame->timestamp();

  gfx::GpuMemoryBufferHandle gpu_memory_buffer_handle =
      GetGpuMemoryBufferHandle(media_frame, mailbox_frame_registry);
  CHECK_EQ(gpu_memory_buffer_handle.type, gfx::NATIVE_PIXMAP);
  CHECK(!gpu_memory_buffer_handle.native_pixmap_handle.planes.empty());
  mojo_frame->gpu_memory_buffer_handle = std::move(gpu_memory_buffer_handle);

  static_assert(
      std::is_same<
          decltype(media_frame->metadata()),
          std::add_lvalue_reference<
              decltype(stable::mojom::VideoFrame::metadata)>::type>::value,
      "Unexpected type for media::VideoFrame::metadata(). If you "
      "need to change this assertion, please contact "
      "chromeos-gfx-video@google.com.");
  mojo_frame->metadata = media_frame->metadata();

  static_assert(
      std::is_same<decltype(media_frame->ColorSpace()),
                   decltype(stable::mojom::VideoFrame::color_space)>::value,
      "Unexpected type for media::VideoFrame::ColorSpace(). If you "
      "need to change this assertion, please contact "
      "chromeos-gfx-video@google.com.");
  mojo_frame->color_space = media_frame->ColorSpace();

  static_assert(
      std::is_same<
          decltype(media_frame->hdr_metadata()),
          std::add_lvalue_reference<std::add_const<
              decltype(stable::mojom::VideoFrame::hdr_metadata)>::type>::type>::
          value,
      "Unexpected type for media::VideoFrame::hdr_metadata(). If you "
      "need to change this assertion, please contact "
      "chromeos-gfx-video@google.com.");
  mojo_frame->hdr_metadata = media_frame->hdr_metadata();

  return mojo_frame;
}

}  // namespace

StableVideoDecoderService::StableVideoDecoderService(
    mojo::PendingRemote<stable::mojom::StableVideoDecoderTracker>
        tracker_remote,
    std::unique_ptr<mojom::VideoDecoder> dst_video_decoder,
    MojoCdmServiceContext* cdm_service_context,
    scoped_refptr<const MailboxFrameRegistry> mailbox_frame_registry)
    : tracker_remote_(std::move(tracker_remote)),
      video_decoder_client_receiver_(this),
      media_log_receiver_(this),
      stable_video_frame_handle_releaser_receiver_(this),
      dst_video_decoder_(std::move(dst_video_decoder)),
      dst_video_decoder_receiver_(dst_video_decoder_.get())
#if BUILDFLAG(IS_CHROMEOS_ASH)
      ,
      cdm_service_context_(cdm_service_context)
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
      ,
      mailbox_frame_registry_(mailbox_frame_registry) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!!dst_video_decoder_);
  dst_video_decoder_remote_.Bind(
      dst_video_decoder_receiver_.BindNewPipeAndPassRemote());
}

StableVideoDecoderService::~StableVideoDecoderService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (cdm_id_)
    cdm_service_context_->UnregisterRemoteCdmContext(cdm_id_.value());
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

void StableVideoDecoderService::GetSupportedConfigs(
    GetSupportedConfigsCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  dst_video_decoder_remote_->GetSupportedConfigs(std::move(callback));
}

void StableVideoDecoderService::Construct(
    mojo::PendingAssociatedRemote<stable::mojom::VideoDecoderClient>
        stable_video_decoder_client_remote,
    mojo::PendingRemote<stable::mojom::MediaLog> stable_media_log_remote,
    mojo::PendingReceiver<stable::mojom::VideoFrameHandleReleaser>
        stable_video_frame_handle_releaser_receiver,
    mojo::ScopedDataPipeConsumerHandle decoder_buffer_pipe,
    const gfx::ColorSpace& target_color_space) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (video_decoder_client_receiver_.is_bound()) {
    mojo::ReportBadMessage("Construct() already called");
    return;
  }

  DCHECK(!video_decoder_client_receiver_.is_bound());
  DCHECK(!stable_video_decoder_client_remote_.is_bound());
  stable_video_decoder_client_remote_.Bind(
      std::move(stable_video_decoder_client_remote));

  DCHECK(!media_log_receiver_.is_bound());
  DCHECK(!stable_media_log_remote_.is_bound());
  stable_media_log_remote_.Bind(std::move(stable_media_log_remote));

  DCHECK(!video_frame_handle_releaser_remote_.is_bound());
  DCHECK(!stable_video_frame_handle_releaser_receiver_.is_bound());
  stable_video_frame_handle_releaser_receiver_.Bind(
      std::move(stable_video_frame_handle_releaser_receiver));

  dst_video_decoder_remote_->Construct(
      video_decoder_client_receiver_.BindNewEndpointAndPassRemote(),
      media_log_receiver_.BindNewPipeAndPassRemote(),
      video_frame_handle_releaser_remote_.BindNewPipeAndPassReceiver(),
      std::move(decoder_buffer_pipe), mojom::CommandBufferIdPtr(),
      target_color_space);
}

void StableVideoDecoderService::Initialize(
    const VideoDecoderConfig& config,
    bool low_delay,
    mojo::PendingRemote<stable::mojom::StableCdmContext> cdm_context,
    InitializeCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!video_decoder_client_receiver_.is_bound()) {
    DVLOG(2) << __func__ << " Construct() must be called first";
    std::move(callback).Run(DecoderStatus::Codes::kFailedToCreateDecoder,
                            /*needs_bitstream_conversion=*/false,
                            /*max_decode_requests=*/1,
                            VideoDecoderType::kUnknown,
                            /*needs_transcryption=*/false);
    return;
  }

  bool needs_transcryption = false;

  // The |config| should have been validated at deserialization time.
  DCHECK(config.IsValidConfig());
  if (config.is_encrypted()) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    if (!cdm_id_) {
      if (!cdm_context) {
        std::move(callback).Run(DecoderStatus::Codes::kMissingCDM,
                                /*needs_bitstream_conversion=*/false,
                                /*max_decode_requests=*/1,
                                VideoDecoderType::kUnknown,
                                /*needs_transcryption=*/false);
        return;
      }
      remote_cdm_context_ = base::WrapRefCounted(
          new chromeos::RemoteCdmContext(std::move(cdm_context)));
      cdm_id_ = cdm_service_context_->RegisterRemoteCdmContext(
          remote_cdm_context_.get());
    }
#if BUILDFLAG(USE_VAAPI)
    needs_transcryption = (VaapiWrapper::GetImplementationType() ==
                           VAImplementation::kMesaGallium);
#endif
#else
    std::move(callback).Run(DecoderStatus::Codes::kUnsupportedConfig,
                            /*needs_bitstream_conversion=*/false,
                            /*max_decode_requests=*/1,
                            VideoDecoderType::kUnknown,
                            /*needs_transcryption=*/false);
    return;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  }

  // Even though this is in-process, we still need to pass a |cdm_id_|
  // instead of a media::CdmContext* since this goes through Mojo IPC. This is
  // why we need to register with the |cdm_service_context_| above.
  //
  // Note: base::Unretained() is safe because *|this| fully owns
  // |dst_video_decoder_remote_|, so the response callback will never run beyond
  // the lifetime of *|this|.
  dst_video_decoder_remote_->Initialize(
      config, low_delay, cdm_id_,
      base::BindOnce(&StableVideoDecoderService::OnInitializeDone,
                     base::Unretained(this), std::move(callback),
                     needs_transcryption));
}

void StableVideoDecoderService::OnInitializeDone(
    InitializeCallback init_cb,
    bool needs_transcryption,
    const DecoderStatus& status,
    bool needs_bitstream_conversion,
    int32_t max_decode_requests,
    VideoDecoderType decoder_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::move(init_cb).Run(status, needs_bitstream_conversion,
                         max_decode_requests, decoder_type,
                         needs_transcryption);
}

void StableVideoDecoderService::Decode(
    const scoped_refptr<DecoderBuffer>& buffer,
    DecodeCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!video_decoder_client_receiver_.is_bound()) {
    DVLOG(2) << __func__ << " Construct() must be called first";
    std::move(callback).Run(DecoderStatus::Codes::kFailedToCreateDecoder);
    return;
  }

  CHECK(buffer);
  mojom::DecoderBufferPtr mojo_buffer = mojom::DecoderBuffer::From(*buffer);
  CHECK(mojo_buffer);
  dst_video_decoder_remote_->Decode(std::move(mojo_buffer),
                                    std::move(callback));
}

void StableVideoDecoderService::Reset(ResetCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!video_decoder_client_receiver_.is_bound()) {
    DVLOG(2) << __func__ << " Construct() must be called first";
    std::move(callback).Run();
    return;
  }
  dst_video_decoder_remote_->Reset(std::move(callback));
}

void StableVideoDecoderService::ReleaseVideoFrame(
    const base::UnguessableToken& release_token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(video_frame_handle_releaser_remote_.is_bound());
  // Note: we don't pass a gpu::SyncToken because it's assumed that the client
  // (the GPU process) has already waited on the SyncToken that comes from the
  // ultimate client (the renderer process) before calling ReleaseVideoFrame()
  // on the out-of-process video decoder.
  video_frame_handle_releaser_remote_->ReleaseVideoFrame(
      release_token, /*release_sync_token=*/std::nullopt);
}

void StableVideoDecoderService::OnVideoFrameDecoded(
    const scoped_refptr<VideoFrame>& frame,
    bool can_read_without_stalling,
    const std::optional<base::UnguessableToken>& release_token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(stable_video_decoder_client_remote_.is_bound());
  DCHECK(release_token.has_value());

  // The mojo traits have been coded assuming these conditions.
  CHECK(frame->metadata().allow_overlay);
  CHECK(!frame->metadata().end_of_stream);
  CHECK(frame->metadata().power_efficient);

  stable_video_decoder_client_remote_->OnVideoFrameDecoded(
      MediaVideoFrameToMojoVideoFrame(std::move(frame),
                                      mailbox_frame_registry_),
      can_read_without_stalling, *release_token);
}

void StableVideoDecoderService::OnWaiting(WaitingReason reason) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(stable_video_decoder_client_remote_.is_bound());
  stable_video_decoder_client_remote_->OnWaiting(reason);
}

void StableVideoDecoderService::RequestOverlayInfo(
    bool restart_for_transitions) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTREACHED_IN_MIGRATION();
}

void StableVideoDecoderService::AddLogRecord(const MediaLogRecord& event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(stable_media_log_remote_.is_bound());
  stable_media_log_remote_->AddLogRecord(event);
}

}  // namespace media
