// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_STABLE_VIDEO_DECODER_FACTORY_PROCESS_SERVICE_H_
#define MEDIA_MOJO_SERVICES_STABLE_VIDEO_DECODER_FACTORY_PROCESS_SERVICE_H_

#include "media/mojo/mojom/stable/stable_video_decoder.mojom.h"

#include "base/sequence_checker.h"
#include "media/mojo/services/media_mojo_export.h"
#include "media/mojo/services/stable_video_decoder_factory_service.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace media {

// A StableVideoDecoderFactoryProcessService allows the browser process to
// initialize a StableVideoDecoderFactory with a gpu::GpuFeatureInfo.
class MEDIA_MOJO_EXPORT StableVideoDecoderFactoryProcessService final
    : public stable::mojom::StableVideoDecoderFactoryProcess {
 public:
  explicit StableVideoDecoderFactoryProcessService(
      mojo::PendingReceiver<stable::mojom::StableVideoDecoderFactoryProcess>
          receiver);
  StableVideoDecoderFactoryProcessService(
      const StableVideoDecoderFactoryProcessService&) = delete;
  StableVideoDecoderFactoryProcessService& operator=(
      const StableVideoDecoderFactoryProcessService&) = delete;
  ~StableVideoDecoderFactoryProcessService() final;

  // stable::mojom::StableVideoDecoderFactoryProcess implementation.
  void InitializeStableVideoDecoderFactory(
      const gpu::GpuFeatureInfo& gpu_feature_info,
      mojo::PendingReceiver<stable::mojom::StableVideoDecoderFactory> receiver)
      final;

  void OnFactoryDisconnected();

 private:
  mojo::Receiver<stable::mojom::StableVideoDecoderFactoryProcess> receiver_
      GUARDED_BY_CONTEXT(sequence_checker_);
  std::unique_ptr<StableVideoDecoderFactoryService> factory_
      GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_STABLE_VIDEO_DECODER_FACTORY_PROCESS_SERVICE_H_
