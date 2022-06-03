// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/ppapi_migration/callback.h"

#include <stdint.h>

#include <memory>
#include <utility>

#include "base/callback.h"
#include "ppapi/cpp/completion_callback.h"

namespace chrome_pdf {

namespace {

void RunAndDeleteResultCallback(void* user_data, int32_t result) {
  std::unique_ptr<ResultCallback> callback(
      static_cast<ResultCallback*>(user_data));
  std::move(*callback).Run(result);
}

}  // namespace

pp::CompletionCallback PPCompletionCallbackFromResultCallback(
    ResultCallback callback) {
  return pp::CompletionCallback(RunAndDeleteResultCallback,
                                new ResultCallback(std::move(callback)));
}

}  // namespace chrome_pdf
