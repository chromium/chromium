// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ML_MODEL_IMPL_MAC_H_
#define SERVICES_ML_MODEL_IMPL_MAC_H_

#include "base/macros.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/ml/model_impl.h"

namespace ml {

class ModelImplMac : public ModelImpl {
 public:
  ModelImplMac();
  ~ModelImplMac() override;

  void CreateCompilation(CreateCompilationCallback callback) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ModelImplMac);
};

}  // namespace ml

#endif  // SERVICES_ML_MODEL_IMPL_MAC_H_