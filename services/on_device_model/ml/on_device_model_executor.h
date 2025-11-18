// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ON_DEVICE_MODEL_ML_ON_DEVICE_MODEL_EXECUTOR_H_
#define SERVICES_ON_DEVICE_MODEL_ML_ON_DEVICE_MODEL_EXECUTOR_H_

#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

#include "base/component_export.h"
#include "base/files/file_path.h"
#include "base/files/memory_mapped_file.h"
#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/native_library.h"
#include "base/types/expected.h"
#include "base/types/pass_key.h"
#include "services/on_device_model/backend.h"
#include "services/on_device_model/backend_model.h"
#include "services/on_device_model/backend_session.h"
#include "services/on_device_model/ml/chrome_ml.h"
#include "services/on_device_model/ml/session_accessor.h"
#include "services/on_device_model/ml/ts_model.h"
#include "services/on_device_model/public/cpp/model_assets.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"
#include "services/on_device_model/public/mojom/on_device_model_service.mojom.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

struct LlgTokenizer;

namespace ml {

class ContextHolder;
class OnDeviceModelExecutor;
class Responder;
class AsrStreamResponder;

class COMPONENT_EXPORT(ON_DEVICE_MODEL_ML) BackendImpl final
    : public on_device_model::Backend {
 public:
  explicit BackendImpl(const ml::ChromeML* chrome_ml);

  // on_device_model::Backend:
  base::expected<void, on_device_model::ServiceDisconnectReason> CanCreate()
      override;
  on_device_model::Capabilities GetCapabilities(
      on_device_model::ModelFile model_file) override;
  base::expected<std::unique_ptr<on_device_model::BackendModel>,
                 on_device_model::mojom::LoadModelResult>
  CreateWithResult(on_device_model::mojom::LoadModelParamsPtr params,
                   base::OnceClosure on_complete) override;
  void LoadTextSafetyModel(
      on_device_model::mojom::TextSafetyModelParamsPtr params,
      mojo::PendingReceiver<on_device_model::mojom::TextSafetyModel> model)
      override;
  std::pair<on_device_model::mojom::DevicePerformanceInfoPtr,
            on_device_model::mojom::DeviceInfoPtr>
  GetDeviceAndPerformanceInfo() override;

 protected:
  ~BackendImpl() override;

 private:
  const raw_ptr<const ml::ChromeML> chrome_ml_;
  base::SequenceBound<ml::TsHolder> ts_holder_;
};

class COMPONENT_EXPORT(ON_DEVICE_MODEL_ML) SessionImpl final
    : public on_device_model::BackendSession {
 public:
  SessionImpl(const ChromeML& chrome_ml,
              OnDeviceModelExecutor& executor,
              SessionAccessor::Ptr session,
              uint32_t max_tokens,
              std::optional<uint32_t> adaptation_id);
  ~SessionImpl() override;

  SessionImpl(const SessionImpl&) = delete;
  SessionImpl& operator=(const SessionImpl&) = delete;

  // on_device_model::BackendSession:
  void Append(on_device_model::mojom::AppendOptionsPtr options,
              mojo::PendingRemote<on_device_model::mojom::ContextClient> client,
              base::OnceClosure on_complete) override;
  void Generate(
      on_device_model::mojom::GenerateOptionsPtr input,
      mojo::PendingRemote<on_device_model::mojom::StreamingResponder> response,
      base::OnceClosure on_complete) override;
  void SizeInTokens(on_device_model::mojom::InputPtr input,
                    base::OnceCallback<void(uint32_t)> callback) override;
  void Score(const std::string& text,
             base::OnceCallback<void(float)> callback) override;
  void GetProbabilitiesBlocking(
      const std::string& input,
      base::OnceCallback<void(const std::vector<float>&)> callback) override;
  std::unique_ptr<BackendSession> Clone() override;
  void AsrStream(on_device_model::mojom::AsrStreamOptionsPtr options,
                 mojo::PendingRemote<on_device_model::mojom::AsrStreamResponder>
                     response) override;
  void AsrAddAudioChunk(on_device_model::mojom::AudioDataPtr data) override;

 private:
  void RemoveContext(ContextHolder* context);

  const raw_ref<const ChromeML> chrome_ml_;
  const raw_ref<OnDeviceModelExecutor> executor_;
  SessionAccessor::Ptr session_;
  const uint32_t max_tokens_;
  std::unique_ptr<Responder> responder_;
  std::unique_ptr<AsrStreamResponder> asr_responder_;
  std::set<std::unique_ptr<ContextHolder>> context_holders_;
  std::optional<uint32_t> adaptation_id_;
  std::optional<std::string> model_response_prefix_;
};

// Uses the ChromeML API to create a model based on the params passed to
// |Create()|. This is the main interface for interacting with the model.
class COMPONENT_EXPORT(ON_DEVICE_MODEL_ML) OnDeviceModelExecutor final
    : public on_device_model::BackendModel {
 public:

  explicit OnDeviceModelExecutor(base::PassKey<OnDeviceModelExecutor>,
                                 const ChromeML& chrome_ml);
  ~OnDeviceModelExecutor() override;

  static base::expected<std::unique_ptr<OnDeviceModelExecutor>,
                        on_device_model::mojom::LoadModelResult>
  CreateWithResult(const ChromeML& chrome_ml,
                   on_device_model::mojom::LoadModelParamsPtr params,
                   base::OnceClosure on_complete);

  // on_device_model::BackendModel:
  std::unique_ptr<on_device_model::BackendSession> CreateSession(
      const ScopedAdaptation* adaptation,
      on_device_model::mojom::SessionParamsPtr params) override;
  std::unique_ptr<ScopedAdaptation> LoadAdaptation(
      on_device_model::mojom::LoadAdaptationParamsPtr params) override;
  void UnloadAdaptation(uint32_t adaptation_id) override;

  ChromeMLConstraint CreateConstraint(
      const on_device_model::mojom::ResponseConstraint& response_constraint,
      const std::optional<std::string>& prefix);

 private:
  on_device_model::mojom::LoadModelResult Init(
      on_device_model::mojom::LoadModelParamsPtr params,
      base::OnceClosure on_complete);

  static void Schedule(uintptr_t context, std::function<void()>* fn);

  const raw_ref<const ChromeML> chrome_ml_;

  // Params for adaptations that have been loaded.
  absl::flat_hash_map<uint32_t, on_device_model::mojom::LoadAdaptationParamsPtr>
      adaptation_params_;

  ChromeMLModel model_ = 0;
  scoped_refptr<base::SequencedTaskRunner> model_task_runner_;
  uint32_t max_tokens_ = 0;
  uint32_t next_adaptation_id_ = 0;
  // Disabling dangling pointer detection because this uses C functions to
  // allocate/free from rust.
  raw_ptr<LlgTokenizer, DisableDanglingPtrDetection> tokenizer_ = nullptr;
  base::WeakPtrFactory<OnDeviceModelExecutor> weak_ptr_factory_{this};
};

}  // namespace ml

#endif  // SERVICES_ON_DEVICE_MODEL_ML_ON_DEVICE_MODEL_EXECUTOR_H_
