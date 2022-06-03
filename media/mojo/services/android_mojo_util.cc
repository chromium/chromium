// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/android_mojo_util.h"

#include "media/mojo/services/mojo_media_drm_storage.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace media {
namespace android_mojo_util {

std::unique_ptr<ProvisionFetcher> CreateProvisionFetcher(
    media::mojom::FrameInterfaceFactory* frame_interfaces) {
  DCHECK(frame_interfaces);
  mojo::PendingRemote<mojom::ProvisionFetcher> provision_fetcher;
  frame_interfaces->CreateProvisionFetcher(
      provision_fetcher.InitWithNewPipeAndPassReceiver());
  return std::make_unique<MojoProvisionFetcher>(std::move(provision_fetcher));
}

std::unique_ptr<MediaDrmStorage> CreateMediaDrmStorage(
    media::mojom::FrameInterfaceFactory* frame_interfaces) {
  DCHECK(frame_interfaces);
  mojo::PendingRemote<mojom::MediaDrmStorage> media_drm_storage;
  frame_interfaces->BindEmbedderReceiver(mojo::GenericPendingReceiver(
      media_drm_storage.InitWithNewPipeAndPassReceiver()));
  return std::make_unique<MojoMediaDrmStorage>(std::move(media_drm_storage));
}

}  // namespace android_mojo_util
}  // namespace media
