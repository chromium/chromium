// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/on_device_model/decision/decision_model.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/types/expected.h"
#include "build/build_config.h"

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_FUCHSIA) && !BUILDFLAG(IS_IOS)
#include "services/on_device_model/decision/laya_model.h"
#endif

namespace on_device_model {

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_FUCHSIA) && !BUILDFLAG(IS_IOS)
namespace {

class DecisionSessionImpl final : public mojom::DecisionSession {
 public:
  explicit DecisionSessionImpl(LayaModel::Session session)
      : session_(std::move(session)) {}
  ~DecisionSessionImpl() override = default;

  DecisionSessionImpl(const DecisionSessionImpl&) = delete;
  DecisionSessionImpl& operator=(const DecisionSessionImpl&) = delete;

  // mojom::DecisionSession:
  void Decide(const std::string& input, DecideCallback callback) override {
    // TODO(crbug.com/565849508): Run Laya model loading and inference on a
    // background MayBlock() sequence so they don't block the service thread.
    base::expected<std::vector<std::vector<float>>, std::string> probs =
        session_.Execute(input);
    if (!probs.has_value()) {
      DVLOG(1) << "LayaModel::Session::Execute failed: " << probs.error();
      std::move(callback).Run(std::nullopt);
      return;
    }

    std::vector<mojom::DecisionQuestionResultPtr> results;
    results.reserve(probs->size());
    for (auto& question_probs : *probs) {
      results.push_back(
          mojom::DecisionQuestionResult::New(std::move(question_probs)));
    }
    std::move(callback).Run(std::move(results));
  }

 private:
  LayaModel::Session session_;
};

}  // namespace

// static
std::unique_ptr<DecisionModel> DecisionModel::Create(
    mojom::DecisionModelParamsPtr params) {
  if (!params || !params->assets.weights.IsFile() ||
      !params->assets.weights.file().IsValid()) {
    return nullptr;
  }
  base::expected<std::unique_ptr<LayaModel>, std::string> model =
      LayaModel::Create(std::move(params->assets.weights.file()),
                        std::move(params->assets.cache));
  if (!model.has_value()) {
    DVLOG(1) << "LayaModel::Create failed: " << model.error();
    return nullptr;
  }
  return std::make_unique<DecisionModel>(std::move(*model));
}

DecisionModel::DecisionModel(std::unique_ptr<LayaModel> model)
    : model_(std::move(model)) {}

DecisionModel::~DecisionModel() = default;

void DecisionModel::StartSession(
    mojom::DecisionQueryPtr query,
    mojo::PendingReceiver<mojom::DecisionSession> session) {
  base::expected<LayaModel::Session, std::string> laya_session =
      model_->CreateSession(std::move(query));
  if (!laya_session.has_value()) {
    DVLOG(1) << "LayaModel::CreateSession failed: " << laya_session.error();
    session.ResetWithReason(/*reason=*/0, laya_session.error());
    return;
  }
  sessions_.Add(std::make_unique<DecisionSessionImpl>(std::move(*laya_session)),
                std::move(session));
}

#else

class LayaModel {};

// static
std::unique_ptr<DecisionModel> DecisionModel::Create(
    mojom::DecisionModelParamsPtr params) {
  return nullptr;
}

DecisionModel::DecisionModel(std::unique_ptr<LayaModel> model)
    : model_(std::move(model)) {}

DecisionModel::~DecisionModel() = default;

void DecisionModel::StartSession(
    mojom::DecisionQueryPtr query,
    mojo::PendingReceiver<mojom::DecisionSession> session) {
  session.ResetWithReason(/*reason=*/0, "Unsupported platform.");
}

#endif

}  // namespace on_device_model
