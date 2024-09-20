// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_MOJO_VIDEO_DECODER_SERVICE_H_
#define MEDIA_MOJO_SERVICES_MOJO_VIDEO_DECODER_SERVICE_H_

#include <map>
#include <memory>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/unguessable_token.h"
#include "media/base/cdm_context.h"
#include "media/base/decoder_status.h"
#include "media/base/overlay_info.h"
#include "media/base/video_decoder.h"
#include "media/mojo/mojom/stable/stable_video_decoder.mojom.h"
#include "media/mojo/mojom/video_decoder.mojom.h"
#include "media/mojo/services/media_mojo_export.h"
#include "media/mojo/services/mojo_media_client.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace media {

class DecoderBuffer;
class MojoCdmServiceContext;
class MojoDecoderBufferReader;
class MojoMediaClient;
class MojoMediaLog;
class VideoFrame;

// Implementation of a mojom::VideoDecoder which runs in the GPU process,
// and wraps a media::VideoDecoder.
class MEDIA_MOJO_EXPORT MojoVideoDecoderService final
    : public mojom::VideoDecoder {
 public:
  explicit MojoVideoDecoderService(
      MojoMediaClient* mojo_media_client,
      MojoCdmServiceContext* mojo_cdm_service_context,
      mojo::PendingRemote<stable::mojom::StableVideoDecoder>
          oop_video_decoder_remote);

  MojoVideoDecoderService(const MojoVideoDecoderService&) = delete;
  MojoVideoDecoderService& operator=(const MojoVideoDecoderService&) = delete;

  ~MojoVideoDecoderService() final;

  // mojom::VideoDecoder implementation
  void GetSupportedConfigs(GetSupportedConfigsCallback callback) final;
  void Construct(
      mojo::PendingAssociatedRemote<mojom::VideoDecoderClient> client,
      mojo::PendingRemote<mojom::MediaLog> media_log,
      mojo::PendingReceiver<mojom::VideoFrameHandleReleaser>
          video_frame_handle_receiver,
      mojo::ScopedDataPipeConsumerHandle decoder_buffer_pipe,
      mojom::CommandBufferIdPtr command_buffer_id,
      const gfx::ColorSpace& target_color_space) final;
  void Initialize(const VideoDecoderConfig& config,
                  bool low_delay,
                  const std::optional<base::UnguessableToken>& cdm_id,
                  InitializeCallback callback) final;
  void Decode(mojom::DecoderBufferPtr buffer, DecodeCallback callback) final;
  void Reset(ResetCallback callback) final;
  void OnOverlayInfoChanged(const OverlayInfo& overlay_info) final;

 private:
  // Helper methods so that we can bind them with a weak pointer to avoid
  // running mojom::VideoDecoder callbacks after connection error happens and
  // |this| is deleted. It's not safe to run the callbacks after a connection
  // error.
  void OnDecoderInitialized(DecoderStatus status);
  void OnReaderRead(mojo::ReportBadMessageCallback bad_message_callback,
                    DecodeCallback callback,
                    std::unique_ptr<ScopedDecodeTrace> trace_event,
                    scoped_refptr<DecoderBuffer> buffer);
  void OnDecoderDecoded(DecodeCallback callback,
                        std::unique_ptr<ScopedDecodeTrace> trace_event,
                        DecoderStatus status);

  // Called by |mojo_decoder_buffer_reader_| when reset is finished.
  void OnReaderFlushed();

  void OnDecoderReset();
  void OnDecoderOutput(scoped_refptr<VideoFrame> frame);

  void OnDecoderWaiting(WaitingReason reason);

  void OnDecoderRequestedOverlayInfo(
      bool restart_for_transitions,
      ProvideOverlayInfoCB provide_overlay_info_cb);

  // Whether this instance is active (Decode() was called at least once).
  bool is_active_instance_ = false;

  // Codec information stored via crash key.
  std::string codec_string_;

  // Decoder factory.
  raw_ptr<MojoMediaClient> mojo_media_client_;

  // A helper object required to get the CDM from a CDM ID.
  raw_ptr<MojoCdmServiceContext> mojo_cdm_service_context_ = nullptr;

  // Channel for sending async messages to the client.
  mojo::AssociatedRemote<mojom::VideoDecoderClient> client_;

  // Proxy object for providing media log services.
  std::unique_ptr<MojoMediaLog> media_log_;

  // Holds VideoFrame references on behalf of the client, until the client
  // releases them or is disconnected.
  mojo::SelfOwnedReceiverRef<mojom::VideoFrameHandleReleaser>
      video_frame_handle_releaser_;

  // Helper for reading DecoderBuffer data from the DataPipe.
  std::unique_ptr<MojoDecoderBufferReader> mojo_decoder_buffer_reader_;

  // The CDM ID and the corresponding CdmContextRef, which must be held to keep
  // the CdmContext alive for the lifetime of the |decoder_|.
  std::optional<base::UnguessableToken> cdm_id_;
  std::unique_ptr<CdmContextRef> cdm_context_ref_;

  std::unique_ptr<media::VideoDecoder> decoder_;

  // An out-of-process video decoder to forward decode requests to. This member
  // just holds the PendingRemote in between the construction of the
  // MojoVideoDecoderService and the call to
  // |mojo_media_client_|->CreateVideoDecoder().
  mojo::PendingRemote<stable::mojom::StableVideoDecoder>
      oop_video_decoder_pending_remote_;

  InitializeCallback init_cb_;
  ResetCallback reset_cb_;

  ProvideOverlayInfoCB provide_overlay_info_cb_;

  base::WeakPtr<MojoVideoDecoderService> weak_this_;
  base::WeakPtrFactory<MojoVideoDecoderService> weak_factory_{this};
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_MOJO_VIDEO_DECODER_SERVICE_H_
