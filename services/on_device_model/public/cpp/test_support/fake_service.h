// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef SERVICES_ON_DEVICE_MODEL_PUBLIC_CPP_TEST_SUPPORT_FAKE_SERVICE_H_
#define SERVICES_ON_DEVICE_MODEL_PUBLIC_CPP_TEST_SUPPORT_FAKE_SERVICE_H_

#include <cstdint>
#include <memory>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"
#include "services/on_device_model/public/cpp/model_assets.h"
#include "services/on_device_model/public/cpp/service_client.h"
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

  // The delay before running the GetDeviceAndPerformanceInfo() response
  // callback.
  base::TimeDelta estimated_performance_delay;

  mojom::PerformanceClass performance_class =
      mojom::PerformanceClass::kVeryHigh;

  // If non-empty, used as the output from Execute().
  std::vector<std::string> model_execute_result;

  std::optional<ServiceDisconnectReason> service_disconnect_reason;

  std::optional<ModelDisconnectReason> drop_connection_request;

  // If not-zero, used as the output from GetSizeInTokens().
  uint32_t size_in_tokens = 0;

  void set_execute_delay(base::TimeDelta delay) { execute_delay = delay; }

  void set_estimated_performance_delay(base::TimeDelta delay) {
    estimated_performance_delay = delay;
  }

  void set_execute_result(const std::vector<std::string>& result) {
    model_execute_result = result;
  }

  void set_drop_connection_request(std::optional<ModelDisconnectReason> value) {
    drop_connection_request = value;
  }

  void set_size_in_tokens(uint32_t size) { size_in_tokens = size; }
};

class FakeOnDeviceSession final : public mojom::Session {
 public:
  explicit FakeOnDeviceSession(FakeOnDeviceServiceSettings* settings,
                               FakeOnDeviceModel* model,
                               mojom::SessionParamsPtr params);
  ~FakeOnDeviceSession() override;

  // mojom::Session:
  void Append(mojom::AppendOptionsPtr options,
              mojo::PendingRemote<mojom::ContextClient> client) override;

  void Generate(
      mojom::GenerateOptionsPtr input,
      mojo::PendingRemote<mojom::StreamingResponder> responder) override;

  void GetSizeInTokens(mojom::InputPtr input,
                       GetSizeInTokensCallback callback) override;

  void Score(const std::string& text, ScoreCallback callback) override;
  void GetProbabilitiesBlocking(
      const std::string& text,
      GetProbabilitiesBlockingCallback callback) override;
  void Clone(
      mojo::PendingReceiver<on_device_model::mojom::Session> session) override;
  void SetPriority(mojom::Priority priority) override;
  void AsrStream(
      on_device_model::mojom::AsrStreamOptionsPtr options,
      mojo::PendingReceiver<on_device_model::mojom::AsrStreamInput> stream,
      mojo::PendingRemote<on_device_model::mojom::AsrStreamResponder> responder)
      override;

 private:
  void GenerateImpl(mojom::GenerateOptionsPtr options,
                    mojo::PendingRemote<mojom::StreamingResponder> responder);
  void AppendImpl(mojom::AppendOptionsPtr options,
                  mojo::Remote<mojom::ContextClient> client);
  void CloneImpl(
      mojo::PendingReceiver<on_device_model::mojom::Session> session);
  void AsrStreamImpl(
      on_device_model::mojom::AsrStreamOptionsPtr options,
      mojo::PendingReceiver<on_device_model::mojom::AsrStreamInput> stream,
      mojo::PendingRemote<on_device_model::mojom::AsrStreamResponder>
          responder);

  raw_ptr<FakeOnDeviceServiceSettings> settings_;
  std::string adaptation_model_weight_;
  std::vector<mojom::AppendOptionsPtr> context_;
  raw_ptr<FakeOnDeviceModel> model_;
  mojom::SessionParamsPtr params_;
  on_device_model::mojom::Priority priority_ =
      on_device_model::mojom::Priority::kForeground;

  base::WeakPtrFactory<FakeOnDeviceSession> weak_factory_{this};
};

class FakeOnDeviceModel : public mojom::OnDeviceModel {
 public:
  struct Data {
    Data();
    ~Data();
    Data(const Data&);

    std::string base_weight = "";
    std::string adaptation_model_weight = "";
    std::string cache_weight = "";
    std::string encoder_cache_weight = "";
    std::string adapter_cache_weight = "";
    std::vector<uint32_t> adaptation_ranks;
  };
  explicit FakeOnDeviceModel(FakeOnDeviceServiceSettings* settings,
                             Data&& data,
                             ml::ModelPerformanceHint performance_hint,
                             ml::ModelBackendType backend_type);
  ~FakeOnDeviceModel() override;

