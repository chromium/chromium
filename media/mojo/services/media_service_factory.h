// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_MEDIA_SERVICE_FACTORY_H_
#define MEDIA_MOJO_SERVICES_MEDIA_SERVICE_FACTORY_H_

#include <memory>

#include "build/build_config.h"
#include "media/mojo/mojom/media_service.mojom.h"
#include "media/mojo/services/media_mojo_export.h"
#include "media/mojo/services/media_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace media {
class GpuMojoMediaClient;

// Creates a MediaService instance using the default MojoMediaClient on each
// platform.
std::unique_ptr<MediaService> MEDIA_MOJO_EXPORT
CreateMediaService(mojo::PendingReceiver<mojom::MediaService> receiver);

// Creates a MediaService instance using the GpuMojoMediaClient.
std::unique_ptr<MediaService> MEDIA_MOJO_EXPORT
CreateGpuMediaService(mojo::PendingReceiver<mojom::MediaService> receiver,
                      std::unique_ptr<GpuMojoMediaClient> client);

// Creates a MediaService instance using the TestMojoMediaClient.
std::unique_ptr<MediaService> MEDIA_MOJO_EXPORT CreateMediaServiceForTesting(
    mojo::PendingReceiver<mojom::MediaService> receiver);

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_MEDIA_SERVICE_FACTORY_H_
