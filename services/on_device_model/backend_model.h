// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ON_DEVICE_MODEL_BACKEND_MODEL_H_
#define SERVICES_ON_DEVICE_MODEL_BACKEND_MODEL_H_

#include <memory>

#include "base/component_export.h"
#include "base/types/expected.h"
#include "services/on_device_model/backend_session.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"
#include "services/on_device_model/public/mojom/on_device_model_service.mojom.h"

namespace on_device_model {

// An interface for a model that can be implemented by different backends.
class COMPONENT_EXPORT(ON_DEVICE_MODEL) BackendModel {
 public:
  // A handle for an adaptation ID that takes care of erasing the session when
  // it is destroyed.
  class COMPONENT_EXPORT(ON_DEVICE_MODEL) ScopedAdaptation {
   public:
    ScopedAdaptation(base::WeakPtr<BackendModel> model, uint32_t adaptation_id);
    ~ScopedAdaptation();

    uint32_t adaptation_id() const { return adaptation_id_; }

   private:
    base::WeakPtr<BackendModel> model_;
    uint32_t adaptation_id_;
  };

  virtual ~BackendModel() = default;

  // Creates a session for this model based on `params`.
  virtual std::unique_ptr<BackendSession> CreateSession(
      const ScopedAdaptation* adaptation,
      on_device_model::mojom::SessionParamsPtr params) = 0;

  // Loads and returns an adaptation based on `params`.
  virtual std::unique_ptr<ScopedAdaptation> LoadAdaptation(
      on_device_model::mojom::LoadAdaptationParamsPtr params) = 0;

  // Unloads an adaptation previously loaded by `LoadAdaptation()`.
  virtual void UnloadAdaptation(uint32_t adaptation_id) = 0;
};

}  // namespace on_device_model

#endif  // SERVICES_ON_DEVICE_MODEL_BACKEND_MODEL_H_
