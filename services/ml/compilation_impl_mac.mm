// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ml/compilation_impl_mac.h"
#include <memory>
#include <utility>

#include "base/command_line.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/ml/compilation_delegate_bnns.h"
#include "services/ml/compilation_delegate_mkl_dnn.h"
#include "services/ml/compilation_delegate_mps.h"
#include "services/ml/ml_switches.h"
#include "services/ml/model_impl_mac.h"

namespace ml {

CompilationImplMac::CompilationImplMac(mojom::ModelInfoPtr model_info)
    : CompilationImpl(std::move(model_info)) {}

CompilationImplMac::~CompilationImplMac() {}

void CompilationImplMac::Finish(int32_t preference, FinishCallback callback) {
  DLOG(INFO) << "CompilationImplMac::Finish";
  DLOG(INFO) << "  "
             << "preference: " << preference;
  preference_ = preference;
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (preference == mojom::PREFER_SUSTAINED_SPEED) {
    if (@available(macOS 10.13, *)) {
      delegate_ = std::make_unique<CompilationDelegateMPS>(this);
    }
  } else if (preference == mojom::PREFER_FAST_SINGLE_ANSWER) {
    if (command_line->HasSwitch(switches::kUseMkldnn)) {
      delegate_ = std::make_unique<CompilationDelegateMklDnn>(this);
    } else {
      if (@available(macOS 10.13, *)) {
        delegate_ = std::make_unique<CompilationDelegateBnns>(this);
      }
    }
  } else {
    LOG(ERROR) << "Preference: " << preference << " is not suppoted.";
  }

  if (!delegate_.get()) {
    std::move(callback).Run(mojom::BAD_DATA);
    return;
  }
  int32_t result = delegate_->Compile();
  std::move(callback).Run(result);
}

}  // namespace ml
