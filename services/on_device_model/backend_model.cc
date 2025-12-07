// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/on_device_model/backend_model.h"

namespace on_device_model {

BackendModel::ScopedAdaptation::ScopedAdaptation(
    base::WeakPtr<BackendModel> model,
    uint32_t adaptation_id)
    : model_(std::move(model)), adaptation_id_(adaptation_id) {}

BackendModel::ScopedAdaptation::~ScopedAdaptation() {
  if (model_) {
    model_->UnloadAdaptation(adaptation_id_);
  }
}

}  // namespace on_device_model
