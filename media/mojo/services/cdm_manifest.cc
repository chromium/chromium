// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/cdm_manifest.h"

#include "base/no_destructor.h"
#include "media/mojo/mojom/cdm_service.mojom.h"
#include "media/mojo/mojom/constants.mojom.h"
#include "services/service_manager/public/cpp/manifest_builder.h"

namespace media {

const service_manager::Manifest& GetCdmManifest() {
  static base::NoDestructor<service_manager::Manifest> manifest{
      service_manager::ManifestBuilder()
          .WithServiceName(mojom::kCdmServiceName)
          .WithDisplayName("Content Decryption Module Service")
          .WithOptions(
              service_manager::ManifestOptionsBuilder()
                  .WithExecutionMode(service_manager::Manifest::ExecutionMode::
                                         kOutOfProcessBuiltin)
                  .WithSandboxType("cdm")
                  .Build())
          .ExposeCapability(
              "media:cdm",
              service_manager::Manifest::InterfaceList<mojom::CdmService>())
          .Build()};
  return *manifest;
}

}  // namespace media
