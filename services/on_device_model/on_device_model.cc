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

  void AddContext(mojom::InputOptionsPtr input) override {
    context_.push_back(input->text);
  }

  void Execute(
      mojom::InputOptionsPtr input,
      mojo::PendingRemote<mojom::StreamingResponder> response) override {
    mojo::Remote<mojom::StreamingResponder> remote(std::move(response));
    for (const std::string& context : context_) {
      remote->OnResponse("Context: " + context + "\n");
    }
    remote->OnResponse("Input: " + input->text + "\n");
    remote->OnComplete();
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
std::unique_ptr<OnDeviceModel> OnDeviceModelService::CreateModel(
    ModelAssets assets) {
  return std::make_unique<OnDeviceModelImpl>();
}

// static
mojom::PerformanceClass OnDeviceModelService::GetEstimatedPerformanceClass() {
  return mojom::PerformanceClass::kError;
}

}  // namespace on_device_model
