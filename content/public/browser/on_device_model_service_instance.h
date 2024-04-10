// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_ON_DEVICE_MODEL_SERVICE_INSTANCE_H_
#define CONTENT_PUBLIC_BROWSER_ON_DEVICE_MODEL_SERVICE_INSTANCE_H_

#include "base/component_export.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/on_device_model/public/mojom/on_device_model_service.mojom.h"

namespace content {

// Get the remote on device model service singleton instance.
CONTENT_EXPORT const mojo::Remote<on_device_model::mojom::OnDeviceModelService>&
GetRemoteOnDeviceModelService();

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_ON_DEVICE_MODEL_SERVICE_INSTANCE_H_
