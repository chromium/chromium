// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ON_DEVICE_MODEL_ML_ON_DEVICE_MODEL_EXECUTOR_H_
#define SERVICES_ON_DEVICE_MODEL_ML_ON_DEVICE_MODEL_EXECUTOR_H_

#include <cstdint>
#include <functional>
#include <memory>

#include "base/component_export.h"
#include "base/files/file_path.h"
#include "base/files/memory_mapped_file.h"
#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/native_library.h"
#include "base/types/expected.h"
#include "base/types/pass_key.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/on_device_model/ml/chrome_ml.h"
#include "services/on_device_model/ml/session_accessor.h"
#include "services/on_device_model/ml/ts_model.h"
#include "services/on_device_model/public/cpp/model_assets.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"
#include "services/on_device_model/public/mojom/on_device_model_service.mojom.h"

namespace ml {

class ContextHolder;
class Responder;

class COMPONENT_EXPORT(ON_DEVICE_MODEL_ML) SessionImpl final {
 public:
  SessionImpl(const ChromeML& chrome_ml,
              ChromeMLModel model,
              SessionAccessor::Ptr session,
              SessionAccessor::Ptr empty_session,
              uint32_t max_tokens,
              std::optional<uint32_t> adaptation_id);
  ~SessionImpl();

  SessionImpl(const SessionImpl&) = delete;
  SessionImpl& operator=(const SessionImpl&) = delete;

  void AddContext(
      on_device_model::mojom::InputOptionsPtr input,
      mojo::PendingRemote<on_device_model::mojom::ContextClient> client,
      base::OnceClosure on_complete);
  void Execute(
      on_device_model::mojom::InputOptionsPtr input,
      mojo::PendingRemote<on_device_model::mojom::StreamingResponder> response,
      base::OnceClosure on_complete);
  void SizeInTokens(on_device_model::mojom::InputPtr input,
                    base::OnceCallback<void(uint32_t)> callback);
  void Score(const std::string& text, base::OnceCallback<void(float)> callback);
  std::unique_ptr<SessionImpl> Clone();

 private:
  void RemoveContext(ContextHolder* context);

  const raw_ref<const ChromeML> chrome_ml_;
  ChromeMLModel model_;
  SessionAccessor::Ptr session_;
  SessionAccessor::Ptr empty_session_;
  const uint32_t max_tokens_;
  std::unique_ptr<Responder> responder_;
  std::set<std::unique_ptr<ContextHolder>> context_holders_;
  std::optional<uint32_t> adaptation_id_;
};

// Uses the ChromeML API to create a model based on the params passed to
// |Create()|. This is the main interface for interacting with the model.
class COMPONENT_EXPORT(ON_DEVICE_MODEL_ML) OnDeviceModelExecutor final {
 public:
  explicit OnDeviceModelExecutor(base::PassKey<OnDeviceModelExecutor>,
                                 const ChromeML& chrome_ml);
  ~OnDeviceModelExecutor();

  static base::expected<std::unique_ptr<OnDeviceModelExecutor>,
                        on_device_model::mojom::LoadModelResult>
  CreateWithResult(const ChromeML& chrome_ml,
                   on_device_model::mojom::LoadModelParamsPtr params,
                   base::OnceClosure on_complete);

  // on_device_model::OnDeviceModel:
  std::unique_ptr<SessionImpl> CreateSession(
      std::optional<uint32_t> adaptation_id);
  base::expected<uint32_t, on_device_model::mojom::LoadModelResult>
  LoadAdaptation(on_device_model::mojom::LoadAdaptationParamsPtr params,
                 base::OnceClosure on_complete);

 private:
  on_device_model::mojom::LoadModelResult Init(
      on_device_model::mojom::LoadModelParamsPtr params,
      base::OnceClosure on_complete);

  static void Schedule(uintptr_t context, std::function<void()>* fn);

  const raw_ref<const ChromeML> chrome_ml_;

  // TODO(b/323572952): Allow disposing of adaptation weights.
  std::vector<std::unique_ptr<base::MemoryMappedFile>> adaptation_data_;

  // Empty sessions keyed by the adaptation ID that can be cloned from.
  std::map<std::optional<uint32_t>, SessionAccessor::Ptr> base_sessions_;

  ChromeMLModel model_ = 0;
  scoped_refptr<base::SequencedTaskRunner> model_task_runner_;
  uint32_t max_tokens_ = 0;
};

}  // namespace ml

#endif  // SERVICES_ON_DEVICE_MODEL_ML_ON_DEVICE_MODEL_EXECUTOR_H_
