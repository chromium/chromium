// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_OOP_VIDEO_DECODER_FACTORY_SERVICE_H_
#define MEDIA_MOJO_SERVICES_OOP_VIDEO_DECODER_FACTORY_SERVICE_H_

#include "base/functional/callback.h"
#include "base/sequence_checker.h"
#include "gpu/config/gpu_feature_info.h"
#include "media/mojo/mojom/interface_factory.mojom.h"
#include "media/mojo/mojom/video_decoder.mojom.h"
#include "media/mojo/services/media_mojo_export.h"
#include "media/mojo/services/mojo_cdm_service_context.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"

namespace media {
namespace mojom {
class VideoDecoder;
}  // namespace mojom

class MojoMediaClient;

// An OOPVideoDecoderFactoryService allows a browser process to create
// VideoDecoders. It's intended to live inside a video decoder process (a
// utility process) and there should only be one such instance per process
// because one video decoder process corresponds to a client that handles one
// origin. For example, all the VideoDecoders for a video conference call
// can live in the same process (and thus be created by the same
// OOPVideoDecoderFactoryService). However, the VideoDecoder for a
// YouTube video should live in a process separate than a VideoDecoder for
// a Vimeo video.
class MEDIA_MOJO_EXPORT OOPVideoDecoderFactoryService
    : public mojom::InterfaceFactory {
 public:
  explicit OOPVideoDecoderFactoryService(
      const gpu::GpuFeatureInfo& gpu_feature_info);
  OOPVideoDecoderFactoryService(const OOPVideoDecoderFactoryService&) = delete;
  OOPVideoDecoderFactoryService& operator=(
      const OOPVideoDecoderFactoryService&) = delete;
  ~OOPVideoDecoderFactoryService() override;

  using VideoDecoderCreationCBForTesting =
      base::RepeatingCallback<std::unique_ptr<mojom::VideoDecoder>(
          MojoMediaClient*,
          MojoCdmServiceContext*)>;
  void SetVideoDecoderCreationCallbackForTesting(
      VideoDecoderCreationCBForTesting video_decoder_creation_cb_for_testing) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    video_decoder_creation_cb_for_testing_ =
        video_decoder_creation_cb_for_testing;
  }

  void BindReceiver(mojo::PendingReceiver<mojom::InterfaceFactory> receiver,
                    base::OnceClosure disconnect_cb);

  // mojom::InterfaceFactory implementation.
#if BUILDFLAG(ALLOW_OOP_VIDEO_DECODER)
  void CreateVideoDecoderWithTracker(
      mojo::PendingReceiver<mojom::VideoDecoder> receiver,
      mojo::PendingRemote<mojom::VideoDecoderTracker> tracker) override;
#endif
  void CreateAudioDecoder(
      mojo::PendingReceiver<mojom::AudioDecoder> receiver) override;
  void CreateVideoDecoder(
      mojo::PendingReceiver<mojom::VideoDecoder> receiver,
      mojo::PendingRemote<mojom::VideoDecoder> dst_video_decoder) override;
  void CreateAudioEncoder(
      mojo::PendingReceiver<mojom::AudioEncoder> receiver) override;
  void CreateDefaultRenderer(
      const std::string& audio_device_id,
      mojo::PendingReceiver<mojom::Renderer> receiver) override;
  void CreateCdm(const CdmConfig& cdm_config,
                 CreateCdmCallback callback) override;

 private:
  VideoDecoderCreationCBForTesting video_decoder_creation_cb_for_testing_
      GUARDED_BY_CONTEXT(sequence_checker_);

  mojo::Receiver<mojom::InterfaceFactory> receiver_;

  // |mojo_media_client_| and |cdm_service_context_| must be declared before
  // |video_decoders_| because the interface implementation instances managed by
  // that set take raw pointers to them.
  std::unique_ptr<MojoMediaClient> mojo_media_client_
      GUARDED_BY_CONTEXT(sequence_checker_);
  MojoCdmServiceContext cdm_service_context_
      GUARDED_BY_CONTEXT(sequence_checker_);
  mojo::UniqueReceiverSet<mojom::VideoDecoder> video_decoders_
      GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_OOP_VIDEO_DECODER_FACTORY_SERVICE_H_
