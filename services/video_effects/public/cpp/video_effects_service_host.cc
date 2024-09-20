// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_effects/public/cpp/video_effects_service_host.h"

#include "base/auto_reset.h"
#include "content/public/browser/service_process_host.h"
#include "content/public/browser/service_process_host_passkeys.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/on_device_model/ml/chrome_ml.h"
#include "services/video_effects/public/mojom/video_effects_service.mojom.h"

namespace {

static mojo::Remote<video_effects::mojom::VideoEffectsService>*
    g_service_remote = nullptr;

}

namespace video_effects {

base::AutoReset<mojo::Remote<video_effects::mojom::VideoEffectsService>*>
SetVideoEffectsServiceRemoteForTesting(
    mojo::Remote<video_effects::mojom::VideoEffectsService>* service_override) {
  return base::AutoReset<
      mojo::Remote<video_effects::mojom::VideoEffectsService>*>(
      &g_service_remote, service_override);
}

video_effects::mojom::VideoEffectsService* GetVideoEffectsService() {
  if (!g_service_remote) {
    g_service_remote =
        new mojo::Remote<video_effects::mojom::VideoEffectsService>();
  }

  if (!g_service_remote->is_bound()) {
    content::ServiceProcessHost::Launch(
        g_service_remote->BindNewPipeAndPassReceiver(),
        content::ServiceProcessHost::Options()
            .WithDisplayName("Video Effects Service")
#if BUILDFLAG(IS_WIN)
            .WithPreloadedLibraries(
                {ml::GetChromeMLPath()},
                content::ServiceProcessHostPreloadLibraries::GetPassKey())
#endif
            .Pass());

    g_service_remote->reset_on_disconnect();
    g_service_remote->reset_on_idle_timeout(base::Seconds(5));
  }

  return g_service_remote->get();
}

}  // namespace video_effects
