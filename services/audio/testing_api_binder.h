// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_AUDIO_TESTING_API_BINDER_H_
#define SERVICES_AUDIO_TESTING_API_BINDER_H_

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "services/audio/public/mojom/system_info.mojom.h"
#include "services/audio/public/mojom/testing_api.mojom.h"

namespace audio {

// Exposes access to global storage for a callback that can bind a TestingApi
// receiver. Test environments can use this to inject an implementation of
// TestingApi, and the service will use it if available.
using TestingApiBinder =
    base::RepeatingCallback<void(mojo::PendingReceiver<mojom::TestingApi>)>;
COMPONENT_EXPORT(AUDIO_SERVICE_TESTING_API_SUPPORT)
TestingApiBinder& GetTestingApiBinder();

using SystemInfoBinder =
    base::RepeatingCallback<void(mojo::PendingReceiver<mojom::SystemInfo>)>;
COMPONENT_EXPORT(AUDIO_SERVICE_TESTING_API_SUPPORT)
SystemInfoBinder& GetSystemInfoBinderForTesting();

}  // namespace audio

#endif  // SERVICES_AUDIO_TESTING_API_BINDER_H_
