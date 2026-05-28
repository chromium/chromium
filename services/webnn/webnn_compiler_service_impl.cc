// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/webnn_compiler_service_impl.h"

#include "services/webnn/ort/compiler_context_impl_ort.h"

namespace webnn {

WebNNCompilerServiceImpl::WebNNCompilerServiceImpl(
    mojo::PendingReceiver<mojom::WebNNCompilerService> receiver)
    : receiver_(this, std::move(receiver)) {}

WebNNCompilerServiceImpl::~WebNNCompilerServiceImpl() = default;

void WebNNCompilerServiceImpl::CreateCompilerContext(
    mojom::CreateContextOptionsPtr context_options,
    const ContextProperties& context_properties,
    base::flat_map<std::string, mojom::EpPackageInfoPtr> ep_package_info,
    mojo::PendingRemote<mojom::WebNNModelLoader> model_loader,
    mojo::PendingReceiver<mojom::WebNNCompilerContext> receiver) {
  // WebNNCompilerContext instances should be created based on the context
  // options. Currently the compiler service is only used by the ORT backend, so
  // here create CompilerContextImplOrt directly.
  auto context = ort::CompilerContextImplOrt::Create(
      std::move(ep_package_info), std::move(context_options),
      context_properties, std::move(model_loader));
  if (!context) {
    return;
  }
  compiler_contexts_.Add(std::move(context), std::move(receiver));
}

}  // namespace webnn
