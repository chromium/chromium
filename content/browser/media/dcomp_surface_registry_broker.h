// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_DCOMP_SURFACE_REGISTRY_BROKER_H_
#define CONTENT_BROWSER_MEDIA_DCOMP_SURFACE_REGISTRY_BROKER_H_

#include "media/mojo/mojom/dcomp_surface_registry.mojom.h"

namespace content {

// This class is hosted in the browser process and it allows unprivileged
// MediaFoundationService (utility) process client to register DCOMP surface
// handles for GPU process access.
// This class is bound and lives on the browser UI thread but will post to IO
// to do the real work since GpuProcessHost must be called on the IO thread.
class DCOMPSurfaceRegistryBroker : public media::mojom::DCOMPSurfaceRegistry {
 public:
  DCOMPSurfaceRegistryBroker();
  DCOMPSurfaceRegistryBroker(const DCOMPSurfaceRegistryBroker&) = delete;
  DCOMPSurfaceRegistryBroker& operator=(const DCOMPSurfaceRegistryBroker&) =
      delete;
  ~DCOMPSurfaceRegistryBroker() override;

  // media::mojom::DCOMPSurfaceRegistry:
  void RegisterDCOMPSurfaceHandle(
      mojo::PlatformHandle surface_handle,
      RegisterDCOMPSurfaceHandleCallback callback) override;
  void UnregisterDCOMPSurfaceHandle(
      const base::UnguessableToken& token) override;
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_DCOMP_SURFACE_REGISTRY_BROKER_H_
