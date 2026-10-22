// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ON_DEVICE_MODEL_DECISION_DECISION_MODEL_H_
#define SERVICES_ON_DEVICE_MODEL_DECISION_DECISION_MODEL_H_

#include <memory>

#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"
#include "services/on_device_model/public/mojom/on_device_model_service.mojom.h"

namespace on_device_model {

class DecisionModel final : public mojom::DecisionModel {
 public:
  static std::unique_ptr<DecisionModel> Create(
      mojom::DecisionModelParamsPtr params);

  explicit DecisionModel();
  ~DecisionModel() override;

  DecisionModel(const DecisionModel&) = delete;
  DecisionModel& operator=(const DecisionModel&) = delete;

  // mojom::DecisionModel:
  void StartSession(
      mojom::DecisionQueryPtr query,
      mojo::PendingReceiver<mojom::DecisionSession> session) override;

 private:
  mojo::UniqueReceiverSet<mojom::DecisionSession> sessions_;
};

}  // namespace on_device_model

#endif  // SERVICES_ON_DEVICE_MODEL_DECISION_DECISION_MODEL_H_
