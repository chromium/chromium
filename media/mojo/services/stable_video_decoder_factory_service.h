// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_STABLE_VIDEO_DECODER_FACTORY_SERVICE_H_
#define MEDIA_MOJO_SERVICES_STABLE_VIDEO_DECODER_FACTORY_SERVICE_H_

#include "base/functional/callback.h"
#include "base/sequence_checker.h"
#include "media/mojo/mojom/stable/stable_video_decoder.mojom.h"
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
class MailboxFrameRegistry;

// A StableVideoDecoderFactoryService allows a browser process to create
// StableVideoDecoders. It's intended to live inside a video decoder process (a
// utility process) and there should only be one such instance per process
// because one video decoder process corresponds to a client that handles one
// origin. For example, all the StableVideoDecoders for a video conference call
// can live in the same process (and thus be created by the same
// StableVideoDecoderFactoryService). However, the StableVideoDecoder for a
// YouTube video should live in a process separate than a StableVideoDecoder for
// a Vimeo video.
class MEDIA_MOJO_EXPORT StableVideoDecoderFactoryService
    : public stable::mojom::StableVideoDecoderFactory {
 public:
  explicit StableVideoDecoderFactoryService(
      const gpu::GpuFeatureInfo& gpu_feature_info);
  StableVideoDecoderFactoryService(const StableVideoDecoderFactoryService&) =
      delete;
  StableVideoDecoderFactoryService& operator=(
      const StableVideoDecoderFactoryService&) = delete;
  ~StableVideoDecoderFactoryService() override;

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

  void BindReceiver(
      mojo::PendingReceiver<stable::mojom::StableVideoDecoderFactory> receiver,
      base::OnceClosure disconnect_cb);

  // stable::mojom::StableVideoDecoderFactory implementation.
  void CreateStableVideoDecoder(
      mojo::PendingReceiver<stable::mojom::StableVideoDecoder> receiver,
      mojo::PendingRemote<stable::mojom::StableVideoDecoderTracker> tracker)
      override;

 private:
  VideoDecoderCreationCBForTesting video_decoder_creation_cb_for_testing_
      GUARDED_BY_CONTEXT(sequence_checker_);

  mojo::Receiver<stable::mojom::StableVideoDecoderFactory> receiver_;

  // Shared between the MojoMediaClientImpl and the StableVideoDecoderService.
  scoped_refptr<MailboxFrameRegistry> mailbox_frame_registry_;

  // |mojo_media_client_| and |cdm_service_context_| must be declared before
  // |video_decoders_| because the interface implementation instances managed by
  // that set take raw pointers to them.
  std::unique_ptr<MojoMediaClient> mojo_media_client_
      GUARDED_BY_CONTEXT(sequence_checker_);
  MojoCdmServiceContext cdm_service_context_
      GUARDED_BY_CONTEXT(sequence_checker_);
  mojo::UniqueReceiverSet<stable::mojom::StableVideoDecoder> video_decoders_
      GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_STABLE_VIDEO_DECODER_FACTORY_SERVICE_H_
