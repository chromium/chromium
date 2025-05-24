// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_OOP_VIDEO_DECODER_SERVICE_H_
#define MEDIA_MOJO_SERVICES_OOP_VIDEO_DECODER_SERVICE_H_

#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "media/mojo/mojom/interface_factory.mojom.h"
#include "media/mojo/mojom/media_log.mojom.h"
#include "media/mojo/mojom/video_decoder.mojom.h"
#include "media/mojo/services/media_mojo_export.h"
#include "media/mojo/services/mojo_cdm_service_context.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/components/cdm_factory_daemon/remote_cdm_context.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace media {

// An OOPVideoDecoderService is a "frontend" for a media::mojom::VideoDecoder
// that lives in a utility process. This utility process can host multiple
// OOPVideoDecoderServices, but the assumption is that they don't distrust each
// other. For example, they should all be serving the same renderer process.
//
// A previous version of this class used to serve as an adapter between the
// stable version of media::mojom::VideoDecoder (the now removed
// media::stable::mojom::StableVideoDecoder) and media::mojom::VideoDecoder.
// Since that stable version is no longer needed, the role of
// OOPVideoDecoderService is much more narrow: it needs to transform
// InitializeWithCdmContext() calls into Initialize() calls -- the client of
// OOPVideoDecoderService in the GPU process can't use Initialize() directly
// because the |cdm_id| in that call can't be used to find the corresponding
// CdmContext outside of the GPU process.
//
// TODO(crbug.com/347331029): consider handling the InitializeWithCdmContext()
// call directly in the MojoVideoDecoderService. If we can do that, we can
// probably remove the OOPVideoDecoderService class (thus also removing the
// in-process Mojo hop that we currently have just to abide by the requirements
// of associated interfaces, see the documentation of |dst_video_decoder_| in
// the class declaration).
class MEDIA_MOJO_EXPORT OOPVideoDecoderService
    : public mojom::VideoDecoder,
      public mojom::VideoFrameHandleReleaser,
      public mojom::VideoDecoderClient,
      public mojom::MediaLog {
 public:
  OOPVideoDecoderService(
      mojo::PendingRemote<mojom::VideoDecoderTracker> tracker_remote,
      std::unique_ptr<mojom::VideoDecoder> dst_video_decoder,
      MojoCdmServiceContext* cdm_service_context);
  OOPVideoDecoderService(const OOPVideoDecoderService&) = delete;
  OOPVideoDecoderService& operator=(const OOPVideoDecoderService&) = delete;
  ~OOPVideoDecoderService() override;

  // mojom::VideoDecoder implementation.
  void GetSupportedConfigs(GetSupportedConfigsCallback callback) final;
  void Construct(mojo::PendingAssociatedRemote<mojom::VideoDecoderClient>
                     video_decoder_client_remote,
                 mojo::PendingRemote<mojom::MediaLog> media_log_remote,
                 mojo::PendingReceiver<mojom::VideoFrameHandleReleaser>
                     video_frame_handle_releaser_receiver,
                 mojo::ScopedDataPipeConsumerHandle decoder_buffer_pipe,
                 mojom::CommandBufferIdPtr command_buffer_id,
                 const gfx::ColorSpace& target_color_space) final;
  void Initialize(const VideoDecoderConfig& config,
                  bool low_delay,
                  mojom::CdmPtr cdm,
                  InitializeCallback callback) final;
  void Decode(mojom::DecoderBufferPtr buffer, DecodeCallback callback) final;
  void Reset(ResetCallback callback) final;
  void OnOverlayInfoChanged(const OverlayInfo& overlay_info) final;

  // mojom::VideoFrameHandleReleaser implementation.
  void ReleaseVideoFrame(
      const base::UnguessableToken& release_token,
      const std::optional<gpu::SyncToken>& release_sync_token) final;

  // mojom::VideoDecoderClient implementation.
  void OnVideoFrameDecoded(
      const scoped_refptr<VideoFrame>& frame,
      bool can_read_without_stalling,
      const std::optional<base::UnguessableToken>& release_token) final;
  void OnWaiting(WaitingReason reason) final;
  void RequestOverlayInfo(bool restart_for_transitions) final;

  // mojom::MediaLog implementation.
  void AddLogRecord(const MediaLogRecord& event) final;

 private:
  void OnInitializeDone(InitializeCallback init_cb,
                        const DecoderStatus& status,
                        bool needs_bitstream_conversion,
                        int32_t max_decode_requests,
                        VideoDecoderType decoder_type,
                        bool needs_transcryption);

  mojo::Remote<mojom::VideoDecoderTracker> tracker_remote_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Incoming calls from the |dst_video_decoder_| to
  // |video_decoder_client_receiver_| are forwarded to
  // |video_decoder_client_remote_|.
  mojo::AssociatedReceiver<mojom::VideoDecoderClient>
      video_decoder_client_receiver_ GUARDED_BY_CONTEXT(sequence_checker_);
  mojo::AssociatedRemote<mojom::VideoDecoderClient> video_decoder_client_remote_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Incoming calls from the |dst_video_decoder_| to |media_log_receiver_| are
  // forwarded to |media_log_remote_|.
  mojo::Receiver<mojom::MediaLog> media_log_receiver_
      GUARDED_BY_CONTEXT(sequence_checker_);
  mojo::Remote<mojom::MediaLog> media_log_remote_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Incoming requests from the client to
  // |video_frame_handle_releaser_receiver_| are forwarded to
  // |video_frame_handle_releaser_remote_|.
  mojo::Receiver<mojom::VideoFrameHandleReleaser>
      video_frame_handle_releaser_receiver_
          GUARDED_BY_CONTEXT(sequence_checker_);
  mojo::Remote<mojom::VideoFrameHandleReleaser>
      video_frame_handle_releaser_remote_ GUARDED_BY_CONTEXT(sequence_checker_);

  // The incoming mojom::VideoDecoder requests are forwarded to
  // |dst_video_decoder_receiver_| through |dst_video_decoder_remote_|.
  //
  // Note: the implementation behind |dst_video_decoder_receiver_| (i.e.,
  // |dst_video_decoder_|) lives in-process. The reason we don't just make calls
  // directly to that implementation is that when we call Construct(), we need
  // to pass a mojo::PendingAssociatedRemote which needs to be sent over an
  // existing pipe before using it to make calls.
  std::unique_ptr<mojom::VideoDecoder> dst_video_decoder_
      GUARDED_BY_CONTEXT(sequence_checker_);
  mojo::Receiver<mojom::VideoDecoder> dst_video_decoder_receiver_
      GUARDED_BY_CONTEXT(sequence_checker_);
  mojo::Remote<mojom::VideoDecoder> dst_video_decoder_remote_
      GUARDED_BY_CONTEXT(sequence_checker_);

#if BUILDFLAG(IS_CHROMEOS)
  // Used for registering the |remote_cdm_context_| so that it can be resolved
  // from the |cdm_id_| later.
  const raw_ptr<MojoCdmServiceContext> cdm_service_context_
      GUARDED_BY_CONTEXT(sequence_checker_);
  scoped_refptr<chromeos::RemoteCdmContext> remote_cdm_context_
      GUARDED_BY_CONTEXT(sequence_checker_);
#endif  // BUILDFLAG(IS_CHROMEOS)

  std::optional<base::UnguessableToken> cdm_id_
      GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_OOP_VIDEO_DECODER_SERVICE_H_
