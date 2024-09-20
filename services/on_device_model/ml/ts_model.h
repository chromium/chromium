// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ON_DEVICE_MODEL_ML_TS_MODEL_H_
#define SERVICES_ON_DEVICE_MODEL_ML_TS_MODEL_H_

#include "base/files/memory_mapped_file.h"
#include "base/memory/raw_ref.h"
#include "base/threading/sequence_bound.h"
#include "components/translate/core/language_detection/language_detection_model.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"
#include "services/on_device_model/ml/chrome_ml.h"
#include "services/on_device_model/ml/chrome_ml_api.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"
#include "services/on_device_model/public/mojom/on_device_model_service.mojom.h"

namespace ml {

// TsHolder holds a single TsModel. Its operations may block.
class COMPONENT_EXPORT(ON_DEVICE_MODEL_ML) TsHolder final {
 public:
  // Note: Uses raw_ref arg so that Bind does not try to copy/move ChromeML.
  explicit TsHolder(raw_ref<const ChromeML> chrome_ml);
  ~TsHolder();

  static base::SequenceBound<TsHolder> Create(
      raw_ref<const ChromeML> chrome_ml);

  void Reset(
      on_device_model::mojom::TextSafetyModelParamsPtr params,
      mojo::PendingReceiver<on_device_model::mojom::TextSafetyModel> model);

 private:
  const raw_ref<const ChromeML> chrome_ml_;

  // A connected model, once we've received assets.
  mojo::UniqueReceiverSet<on_device_model::mojom::TextSafetyModel> model_;
};

}  // namespace ml

#endif  // SERVICES_ON_DEVICE_MODEL_ML_TS_MODEL_H_
