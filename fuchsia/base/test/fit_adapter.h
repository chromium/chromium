// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_BASE_TEST_FIT_ADAPTER_H_
#define FUCHSIA_BASE_TEST_FIT_ADAPTER_H_

#include <lib/fit/function.h>

#include "base/bind.h"
#include "base/callback_helpers.h"

namespace cr_fuchsia {

// Adapts a base::OnceCallback<> to a fit::function<>, to allow //base callbacks
// to be used directly as FIDL result callbacks.
template <typename ReturnType, typename... ArgumentTypes>
fit::function<ReturnType(ArgumentTypes...)> CallbackToFitFunction(
    base::OnceCallback<ReturnType(ArgumentTypes...)> callback) {
  return [callback = std::move(callback)](ArgumentTypes... args) mutable {
    std::move(callback).Run(std::forward<ArgumentTypes>(args)...);
  };
}

}  // namespace cr_fuchsia

#endif  // FUCHSIA_BASE_TEST_FIT_ADAPTER_H_
