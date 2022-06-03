// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/media_service.h"

#include "base/bind.h"
#include "base/check.h"
#include "media/media_buildflags.h"
#include "media/mojo/mojom/interface_factory.mojom.h"
#include "media/mojo/services/interface_factory_impl.h"
#include "media/mojo/services/mojo_media_client.h"

namespace media {

MediaService::MediaService(std::unique_ptr<MojoMediaClient> mojo_media_client,
                           mojo::PendingReceiver<mojom::MediaService> receiver)
    : receiver_(this, std::move(receiver)),
      mojo_media_client_(std::move(mojo_media_client)) {
  DCHECK(mojo_media_client_);
  mojo_media_client_->Initialize();
}

MediaService::~MediaService() = default;

void MediaService::CreateInterfaceFactory(
    mojo::PendingReceiver<mojom::InterfaceFactory> receiver,
    mojo::PendingRemote<mojom::FrameInterfaceFactory> frame_interfaces) {
  // Ignore request if service has already stopped.
  if (!mojo_media_client_)
    return;

  interface_factory_receivers_.Add(
      std::make_unique<InterfaceFactoryImpl>(std::move(frame_interfaces),
                                             mojo_media_client_.get()),
      std::move(receiver));
}

}  // namespace media
