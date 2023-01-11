// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_WEB_COMMON_TEST_FIT_ADAPTER_H_
#define FUCHSIA_WEB_COMMON_TEST_FIT_ADAPTER_H_

#include <lib/fit/function.h>

#include "base/functional/callback.h"

// Adapts a base::OnceCallback<> to a fit::function<>, to allow //base callbacks
// to be used directly as FIDL result callbacks.
template <typename ReturnType, typename... ArgumentTypes>
fit::function<ReturnType(ArgumentTypes...)> CallbackToFitFunction(
    base::OnceCallback<ReturnType(ArgumentTypes...)> callback) {
  return [callback = std::move(callback)](ArgumentTypes... args) mutable {
    std::move(callback).Run(std::forward<ArgumentTypes>(args)...);
  };
}

#endif  // FUCHSIA_WEB_COMMON_TEST_FIT_ADAPTER_H_
