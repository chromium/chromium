// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef SERVICES_ON_DEVICE_MODEL_PUBLIC_CPP_TEST_SUPPORT_FAKE_SERVICE_H_
#define SERVICES_ON_DEVICE_MODEL_PUBLIC_CPP_TEST_SUPPORT_FAKE_SERVICE_H_

#include <cstdint>

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"
#include "services/on_device_model/public/cpp/model_assets.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"
#include "services/on_device_model/public/mojom/on_device_model_service.mojom.h"

namespace on_device_model {

class FakeOnDeviceModel;

// The expected content of safety model files.
inline constexpr std::string FakeTsData() {
  return "fake_ts_data";
}
inline constexpr std::string FakeTsSpModel() {
  return "fake_ts_sp_model";
}
inline constexpr std::string FakeLanguageModel() {
  return "fake_language_model";
}

// Hooks for tests to control the FakeOnDeviceService behavior.
struct FakeOnDeviceServiceSettings final {
  FakeOnDeviceServiceSettings();
  ~FakeOnDeviceServiceSettings();

  // If non-zero this amount of delay is added before the response is sent.
  base::TimeDelta execute_delay;

  // The delay before running the GetEstimatedPerformanceClass() response
  // callback.
  base::TimeDelta estimated_performance_delay;

  // If non-empty, used as the output from Execute().
  std::vector<std::string> model_execute_result;

  mojom::LoadModelResult load_model_result = mojom::LoadModelResult::kSuccess;

  bool drop_connection_request = false;

  void set_execute_delay(base::TimeDelta delay) { execute_delay = delay; }

  void set_estimated_performance_delay(base::TimeDelta delay) {
    estimated_performance_delay = delay;
  }

  void set_execute_result(const std::vector<std::string>& result) {
    model_execute_result = result;
  }

  void set_load_model_result(mojom::LoadModelResult result) {
    load_model_result = result;
  }

  void set_drop_connection_request(bool value) {
    drop_connection_request = value;
  }
};

class FakeOnDeviceSession final : public mojom::Session {
 public:
  explicit FakeOnDeviceSession(FakeOnDeviceServiceSettings* settings,
                               const std::string& adaptation_model_weight,
                               FakeOnDeviceModel* model);
  ~FakeOnDeviceSession() override;

  // mojom::Session:
  void AddContext(mojom::InputOptionsPtr input,
                  mojo::PendingRemote<mojom::ContextClient> client) override;

  void Execute(
      mojom::InputOptionsPtr input,
      mojo::PendingRemote<mojom::StreamingResponder> response) override;

  void GetSizeInTokensDeprecated(const std::string& text,
                       GetSizeInTokensCallback callback) override;
  void GetSizeInTokens(mojom::InputPtr input,
                       GetSizeInTokensCallback callback) override;

  void Score(const std::string& text, ScoreCallback callback) override;

  void Clone(
      mojo::PendingReceiver<on_device_model::mojom::Session> session) override;

 private:
  void ExecuteImpl(mojom::InputOptionsPtr input,
                   mojo::PendingRemote<mojom::StreamingResponder> response);

  void AddContextInternal(mojom::InputOptionsPtr input,
                          mojo::PendingRemote<mojom::ContextClient> client);

  raw_ptr<FakeOnDeviceServiceSettings> settings_;
  std::string adaptation_model_weight_;
  std::vector<mojom::InputOptionsPtr> context_;
  raw_ptr<FakeOnDeviceModel> model_;

  base::WeakPtrFactory<FakeOnDeviceSession> weak_factory_{this};
};

class FakeOnDeviceModel : public mojom::OnDeviceModel {
 public:
  struct Data {
    std::string adaptation_model_weight = "";
  };
  explicit FakeOnDeviceModel(FakeOnDeviceServiceSettings* settings,
                             Data&& data);
  ~FakeOnDeviceModel() override;

  // mojom::OnDeviceModel:
  void StartSession(mojo::PendingReceiver<mojom::Session> session) override;

  void DetectLanguage(const std::string& text,
                      DetectLanguageCallback callback) override;

  void ClassifyTextSafety(const std::string& text,
                          ClassifyTextSafetyCallback callback) override;

  void LoadAdaptation(mojom::LoadAdaptationParamsPtr params,
                      mojo::PendingReceiver<mojom::OnDeviceModel> model,
                      LoadAdaptationCallback callback) override;

  void AddSession(
      mojo::PendingReceiver<on_device_model::mojom::Session> receiver,
      std::unique_ptr<FakeOnDeviceSession> session);

 private:
  raw_ptr<FakeOnDeviceServiceSettings> settings_;
  Data data_;

  mojo::UniqueReceiverSet<mojom::Session> receivers_;
  mojo::UniqueReceiverSet<mojom::OnDeviceModel> model_adaptation_receivers_;
};

class FakeTsModel final : public on_device_model::mojom::TextSafetyModel {
 public:
  explicit FakeTsModel(on_device_model::mojom::TextSafetyModelParamsPtr params);
  ~FakeTsModel() override;

  // on_device_model::mojom::TextSafetyModel
  void ClassifyTextSafety(const std::string& text,
                          ClassifyTextSafetyCallback callback) override;
  void DetectLanguage(const std::string& text,
                      DetectLanguageCallback callback) override;

 private:
  bool has_safety_model_ = false;
  bool has_language_model_ = false;
};

// TsHolder holds a single TsModel. Its operations may block.
class FakeTsHolder final {
 public:
  explicit FakeTsHolder();
  ~FakeTsHolder();

  void Reset(on_device_model::mojom::TextSafetyModelParamsPtr params,
             mojo::PendingReceiver<on_device_model::mojom::TextSafetyModel>
                 model_receiver);

 private:
  mojo::UniqueReceiverSet<on_device_model::mojom::TextSafetyModel> model_;
};

class FakeOnDeviceModelService : public mojom::OnDeviceModelService {
 public:
  FakeOnDeviceModelService(
      mojo::PendingReceiver<mojom::OnDeviceModelService> receiver,
      FakeOnDeviceServiceSettings* settings);
  ~FakeOnDeviceModelService() override;

  size_t on_device_model_receiver_count() const {
    return model_receivers_.size();
  }

 private:
  // mojom::OnDeviceModelService:
  void LoadModel(mojom::LoadModelParamsPtr params,
                 mojo::PendingReceiver<mojom::OnDeviceModel> model,
                 LoadModelCallback callback) override;
  void LoadTextSafetyModel(
      mojom::TextSafetyModelParamsPtr params,
      mojo::PendingReceiver<mojom::TextSafetyModel> model) override;
  void GetEstimatedPerformanceClass(
      GetEstimatedPerformanceClassCallback callback) override;

  raw_ptr<FakeOnDeviceServiceSettings> settings_;
  FakeTsHolder ts_holder_;
  mojo::Receiver<mojom::OnDeviceModelService> receiver_;
  mojo::UniqueReceiverSet<mojom::OnDeviceModel> model_receivers_;
};

}  // namespace on_device_model

#endif  // SERVICES_ON_DEVICE_MODEL_PUBLIC_CPP_TEST_SUPPORT_FAKE_SERVICE_H_
