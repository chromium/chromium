// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/tflite/context_provider_tflite.h"

#include <utility>

#include "base/check.h"
#include "base/task/thread_pool.h"
#include "services/webnn/error.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom.h"
#include "services/webnn/public/mojom/webnn_error.mojom.h"
#include "services/webnn/tflite/context_impl_tflite.h"
#include "services/webnn/tflite/graph_builder_tflite.h"

namespace webnn::tflite {

ContextProviderTflite::ContextProviderTflite(
    CreateWeightsFileCallback create_weights_file_callback,
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner)
    : create_weights_file_callback_(std::move(create_weights_file_callback)),
      main_task_runner_(std::move(main_task_runner)) {}
ContextProviderTflite::~ContextProviderTflite() = default;

void ContextProviderTflite::CreateWebNNContext(
    mojom::CreateContextOptionsPtr options,
    CreateWebNNContextCallback callback) {
  std::move(callback).Run(ToError<mojom::CreateContextResult>(
      mojom::Error::Code::kNotSupportedError,
      "TFLite backend in this process is not supported."));
}

void ContextProviderTflite::CreateWeightsFile(
    base::OnceCallback<void(base::File)> callback) {
  create_weights_file_callback_.Run(std::move(callback));
}

void ContextProviderTflite::RemoveWebNNContextImpl(
    const blink::WebNNContextToken& handle) {
  auto it = context_impls_.find(handle);
  CHECK(it != context_impls_.end());
  context_impls_.erase(it);
}

}  // namespace webnn::tflite
