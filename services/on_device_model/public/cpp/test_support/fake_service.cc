// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/on_device_model/public/cpp/test_support/fake_service.h"

#include "base/check.h"
#include "base/containers/span.h"
#include "base/files/memory_mapped_file.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/to_string.h"

namespace on_device_model {

namespace {

std::string ReadFile(base::File& file) {
  // Using MemoryMappedFile to handle async file.
  base::MemoryMappedFile map;
  CHECK(map.Initialize(std::move(file)));
  return std::string(base::as_string_view(base::as_chars(map.bytes())));
}

std::string OnDeviceInputToString(const mojom::Input& input) {
  std::ostringstream oss;
  for (const auto& piece : input.pieces) {
    if (std::holds_alternative<std::string>(piece)) {
      oss << std::get<std::string>(piece);
    }
  }
  return oss.str();
}

std::string CtxToString(const mojom::InputOptions& input) {
  std::string suffix;
  std::string context = OnDeviceInputToString(*input.input);
  if (input.token_offset) {
    context.erase(context.begin(), context.begin() + *input.token_offset);
    suffix += " off:" + base::NumberToString(*input.token_offset);
  }
  if (input.max_tokens) {
    if (input.max_tokens < context.size()) {
      context.resize(*input.max_tokens);
    }
    suffix += " max:" + base::NumberToString(*input.max_tokens);
  }
  return context + suffix;
}

}  // namespace

FakeOnDeviceServiceSettings::FakeOnDeviceServiceSettings() = default;
FakeOnDeviceServiceSettings::~FakeOnDeviceServiceSettings() = default;

FakeOnDeviceSession::FakeOnDeviceSession(
    FakeOnDeviceServiceSettings* settings,
    const std::string& adaptation_model_weight,
    FakeOnDeviceModel* model)
    : settings_(settings),
      adaptation_model_weight_(adaptation_model_weight),
      model_(model) {}

FakeOnDeviceSession::~FakeOnDeviceSession() = default;

void FakeOnDeviceSession::AddContext(
    mojom::InputOptionsPtr input,
    mojo::PendingRemote<mojom::ContextClient> client) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&FakeOnDeviceSession::AddContextInternal,
                                weak_factory_.GetWeakPtr(), std::move(input),
                                std::move(client)));
}

void FakeOnDeviceSession::Execute(
    mojom::InputOptionsPtr input,
    mojo::PendingRemote<mojom::StreamingResponder> response) {
  if (settings_->execute_delay.is_zero()) {
    ExecuteImpl(std::move(input), std::move(response));
    return;
  }
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&FakeOnDeviceSession::ExecuteImpl,
                     weak_factory_.GetWeakPtr(), std::move(input),
                     std::move(response)),
      settings_->execute_delay);
}

void FakeOnDeviceSession::GetSizeInTokensDeprecated(const std::string& text,
                                          GetSizeInTokensCallback callback) {
  std::move(callback).Run(0);
}

void FakeOnDeviceSession::GetSizeInTokens(mojom::InputPtr input,
                                          GetSizeInTokensCallback callback) {
  std::move(callback).Run(0);
}

void FakeOnDeviceSession::Score(const std::string& text,
                                ScoreCallback callback) {
  std::move(callback).Run(0.5);
}

void FakeOnDeviceSession::Clone(
    mojo::PendingReceiver<on_device_model::mojom::Session> session) {
  auto new_session = std::make_unique<FakeOnDeviceSession>(
      settings_, adaptation_model_weight_, model_);
  for (const auto& c : context_) {
    new_session->context_.push_back(c->Clone());
  }
  model_->AddSession(std::move(session), std::move(new_session));
}

void FakeOnDeviceSession::ExecuteImpl(
    mojom::InputOptionsPtr input,
    mojo::PendingRemote<mojom::StreamingResponder> response) {
  mojo::Remote<mojom::StreamingResponder> remote(std::move(response));
  for (const auto& context : context_) {
    auto chunk = mojom::ResponseChunk::New();
    chunk->text = "Context: " + CtxToString(*context) + "\n";
    remote->OnResponse(std::move(chunk));
  }
  if (!adaptation_model_weight_.empty()) {
    auto chunk = mojom::ResponseChunk::New();
    chunk->text = "Adaptation model: " + adaptation_model_weight_ + "\n";
    remote->OnResponse(std::move(chunk));
  }

  if (settings_->model_execute_result.empty()) {
    auto chunk = mojom::ResponseChunk::New();
    chunk->text = "Input: " + OnDeviceInputToString(*input->input) + "\n";
    if (input->top_k > 1) {
      chunk->text += "TopK: " + base::NumberToString(*input->top_k) +
                     ", Temp: " + base::NumberToString(*input->temperature) +
                     "\n";
    }
    remote->OnResponse(std::move(chunk));
  } else {
    for (const auto& text : settings_->model_execute_result) {
      auto chunk = mojom::ResponseChunk::New();
      chunk->text = text;
      remote->OnResponse(std::move(chunk));
    }
  }
  auto summary = mojom::ResponseSummary::New();
  remote->OnComplete(std::move(summary));
}

void FakeOnDeviceSession::AddContextInternal(
    mojom::InputOptionsPtr input,
    mojo::PendingRemote<mojom::ContextClient> client) {
  uint32_t input_tokens =
      static_cast<uint32_t>(OnDeviceInputToString(*input->input).size());
  uint32_t max_tokens = input->max_tokens.value_or(input_tokens);
  uint32_t token_offset = input->token_offset.value_or(0);
  uint32_t tokens_processed = std::min(input_tokens - token_offset, max_tokens);
  context_.emplace_back(std::move(input));
  if (client) {
    mojo::Remote<mojom::ContextClient> remote(std::move(client));
    remote->OnComplete(tokens_processed);
  }
}

