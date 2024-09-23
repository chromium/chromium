// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_STABLE_VIDEO_DECODER_SERVICE_H_
#define MEDIA_MOJO_SERVICES_STABLE_VIDEO_DECODER_SERVICE_H_

#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/unguessable_token.h"
#include "build/chromeos_buildflags.h"
#include "media/mojo/mojom/media_log.mojom.h"
#include "media/mojo/mojom/stable/stable_video_decoder.mojom.h"
#include "media/mojo/mojom/video_decoder.mojom.h"
#include "media/mojo/services/media_mojo_export.h"
#include "media/mojo/services/mojo_cdm_service_context.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/components/cdm_factory_daemon/remote_cdm_context.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace media {

class MailboxFrameRegistry;
// A StableVideoDecoderService serves as an adapter between the
// stable::mojom::StableVideoDecoder interface and the mojom::VideoDecoder
// interface. This allows us to provide hardware video decoding capabilities to
// clients that may be using a different version of the
// stable::mojom::StableVideoDecoder interface, e.g., LaCrOS. A
// StableVideoDecoderService is intended to live in a video decoder process.
// This process can host multiple StableVideoDecoderServices, but the assumption
// is that they don't distrust each other. For example, they should all be
// serving the same renderer process.
//
// TODO(b/195769334): a StableVideoDecoderService should probably be responsible
// for checking incoming data to address issues that may arise due to the stable
// nature of the stable::mojom::StableVideoDecoder interface. For example,
// suppose the StableVideoDecoderService implements an older version of the
// interface relative to the one used by the client. If the client Initialize()s
// the StableVideoDecoderService with a VideoCodecProfile that's unsupported by
// the older version of the interface, the StableVideoDecoderService should
// reject that initialization. Conversely, the client of the
// StableVideoDecoderService should also check incoming data due to similar
// concerns.
class MEDIA_MOJO_EXPORT StableVideoDecoderService
    : public stable::mojom::StableVideoDecoder,
      public stable::mojom::VideoFrameHandleReleaser,
      public mojom::VideoDecoderClient,
      public mojom::MediaLog {
 public:
  StableVideoDecoderService(
      mojo::PendingRemote<stable::mojom::StableVideoDecoderTracker>
          tracker_remote,
      std::unique_ptr<mojom::VideoDecoder> dst_video_decoder,
      MojoCdmServiceContext* cdm_service_context,
      scoped_refptr<const MailboxFrameRegistry> mailbox_frame_registry);
  StableVideoDecoderService(const StableVideoDecoderService&) = delete;
  StableVideoDecoderService& operator=(const StableVideoDecoderService&) =
      delete;
  ~StableVideoDecoderService() override;

  // stable::mojom::StableVideoDecoder implementation.
  void GetSupportedConfigs(GetSupportedConfigsCallback callback) final;
  void Construct(
      mojo::PendingAssociatedRemote<stable::mojom::VideoDecoderClient>
          stable_video_decoder_client_remote,
      mojo::PendingRemote<stable::mojom::MediaLog> stable_media_log_remote,
      mojo::PendingReceiver<stable::mojom::VideoFrameHandleReleaser>
          stable_video_frame_handle_releaser_receiver,
      mojo::ScopedDataPipeConsumerHandle decoder_buffer_pipe,
      const gfx::ColorSpace& target_color_space) final;
  void Initialize(
      const VideoDecoderConfig& config,
      bool low_delay,
      mojo::PendingRemote<stable::mojom::StableCdmContext> cdm_context,
      InitializeCallback callback) final;
  void Decode(const scoped_refptr<DecoderBuffer>& buffer,
              DecodeCallback callback) final;
  void Reset(ResetCallback callback) final;

  // mojom::stable::VideoFrameHandleReleaser implementation.
  void ReleaseVideoFrame(const base::UnguessableToken& release_token) final;

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
                        bool needs_transcryption,
                        const DecoderStatus& status,
                        bool needs_bitstream_conversion,
                        int32_t max_decode_requests,
                        VideoDecoderType decoder_type);

  mojo::Remote<stable::mojom::StableVideoDecoderTracker> tracker_remote_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Incoming calls from the |dst_video_decoder_| to
  // |video_decoder_client_receiver_| are forwarded to
  // |stable_video_decoder_client_remote_|.
  mojo::AssociatedReceiver<mojom::VideoDecoderClient>
      video_decoder_client_receiver_ GUARDED_BY_CONTEXT(sequence_checker_);
  mojo::AssociatedRemote<stable::mojom::VideoDecoderClient>
      stable_video_decoder_client_remote_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Incoming calls from the |dst_video_decoder_| to |media_log_receiver_| are
  // forwarded to |stable_media_log_remote_|.
  mojo::Receiver<mojom::MediaLog> media_log_receiver_
      GUARDED_BY_CONTEXT(sequence_checker_);
  mojo::Remote<stable::mojom::MediaLog> stable_media_log_remote_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Incoming requests from the client to
  // |stable_video_frame_handle_releaser_receiver_| are forwarded to
  // |video_frame_handle_releaser_remote_|.
  mojo::Receiver<stable::mojom::VideoFrameHandleReleaser>
      stable_video_frame_handle_releaser_receiver_
          GUARDED_BY_CONTEXT(sequence_checker_);
  mojo::Remote<mojom::VideoFrameHandleReleaser>
      video_frame_handle_releaser_remote_ GUARDED_BY_CONTEXT(sequence_checker_);

  // The incoming stable::mojom::StableVideoDecoder requests are forwarded to
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

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Used for registering the |remote_cdm_context_| so that it can be resolved
  // from the |cdm_id_| later.
  const raw_ptr<MojoCdmServiceContext> cdm_service_context_
      GUARDED_BY_CONTEXT(sequence_checker_);
  scoped_refptr<chromeos::RemoteCdmContext> remote_cdm_context_
      GUARDED_BY_CONTEXT(sequence_checker_);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // Used by OnVideoFrameDecoded() to convert media VideoFrames to a
  // stable::mojo::VideoFrame.
  const scoped_refptr<const MailboxFrameRegistry> mailbox_frame_registry_;

  std::optional<base::UnguessableToken> cdm_id_
      GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_STABLE_VIDEO_DECODER_SERVICE_H_
