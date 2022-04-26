// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_STABLE_VIDEO_DECODER_SERVICE_H_
#define MEDIA_MOJO_SERVICES_STABLE_VIDEO_DECODER_SERVICE_H_

#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "media/mojo/mojom/stable/stable_video_decoder.mojom.h"
#include "media/mojo/mojom/video_decoder.mojom.h"
#include "media/mojo/services/media_mojo_export.h"

namespace media {

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
    : public stable::mojom::StableVideoDecoder {
 public:
  explicit StableVideoDecoderService(
      std::unique_ptr<mojom::VideoDecoder> dst_video_decoder);
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

 private:
  // The incoming stable::mojom::StableVideoDecoder requests are forwarded to
  // |dst_video_decoder_|.
  std::unique_ptr<mojom::VideoDecoder> dst_video_decoder_
      GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_STABLE_VIDEO_DECODER_SERVICE_H_
