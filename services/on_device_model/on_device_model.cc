// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/on_device_model/public/cpp/on_device_model.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/on_device_model/on_device_model_service.h"

namespace on_device_model {
namespace {

class SessionImpl : public OnDeviceModel::Session {
 public:
  SessionImpl() = default;
  ~SessionImpl() override = default;

  SessionImpl(const SessionImpl&) = delete;
  SessionImpl& operator=(const SessionImpl&) = delete;

  void AddContext(mojom::InputOptionsPtr input,
                  mojo::PendingRemote<mojom::ContextClient> client) override {
    std::string text = input->text;
    if (input->token_offset) {
      text.erase(text.begin(), text.begin() + *input->token_offset);
    }
    if (input->max_tokens && *input->max_tokens < text.size()) {
      text.resize(*input->max_tokens);
    }
    context_.push_back(text);
    if (client) {
      mojo::Remote<mojom::ContextClient> remote(std::move(client));
      remote->OnComplete(text.size());
    }
  }

  void Execute(
      mojom::InputOptionsPtr input,
      mojo::PendingRemote<mojom::StreamingResponder> response) override {
    mojo::Remote<mojom::StreamingResponder> remote(std::move(response));
    if (!input->ignore_context) {
      for (const std::string& context : context_) {
        remote->OnResponse("Context: " + context + "\n");
      }
    }
    remote->OnResponse("Input: " + input->text + "\n");
    remote->OnComplete(mojom::ResponseStatus::kOk);
  }

 private:
  std::vector<std::string> context_;
};

class OnDeviceModelImpl : public OnDeviceModel {
 public:
  OnDeviceModelImpl() = default;
  ~OnDeviceModelImpl() override = default;

  OnDeviceModelImpl(const OnDeviceModelImpl&) = delete;
  OnDeviceModelImpl& operator=(const OnDeviceModelImpl&) = delete;

  std::unique_ptr<Session> CreateSession() override {
    return std::make_unique<SessionImpl>();
  }
};

}  // namespace

// static
base::expected<std::unique_ptr<OnDeviceModel>, mojom::LoadModelResult>
OnDeviceModelService::CreateModel(mojom::LoadModelParamsPtr params) {
  return base::ok<std::unique_ptr<OnDeviceModel>>(
      std::make_unique<OnDeviceModelImpl>());
}

// static
mojom::PerformanceClass OnDeviceModelService::GetEstimatedPerformanceClass() {
  return mojom::PerformanceClass::kFailedToLoadLibrary;
}

}  // namespace on_device_model
