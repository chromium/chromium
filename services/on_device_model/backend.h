// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ON_DEVICE_MODEL_BACKEND_H_
#define SERVICES_ON_DEVICE_MODEL_BACKEND_H_

#include "base/component_export.h"
#include "base/types/expected.h"
#include "services/on_device_model/backend_model.h"
#include "services/on_device_model/public/cpp/service_client.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"
#include "services/on_device_model/public/mojom/on_device_model_service.mojom.h"

namespace on_device_model {

class BackendModel;

// A backend that is able to create models and get other info about the device.
class COMPONENT_EXPORT(ON_DEVICE_MODEL) Backend
    : public base::RefCountedThreadSafe<Backend> {
 public:
  // Whether the backend is able to create a model.
  virtual base::expected<void, ServiceDisconnectReason> CanCreate() = 0;

  // Returns the capabilities for the corresponding `model_file`.
  virtual Capabilities GetCapabilities(ModelFile model_file) = 0;

  // Creates a new model given `params`. Will call `on_complete` when any async
  // processing of the model has finished.
  virtual base::expected<std::unique_ptr<BackendModel>, mojom::LoadModelResult>
  CreateWithResult(mojom::LoadModelParamsPtr params,
                   base::OnceClosure on_complete) = 0;

  // Binds a text safety model to `model` given `params`.
  virtual void LoadTextSafetyModel(
      mojom::TextSafetyModelParamsPtr params,
      mojo::PendingReceiver<mojom::TextSafetyModel> model) = 0;

  // Gets the device and performance information for this device.
  virtual std::pair<mojom::DevicePerformanceInfoPtr, mojom::DeviceInfoPtr>
  GetDeviceAndPerformanceInfo() = 0;

 protected:
  friend class base::RefCountedThreadSafe<Backend>;
  Backend() = default;
  virtual ~Backend() = default;
};

}  // namespace on_device_model

#endif  // SERVICES_ON_DEVICE_MODEL_BACKEND_H_
