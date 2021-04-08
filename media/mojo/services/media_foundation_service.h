// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_MEDIA_FOUNDATION_SERVICE_H_
#define MEDIA_MOJO_SERVICES_MEDIA_FOUNDATION_SERVICE_H_

#include "build/build_config.h"
#include "media/mojo/mojom/frame_interface_factory.mojom.h"
#include "media/mojo/mojom/interface_factory.mojom.h"
#include "media/mojo/mojom/media_foundation_service.mojom.h"
#include "media/mojo/services/media_foundation_mojo_media_client.h"
#include "media/mojo/services/media_mojo_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"

namespace media {

// This class is similar to media::CdmService and media::MediaService, with
// extra support for CDM preloading and key system support query.
class MEDIA_MOJO_EXPORT MediaFoundationService final
    : public mojom::MediaFoundationService {
 public:
  explicit MediaFoundationService(
      mojo::PendingReceiver<mojom::MediaFoundationService> receiver);
  MediaFoundationService(const MediaFoundationService&) = delete;
  MediaFoundationService operator=(const MediaFoundationService&) = delete;
  ~MediaFoundationService() final;

 private:
  // mojom::MediaFoundationService implementation:
  void Initialize(const base::FilePath& cdm_path) final;
  void IsKeySystemSupported(const std::string& key_system,
                            IsKeySystemSupportedCallback callback) final;
  void CreateInterfaceFactory(
      mojo::PendingReceiver<mojom::InterfaceFactory> receiver,
      mojo::PendingRemote<mojom::FrameInterfaceFactory> frame_interfaces) final;

  mojo::Receiver<mojom::MediaFoundationService> receiver_;
  MediaFoundationMojoMediaClient mojo_media_client_;
  mojo::UniqueReceiverSet<mojom::InterfaceFactory> interface_factory_receivers_;
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_MEDIA_FOUNDATION_SERVICE_H_
