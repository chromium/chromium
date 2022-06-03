// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_STABLE_VIDEO_DECODER_FACTORY_SERVICE_H_
#define MEDIA_MOJO_SERVICES_STABLE_VIDEO_DECODER_FACTORY_SERVICE_H_

#include "media/mojo/mojom/stable/stable_video_decoder.mojom.h"
#include "media/mojo/services/media_mojo_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace media {

class MEDIA_MOJO_EXPORT StableVideoDecoderFactoryService
    : public stable::mojom::StableVideoDecoderFactory {
 public:
  StableVideoDecoderFactoryService();
  StableVideoDecoderFactoryService(const StableVideoDecoderFactoryService&) =
      delete;
  StableVideoDecoderFactoryService& operator=(
      const StableVideoDecoderFactoryService&) = delete;
  ~StableVideoDecoderFactoryService() override;

  void BindReceiver(
      mojo::PendingReceiver<stable::mojom::StableVideoDecoderFactory> receiver);

  // stable::mojom::StableVideoDecoderFactory implementation.
  void CreateStableVideoDecoder(
      mojo::PendingReceiver<stable::mojom::StableVideoDecoder> receiver)
      override;

 private:
  mojo::ReceiverSet<stable::mojom::StableVideoDecoderFactory> receivers_;
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_STABLE_VIDEO_DECODER_FACTORY_SERVICE_H_
