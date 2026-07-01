// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/on_device_model/safety/safety_model_holder.h"

#include <utility>

#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequence_bound.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"
#include "services/on_device_model/public/mojom/on_device_model_service.mojom.h"
#include "services/on_device_model/safety/bert_safety_model.h"

namespace on_device_model {

SafetyModelHolder::SafetyModelHolder() = default;
SafetyModelHolder::~SafetyModelHolder() = default;

// static
base::SequenceBound<SafetyModelHolder> SafetyModelHolder::Create() {
  return base::SequenceBound<SafetyModelHolder>(
      base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()}));
}

void SafetyModelHolder::Reset(
    mojom::TextSafetyModelParamsPtr params,
    mojo::PendingReceiver<mojom::TextSafetyModel> model) {
  model_.Clear();

  auto impl = BertSafetyModel::Create(std::move(params));
  if (impl) {
    model_.Add(std::move(impl), std::move(model));
  }
}

}  // namespace on_device_model