FakeOnDeviceModel::FakeOnDeviceModel(FakeOnDeviceServiceSettings* settings,
                                     FakeOnDeviceModel::Data&& data)
    : settings_(settings), data_(std::move(data)) {}

FakeOnDeviceModel::~FakeOnDeviceModel() = default;

void FakeOnDeviceModel::StartSession(
    mojo::PendingReceiver<mojom::Session> session) {
  AddSession(std::move(session),
             std::make_unique<FakeOnDeviceSession>(
                 settings_, data_.adaptation_model_weight, this));
}

void FakeOnDeviceModel::AddSession(
    mojo::PendingReceiver<mojom::Session> receiver,
    std::unique_ptr<FakeOnDeviceSession> session) {
  // Mirror what the real OnDeviceModel does, which is only allow a single
  // Session.
  receivers_.Clear();
  receivers_.Add(std::move(session), std::move(receiver));
}

void FakeOnDeviceModel::DetectLanguage(const std::string& text,
                                       DetectLanguageCallback callback) {
  NOTREACHED();
}

void FakeOnDeviceModel::ClassifyTextSafety(
    const std::string& text,
    ClassifyTextSafetyCallback callback) {
  NOTREACHED();
}

void FakeOnDeviceModel::LoadAdaptation(
    mojom::LoadAdaptationParamsPtr params,
    mojo::PendingReceiver<mojom::OnDeviceModel> model,
    LoadAdaptationCallback callback) {
  Data data = data_;
  data.adaptation_model_weight = ReadFile(params->assets.weights);
  auto test_model =
      std::make_unique<FakeOnDeviceModel>(settings_, std::move(data));
  model_adaptation_receivers_.Add(std::move(test_model), std::move(model));
  std::move(callback).Run(mojom::LoadModelResult::kSuccess);
}

FakeTsModel::FakeTsModel(
    on_device_model::mojom::TextSafetyModelParamsPtr params) {
  if (params->ts_assets) {
    CHECK_EQ(ReadFile(params->ts_assets->data), FakeTsData());
    CHECK_EQ(ReadFile(params->ts_assets->sp_model), FakeTsSpModel());
    has_safety_model_ = true;
  }
  if (params->language_assets) {
    CHECK_EQ(ReadFile(params->language_assets->model), FakeLanguageModel());
    has_language_model_ = true;
  }
}
FakeTsModel::~FakeTsModel() = default;

void FakeTsModel::ClassifyTextSafety(const std::string& text,
                                     ClassifyTextSafetyCallback callback) {
  CHECK(has_safety_model_);
  auto safety_info = mojom::SafetyInfo::New();
  // Text is unsafe if it contains "unsafe".
  bool has_unsafe = text.find("unsafe") != std::string::npos;
  safety_info->class_scores.emplace_back(has_unsafe ? 0.8 : 0.2);

  bool has_reasonable = text.find("reasonable") != std::string::npos;
  safety_info->class_scores.emplace_back(has_reasonable ? 0.2 : 0.8);

  if (has_language_model_) {
    if (text.find("esperanto") != std::string::npos) {
      safety_info->language = mojom::LanguageDetectionResult::New("eo", 1.0);
    }
  }
  std::move(callback).Run(std::move(safety_info));
}
void FakeTsModel::DetectLanguage(const std::string& text,
                                 DetectLanguageCallback callback) {
  CHECK(has_language_model_);
  mojom::LanguageDetectionResultPtr language;
  if (text.find("esperanto") != std::string::npos) {
    language = mojom::LanguageDetectionResult::New("eo", 1.0);
  }
  std::move(callback).Run(std::move(language));
}

FakeTsHolder::FakeTsHolder() = default;
FakeTsHolder::~FakeTsHolder() = default;

void FakeTsHolder::Reset(
    on_device_model::mojom::TextSafetyModelParamsPtr params,
    mojo::PendingReceiver<on_device_model::mojom::TextSafetyModel>
        model_receiver) {
  model_.Clear();
  model_.Add(std::make_unique<FakeTsModel>(std::move(params)),
             std::move(model_receiver));
}

FakeOnDeviceModelService::FakeOnDeviceModelService(
    mojo::PendingReceiver<mojom::OnDeviceModelService> receiver,
    FakeOnDeviceServiceSettings* settings)
    : settings_(settings), receiver_(this, std::move(receiver)) {}

FakeOnDeviceModelService::~FakeOnDeviceModelService() = default;

void FakeOnDeviceModelService::LoadModel(
    mojom::LoadModelParamsPtr params,
    mojo::PendingReceiver<mojom::OnDeviceModel> model,
    LoadModelCallback callback) {
  if (settings_->drop_connection_request) {
    std::move(callback).Run(settings_->load_model_result);
    return;
  }
  auto test_model =
      std::make_unique<FakeOnDeviceModel>(settings_, FakeOnDeviceModel::Data{});
  model_receivers_.Add(std::move(test_model), std::move(model));
  std::move(callback).Run(settings_->load_model_result);
}

void FakeOnDeviceModelService::LoadTextSafetyModel(
    mojom::TextSafetyModelParamsPtr params,
    mojo::PendingReceiver<mojom::TextSafetyModel> model) {
  ts_holder_.Reset(std::move(params), std::move(model));
}

void FakeOnDeviceModelService::GetEstimatedPerformanceClass(
    GetEstimatedPerformanceClassCallback callback) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), mojom::PerformanceClass::kVeryHigh),
      settings_->estimated_performance_delay);
}

}  // namespace on_device_model
