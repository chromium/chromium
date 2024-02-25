// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_COREML_CONTEXT_IMPL_H_
#define SERVICES_WEBNN_COREML_CONTEXT_IMPL_H_

#include "services/webnn/webnn_context_impl.h"

namespace webnn::coreml {

// `ContextImpl` is created by `WebNNContextProviderImpl` and responsible for
// creating a `GraphImpl` for the CoreML backend on macOS.
// Mac OS 13.0+ is required for model compilation
// https://developer.apple.com/documentation/coreml/mlmodel/3931182-compilemodel
class API_AVAILABLE(macos(13.0)) ContextImpl final : public WebNNContextImpl {
 public:
  ContextImpl(mojo::PendingReceiver<mojom::WebNNContext> receiver,
              WebNNContextProviderImpl* context_provider);

  ContextImpl(const WebNNContextImpl&) = delete;
  ContextImpl& operator=(const ContextImpl&) = delete;

  ~ContextImpl() override;

 private:
  void CreateGraphImpl(mojom::GraphInfoPtr graph_info,
                       CreateGraphCallback callback) override;
};

}  // namespace webnn::coreml

#endif  // SERVICES_WEBNN_DML_CONTEXT_IMPL_H_
