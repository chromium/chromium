// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_OOP_VIDEO_DECODER_FACTORY_PROCESS_SERVICE_H_
#define MEDIA_MOJO_SERVICES_OOP_VIDEO_DECODER_FACTORY_PROCESS_SERVICE_H_

#include "base/sequence_checker.h"
#include "media/mojo/mojom/video_decoder_factory_process.mojom.h"
#include "media/mojo/services/media_mojo_export.h"
#include "media/mojo/services/oop_video_decoder_factory_service.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace media {

// An OOPVideoDecoderFactoryProcessService allows the browser process to
// initialize an InterfaceFactory with a gpu::GpuFeatureInfo.
class MEDIA_MOJO_EXPORT OOPVideoDecoderFactoryProcessService final
    : public mojom::VideoDecoderFactoryProcess {
 public:
  explicit OOPVideoDecoderFactoryProcessService(
      mojo::PendingReceiver<mojom::VideoDecoderFactoryProcess> receiver);
  OOPVideoDecoderFactoryProcessService(
      const OOPVideoDecoderFactoryProcessService&) = delete;
  OOPVideoDecoderFactoryProcessService& operator=(
      const OOPVideoDecoderFactoryProcessService&) = delete;
  ~OOPVideoDecoderFactoryProcessService() final;

  // mojom::VideoDecoderFactoryProcess implementation.
  void InitializeVideoDecoderFactory(
      const gpu::GpuFeatureInfo& gpu_feature_info,
      mojo::PendingReceiver<mojom::InterfaceFactory> receiver) final;

  void OnFactoryDisconnected();

 private:
  mojo::Receiver<mojom::VideoDecoderFactoryProcess> receiver_
      GUARDED_BY_CONTEXT(sequence_checker_);
  std::unique_ptr<OOPVideoDecoderFactoryService> factory_
      GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_OOP_VIDEO_DECODER_FACTORY_PROCESS_SERVICE_H_
