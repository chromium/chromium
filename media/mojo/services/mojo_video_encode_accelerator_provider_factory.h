// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_MOJO_VIDEO_ENCODE_ACCELERATOR_PROVIDER_FACTORY_H_
#define MEDIA_MOJO_SERVICES_MOJO_VIDEO_ENCODE_ACCELERATOR_PROVIDER_FACTORY_H_

#include "base/sequence_checker.h"
#include "media/mojo/mojom/video_encode_accelerator.mojom.h"
#include "media/mojo/services/media_mojo_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"

namespace media {

// This class implements mojom::VideoEncodeAcceleratorProviderFactory and lives
// in a utility process. This class houses a UniqueReceiverSet of
// mojom::VideoEncodeAcceleratorProviders so that each utility process can have
// multiple encoder providers.
class MEDIA_MOJO_EXPORT MojoVideoEncodeAcceleratorProviderFactory
    : public mojom::VideoEncodeAcceleratorProviderFactory {
 public:
  MojoVideoEncodeAcceleratorProviderFactory();
  MojoVideoEncodeAcceleratorProviderFactory(
      const MojoVideoEncodeAcceleratorProviderFactory&) = delete;
  MojoVideoEncodeAcceleratorProviderFactory& operator=(
      const MojoVideoEncodeAcceleratorProviderFactory&) = delete;
  ~MojoVideoEncodeAcceleratorProviderFactory() override;

  void BindReceiver(
      mojo::PendingReceiver<mojom::VideoEncodeAcceleratorProviderFactory>
          receiver);

  // mojom::VideoEncodeAcceleratorProviderFactory implementation.
  void CreateVideoEncodeAcceleratorProvider(
      mojo::PendingReceiver<mojom::VideoEncodeAcceleratorProvider> receiver)
      override;

 private:
  mojo::Receiver<mojom::VideoEncodeAcceleratorProviderFactory> receiver_
      GUARDED_BY_CONTEXT(sequence_checker_);
  mojo::UniqueReceiverSet<mojom::VideoEncodeAcceleratorProvider>
      video_encoder_providers_ GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_MOJO_VIDEO_ENCODE_ACCELERATOR_PROVIDER_FACTORY_H_