// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ON_DEVICE_MODEL_ON_DEVICE_MODEL_SERVICE_H_
#define SERVICES_ON_DEVICE_MODEL_ON_DEVICE_MODEL_SERVICE_H_

#include <set>

#include "base/component_export.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/memory/raw_ptr.h"
#include "base/threading/sequence_bound.h"
#include "base/types/expected.h"
#include "base/uuid.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"
#include "services/on_device_model/backend.h"
#include "services/on_device_model/ml/chrome_ml.h"
#include "services/on_device_model/ml/gpu_blocklist.h"
#include "services/on_device_model/ml/ts_model.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"
#include "services/on_device_model/public/mojom/on_device_model_service.mojom.h"

namespace on_device_model {

class COMPONENT_EXPORT(ON_DEVICE_MODEL) OnDeviceModelService
    : public mojom::OnDeviceModelService {
 public:
  OnDeviceModelService(
      mojo::PendingReceiver<mojom::OnDeviceModelService> receiver,
      const ml::ChromeML& impl);
  OnDeviceModelService(
      mojo::PendingReceiver<mojom::OnDeviceModelService> receiver,
      scoped_refptr<Backend> backend);

  // Creates a service bound to the receiver.
  static std::unique_ptr<mojom::OnDeviceModelService> Create(
      mojo::PendingReceiver<mojom::OnDeviceModelService> receiver,
      scoped_refptr<Backend> backend = nullptr);

  ~OnDeviceModelService() override;

  OnDeviceModelService(const OnDeviceModelService&) = delete;
  OnDeviceModelService& operator=(const OnDeviceModelService&) = delete;

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

  size_t NumModelsForTesting() const { return models_.size(); }

  void SetForceQueueingForTesting(bool force_queueing);

 private:
  void DeleteModel(base::WeakPtr<mojom::OnDeviceModel> model);

  mojo::Receiver<mojom::OnDeviceModelService> receiver_;
  std::set<std::unique_ptr<mojom::OnDeviceModel>, base::UniquePtrComparator>
      models_;
  scoped_refptr<Backend> backend_;
  base::WeakPtrFactory<OnDeviceModelService> weak_factory_{this};
};

}  // namespace on_device_model

#endif  // SERVICES_ON_DEVICE_MODEL_ON_DEVICE_MODEL_SERVICE_H_
