// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/on_device_model/decision/decision_model.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace on_device_model {
namespace {

class DecisionSessionImpl final : public mojom::DecisionSession {
 public:
  explicit DecisionSessionImpl() = default;
  ~DecisionSessionImpl() override = default;

  DecisionSessionImpl(const DecisionSessionImpl&) = delete;
  DecisionSessionImpl& operator=(const DecisionSessionImpl&) = delete;

  // mojom::DecisionSession:
  void Decide(const std::string& input, DecideCallback callback) override {
    std::move(callback).Run({{}});
  }
};

}  // namespace

// static
std::unique_ptr<DecisionModel> DecisionModel::Create(
    mojom::DecisionModelParamsPtr params) {
  if (!params || !params->assets.weights.IsFile() ||
      !params->assets.weights.file().IsValid()) {
    return nullptr;
  }
  return std::make_unique<DecisionModel>();
}

DecisionModel::DecisionModel() = default;

DecisionModel::~DecisionModel() = default;

void DecisionModel::StartSession(
    mojom::DecisionQueryPtr query,
    mojo::PendingReceiver<mojom::DecisionSession> session) {
  sessions_.Add(std::make_unique<DecisionSessionImpl>(), std::move(session));
}

}  // namespace on_device_model
