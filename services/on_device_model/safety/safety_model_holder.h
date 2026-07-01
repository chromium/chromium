// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ON_DEVICE_MODEL_SAFETY_SAFETY_MODEL_HOLDER_H_
#define SERVICES_ON_DEVICE_MODEL_SAFETY_SAFETY_MODEL_HOLDER_H_

#include "base/component_export.h"
#include "base/threading/sequence_bound.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"
#include "services/on_device_model/public/mojom/on_device_model_service.mojom.h"

namespace on_device_model {

// SafetyModelHolder holds a single TextSafetyModel. Its operations may block.
class COMPONENT_EXPORT(ON_DEVICE_MODEL) SafetyModelHolder final {
 public:
  SafetyModelHolder();
  ~SafetyModelHolder();
  SafetyModelHolder(const SafetyModelHolder&) = delete;
  SafetyModelHolder& operator=(const SafetyModelHolder&) = delete;

  static base::SequenceBound<SafetyModelHolder> Create();

  void Reset(mojom::TextSafetyModelParamsPtr params,
             mojo::PendingReceiver<mojom::TextSafetyModel> model);

 private:
  // A connected model, once we've received assets.
  mojo::UniqueReceiverSet<mojom::TextSafetyModel> model_;
};

}  // namespace on_device_model

#endif  // SERVICES_ON_DEVICE_MODEL_SAFETY_SAFETY_MODEL_HOLDER_H_
