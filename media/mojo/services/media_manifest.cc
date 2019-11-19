// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/media_manifest.h"

#include "base/no_destructor.h"
#include "media/mojo/buildflags.h"
#include "media/mojo/mojom/constants.mojom.h"
#include "media/mojo/mojom/media_service.mojom.h"
#include "services/service_manager/public/cpp/manifest_builder.h"

#if defined(IS_CHROMECAST)
#include "chromecast/common/mojom/constants.mojom.h"
#endif

namespace media {

const service_manager::Manifest& GetMediaManifest() {
  static base::NoDestructor<service_manager::Manifest> manifest {
    service_manager::ManifestBuilder()
        .WithServiceName(mojom::kMediaServiceName)
        .WithDisplayName("Media Service")
        .WithOptions(
            service_manager::ManifestOptionsBuilder()
#if BUILDFLAG(ENABLE_MOJO_MEDIA_IN_UTILITY_PROCESS) || \
    BUILDFLAG(ENABLE_MOJO_MEDIA_IN_GPU_PROCESS)
                .WithExecutionMode(service_manager::Manifest::ExecutionMode::
                                       kOutOfProcessBuiltin)
                .WithSandboxType("utility")
#else
                .WithExecutionMode(
                    service_manager::Manifest::ExecutionMode::kInProcessBuiltin)
#endif
                .Build())
        .ExposeCapability(
            "media:media",
            service_manager::Manifest::InterfaceList<mojom::MediaService>())
#if defined(IS_CHROMECAST)
        .RequireCapability(chromecast::mojom::kChromecastServiceName,
                           "multizone")
#endif
        .Build()
  };
  return *manifest;
}

const service_manager::Manifest& GetMediaRendererManifest() {
  static base::NoDestructor<service_manager::Manifest> manifest {
    service_manager::ManifestBuilder()
        .WithServiceName(mojom::kMediaRendererServiceName)
        .WithDisplayName("Media Renderer Service")
        .WithOptions(
            service_manager::ManifestOptionsBuilder()
                .WithExecutionMode(
                    service_manager::Manifest::ExecutionMode::kInProcessBuiltin)
                .Build())
        .ExposeCapability(
            "media:media",
            service_manager::Manifest::InterfaceList<mojom::MediaService>())
#if defined(IS_CHROMECAST)
        .RequireCapability(chromecast::mojom::kChromecastServiceName,
                           "multizone")
#endif
        .Build()
  };
  return *manifest;
}

}  // namespace media
