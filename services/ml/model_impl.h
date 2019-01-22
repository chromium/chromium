// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ML_MODEL_IMPL_H_
#define SERVICES_ML_MODEL_IMPL_H_

#include "base/macros.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/ml/common.h"
#include "services/ml/public/mojom/constants.mojom.h"
#include "services/ml/public/mojom/model.mojom.h"

namespace ml {

class ModelImpl : public mojom::Model {
 public:
  ModelImpl();
  ~ModelImpl() override;

  void Finish(mojom::ModelInfoPtr model_info, FinishCallback callback) override;

  void CreateCompilation(CreateCompilationCallback callback) override;

 private:
  friend class CompilationImpl;
  mojom::ModelInfoPtr model_info_;
  DISALLOW_COPY_AND_ASSIGN(ModelImpl);
};

}  // namespace ml

#endif  // SERVICES_ML_MODEL_IMPL_MAC_H_
