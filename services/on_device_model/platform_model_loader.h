// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ON_DEVICE_MODEL_PLATFORM_MODEL_LOADER_H_
#define SERVICES_ON_DEVICE_MODEL_PLATFORM_MODEL_LOADER_H_

#include <cstdint>
#include <optional>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/types/expected.h"
#include "base/uuid.h"
#include "services/on_device_model/public/cpp/on_device_model.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"
#include "services/on_device_model/public/mojom/on_device_model_service.mojom.h"

namespace on_device_model {

class PlatformModelLoader {
 public:
  using LoadModelCallback = base::OnceCallback<void(mojom::LoadModelResult)>;

  PlatformModelLoader() = default;
  virtual ~PlatformModelLoader() = default;

  PlatformModelLoader(const PlatformModelLoader&) = delete;
  PlatformModelLoader& operator=(const PlatformModelLoader&) = delete;

  virtual void LoadModelWithUuid(
      const base::Uuid& uuid,
      mojo::PendingReceiver<mojom::OnDeviceModel> model,
      LoadModelCallback callback) = 0;
};

}  // namespace on_device_model

#endif  // SERVICES_ON_DEVICE_MODEL_PLATFORM_MODEL_LOADER_H_
