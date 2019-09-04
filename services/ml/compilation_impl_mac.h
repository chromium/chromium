// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ML_COMPILATION_IMPL_MAC_H_
#define SERVICES_ML_COMPILATION_IMPL_MAC_H_

#include <vector>

#include "services/ml/compilation_impl.h"

namespace ml {
class CompilationImplMac : public CompilationImpl {
 public:
  explicit CompilationImplMac(mojom::ModelInfoPtr model_info);
  ~CompilationImplMac() override;

  void Finish(int32_t preference, FinishCallback callback) override;

  DISALLOW_COPY_AND_ASSIGN(CompilationImplMac);
};

}  // namespace ml

#endif  // SERVICES_ML_COMPILATION_IMPL_MAC_H_