// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/coreml/context_impl_coreml.h"

#import <CoreML/CoreML.h>

#include "services/webnn/coreml/graph_impl_coreml.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom.h"
#include "services/webnn/webnn_context_provider_impl.h"

namespace webnn::coreml {

ContextImplCoreml::ContextImplCoreml(
    mojo::PendingReceiver<mojom::WebNNContext> receiver,
    WebNNContextProviderImpl* context_provider,
    mojom::CreateContextOptionsPtr options)
    : WebNNContextImpl(std::move(receiver),
                       context_provider,
                       GraphBuilderCoreml::GetContextProperties()),
      options_(std::move(options)) {}

ContextImplCoreml::~ContextImplCoreml() = default;

void ContextImplCoreml::CreateGraphImpl(
    mojom::GraphInfoPtr graph_info,
    mojom::WebNNContext::CreateGraphCallback callback) {
  GraphImplCoreml::CreateAndBuild(std::move(graph_info), options_.Clone(),
                                  std::move(callback));
}

std::unique_ptr<WebNNBufferImpl> ContextImplCoreml::CreateBufferImpl(
    mojo::PendingAssociatedReceiver<mojom::WebNNBuffer> receiver,
    mojom::BufferInfoPtr buffer_info,
    const base::UnguessableToken& buffer_handle) {
  // TODO(crbug.com/40278771): Implement MLBuffer for CoreML. Involve
  // an IPC security reviewer.
  NOTIMPLEMENTED();
  return {};
}

}  // namespace webnn::coreml