  // mojom::OnDeviceModel:
  void StartSession(mojo::PendingReceiver<mojom::Session> session,
                    mojom::SessionParamsPtr params) override;

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

  const Data& data() const { return data_; }

  ml::ModelPerformanceHint performance_hint() const {
    return performance_hint_;
  }

  ml::ModelBackendType backend_type() const { return backend_type_; }

 private:
  raw_ptr<FakeOnDeviceServiceSettings> settings_;
  Data data_;
  ml::ModelPerformanceHint performance_hint_;
  ml::ModelBackendType backend_type_;

  mojo::UniqueReceiverSet<mojom::Session> receivers_;
  mojo::UniqueReceiverSet<mojom::OnDeviceModel> model_adaptation_receivers_;
};

class FakeTsModel final : public mojom::TextSafetyModel,
                          public mojom::TextSafetySession {
 public:
  explicit FakeTsModel(mojom::TextSafetyModelParamsPtr params);
  ~FakeTsModel() override;

  // on_device_model::mojom::TextSafetyModel
  void StartSession(
      mojo::PendingReceiver<mojom::TextSafetySession> session) override;

  // on_device_model::mojom::TextSafetySession
  void ClassifyTextSafety(const std::string& text,
                          ClassifyTextSafetyCallback callback) override;
  void DetectLanguage(const std::string& text,
                      DetectLanguageCallback callback) override;
  void Clone(mojo::PendingReceiver<mojom::TextSafetySession> session) override;

 private:
  bool has_safety_model_ = false;
  bool has_language_model_ = false;
  mojo::ReceiverSet<mojom::TextSafetySession> sessions_;
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
  explicit FakeOnDeviceModelService(FakeOnDeviceServiceSettings* settings);
  ~FakeOnDeviceModelService() override;

  size_t on_device_model_receiver_count() const {
    return model_receivers_.size();
  }

  FakeOnDeviceModel* model() {
    auto contexts = model_receivers_.GetAllContexts();
    if (contexts.size() != 1) {
      return nullptr;
    }
    return *contexts.begin()->second;
  }

 private:
  // mojom::OnDeviceModelService:
  void LoadModel(mojom::LoadModelParamsPtr params,
                 mojo::PendingReceiver<mojom::OnDeviceModel> model,
                 LoadModelCallback callback) override;
  void GetCapabilities(ModelFile model_file,
                       GetCapabilitiesCallback callback) override;
  void LoadTextSafetyModel(
      mojom::TextSafetyModelParamsPtr params,
      mojo::PendingReceiver<mojom::TextSafetyModel> model) override;
  void GetDeviceAndPerformanceInfo(
      GetDeviceAndPerformanceInfoCallback callback) override;

  raw_ptr<FakeOnDeviceServiceSettings> settings_;
  FakeTsHolder ts_holder_;
  mojo::UniqueReceiverSet<mojom::OnDeviceModel, FakeOnDeviceModel*>
      model_receivers_;
};

class FakeServiceLauncher final {
 public:
  explicit FakeServiceLauncher(
      on_device_model::FakeOnDeviceServiceSettings* settings);
  ~FakeServiceLauncher();

  // Provides a launcher for using with ServiceClient.
  auto LaunchFn() {
    return base::BindRepeating(&FakeServiceLauncher::LaunchService,
                               weak_ptr_factory_.GetWeakPtr());
  }

  void clear_did_launch_service() { did_launch_service_ = false; }

  bool did_launch_service() const { return did_launch_service_; }

  bool is_service_running() const { return services_.size() > 0; }

  size_t on_device_model_receiver_count() const {
    size_t total = 0;
    for (const auto& [_, context] : services_.GetAllContexts()) {
      total += (*context)->on_device_model_receiver_count();
    }
    return total;
  }

  FakeOnDeviceModelService* service() {
    auto contexts = services_.GetAllContexts();
    if (contexts.size() != 1) {
      return nullptr;
    }
    return *contexts.begin()->second;
  }

  void CrashService() { services_.Clear(); }

 private:
  void LaunchService(
      mojo::PendingReceiver<on_device_model::mojom::OnDeviceModelService>
          pending_receiver);

  raw_ptr<on_device_model::FakeOnDeviceServiceSettings> settings_;
  mojo::UniqueReceiverSet<mojom::OnDeviceModelService,
                          FakeOnDeviceModelService*>
      services_;
  bool did_launch_service_;
  base::WeakPtrFactory<FakeServiceLauncher> weak_ptr_factory_;
};

}  // namespace on_device_model

#endif  // SERVICES_ON_DEVICE_MODEL_PUBLIC_CPP_TEST_SUPPORT_FAKE_SERVICE_H_
