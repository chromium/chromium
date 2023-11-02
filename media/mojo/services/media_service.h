// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_MEDIA_SERVICE_H_
#define MEDIA_MOJO_SERVICES_MEDIA_SERVICE_H_

#include <memory>

#include "build/build_config.h"
#include "media/mojo/mojom/frame_interface_factory.mojom.h"
#include "media/mojo/mojom/interface_factory.mojom.h"
#include "media/mojo/mojom/media_service.mojom.h"
#include "media/mojo/services/deferred_destroy_unique_receiver_set.h"
#include "media/mojo/services/media_mojo_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace media {

class MojoMediaClient;

class MEDIA_MOJO_EXPORT MediaService final : public mojom::MediaService {
 public:
  MediaService(std::unique_ptr<MojoMediaClient> mojo_media_client,
               mojo::PendingReceiver<mojom::MediaService> receiver);

  MediaService(const MediaService&) = delete;
  MediaService& operator=(const MediaService&) = delete;

  ~MediaService() final;

 private:
  void Create(mojo::PendingReceiver<mojom::MediaService> receiver);

  // mojom::MediaService implementation:
  void CreateInterfaceFactory(
      mojo::PendingReceiver<mojom::InterfaceFactory> receiver,
      mojo::PendingRemote<mojom::FrameInterfaceFactory> frame_interfaces) final;

  mojo::Receiver<mojom::MediaService> receiver_;

  // Note: Since each instance runs on a different thread, do not share a common
  // MojoMediaClient with other instances to avoid threading issues. Hence using
  // a unique_ptr here.
  //
  // Note: Since |*ref_factory_| is passed to |mojo_media_client_|,
  // |mojo_media_client_| must be destructed before |ref_factory_|.
  std::unique_ptr<MojoMediaClient> mojo_media_client_;

  DeferredDestroyUniqueReceiverSet<mojom::InterfaceFactory>
      interface_factory_receivers_;
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_MEDIA_SERVICE_H_
