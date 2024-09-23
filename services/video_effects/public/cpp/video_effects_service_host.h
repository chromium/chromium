// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_EFFECTS_PUBLIC_CPP_VIDEO_EFFECTS_SERVICE_HOST_H_
#define SERVICES_VIDEO_EFFECTS_PUBLIC_CPP_VIDEO_EFFECTS_SERVICE_HOST_H_

#include "base/auto_reset.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/video_effects/public/mojom/video_effects_service.mojom-forward.h"

namespace video_effects {

base::AutoReset<mojo::Remote<video_effects::mojom::VideoEffectsService>*>
SetVideoEffectsServiceRemoteForTesting(
    mojo::Remote<video_effects::mojom::VideoEffectsService>* service_override);

video_effects::mojom::VideoEffectsService* GetVideoEffectsService();

}  // namespace video_effects

#endif  // SERVICES_VIDEO_EFFECTS_PUBLIC_CPP_VIDEO_EFFECTS_SERVICE_HOST_H_
