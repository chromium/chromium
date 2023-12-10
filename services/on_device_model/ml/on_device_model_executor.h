// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ON_DEVICE_MODEL_ML_ON_DEVICE_MODEL_EXECUTOR_H_
#define SERVICES_ON_DEVICE_MODEL_ML_ON_DEVICE_MODEL_EXECUTOR_H_

#include "base/files/file_path.h"
#include "base/files/memory_mapped_file.h"
#include "base/memory/raw_ref.h"
#include "base/native_library.h"
#include "base/types/expected.h"
#include "base/types/pass_key.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/on_device_model/ml/chrome_ml.h"
#include "services/on_device_model/public/cpp/model_assets.h"
#include "services/on_device_model/public/cpp/on_device_model.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"
#include "services/on_device_model/public/mojom/on_device_model_service.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ml {

// Uses the ChromeML API to create a model based on the params passed to
// |Create()|. This is the main interface for interacting with the model.
class OnDeviceModelExecutor
    : public on_device_model::OnDeviceModel,
      public base::SupportsWeakPtr<OnDeviceModelExecutor> {
 public:
  explicit OnDeviceModelExecutor(base::PassKey<OnDeviceModelExecutor>,
                                 const ChromeML& chrome_ml);
  ~OnDeviceModelExecutor() override;

  static base::expected<std::unique_ptr<OnDeviceModelExecutor>,
                        on_device_model::mojom::LoadModelResult>
  CreateWithResult(const ChromeML& chrome_ml,
                   on_device_model::mojom::LoadModelParamsPtr params);

  // on_device_model::OnDeviceModel:
  std::unique_ptr<Session> CreateSession() override;

 private:
  on_device_model::mojom::LoadModelResult Init(
      on_device_model::mojom::LoadModelParamsPtr params);

  void DisposeSentencepiece();
  void DisposeModelProto();
  void DisposeWeights();

  static void Schedule(uintptr_t context, std::function<void()>* fn);

  const raw_ref<const ChromeML> chrome_ml_;

  std::unique_ptr<base::MemoryMappedFile> sentencepiece_model_proto_;
  std::unique_ptr<base::MemoryMappedFile> model_proto_;
  std::unique_ptr<base::MemoryMappedFile> weights_;
  base::MemoryMappedFile ts_data_;
  base::MemoryMappedFile ts_sp_model_;

  ChromeMLModel model_ = 0;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
};

}  // namespace ml

#endif  // SERVICES_ON_DEVICE_MODEL_ML_ON_DEVICE_MODEL_EXECUTOR_H_
