// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_MEDIA_FOUNDATION_SERVICE_H_
#define MEDIA_MOJO_SERVICES_MEDIA_FOUNDATION_SERVICE_H_

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "gpu/config/gpu_info.h"
#include "media/mojo/mojom/frame_interface_factory.mojom.h"
#include "media/mojo/mojom/interface_factory.mojom.h"
#include "media/mojo/mojom/media_foundation_service.mojom.h"
#include "media/mojo/services/deferred_destroy_unique_receiver_set.h"
#include "media/mojo/services/media_foundation_mojo_media_client.h"
#include "media/mojo/services/media_mojo_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace media {

// This class is similar to media::CdmService and media::MediaService, with
// extra support for CDM preloading and key system support query.
class MEDIA_MOJO_EXPORT MediaFoundationService final
    : public mojom::MediaFoundationService {
 public:
  // The MediaFoundationService process is NOT sandboxed after startup. The
  // `ensure_sandboxed_cb` must be called after necessary initialization to
  // ensure the process is sandboxed.
  explicit MediaFoundationService(
      mojo::PendingReceiver<mojom::MediaFoundationService> receiver);
  MediaFoundationService(const MediaFoundationService&) = delete;
  MediaFoundationService operator=(const MediaFoundationService&) = delete;
  ~MediaFoundationService() final;

  // mojom::MediaFoundationService implementation:
  void IsKeySystemSupported(const std::string& key_system,
                            IsKeySystemSupportedCallback callback) final;
  void CreateInterfaceFactory(
      mojo::PendingReceiver<mojom::InterfaceFactory> receiver,
      mojo::PendingRemote<mojom::FrameInterfaceFactory> frame_interfaces) final;

 private:
  mojo::Receiver<mojom::MediaFoundationService> receiver_;
  MediaFoundationMojoMediaClient mojo_media_client_;
  DeferredDestroyUniqueReceiverSet<mojom::InterfaceFactory>
      interface_factory_receivers_;
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_MEDIA_FOUNDATION_SERVICE_H_
