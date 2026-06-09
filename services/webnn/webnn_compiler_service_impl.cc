// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/webnn_compiler_service_impl.h"

#include "base/functional/bind.h"
#include "services/webnn/ort/compiler_context_impl_ort.h"
#include "services/webnn/public/cpp/compiler_disconnect_reason.h"

namespace webnn {

namespace {

// How long to wait after the last compiler context disconnects before
// shutting down the compiler process.
constexpr base::TimeDelta kIdleTimeout = base::Seconds(30);

}  // namespace

WebNNCompilerServiceImpl::WebNNCompilerServiceImpl(
    mojo::PendingReceiver<mojom::WebNNCompilerService> receiver)
    : receiver_(this, std::move(receiver)) {
  compiler_contexts_.set_disconnect_handler(base::BindRepeating(
      &WebNNCompilerServiceImpl::OnCompilerContextDisconnected,
      base::Unretained(this)));

  // Start the idle timer immediately as a safety net. Currently the process
  // is only launched when a context is requested (lazy launch in
  // GpuProcessHost::RequestWebNNCompilerContext), so CreateCompilerContext()
  // will cancel this timer almost immediately. But if the launch path ever
  // changes, this ensures the process won't linger indefinitely.
  idle_timer_.Start(FROM_HERE, kIdleTimeout,
                    base::BindOnce(&WebNNCompilerServiceImpl::OnIdleTimeout,
                                   base::Unretained(this)));
}

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
  // A new context is being added — cancel any pending idle shutdown.
  idle_timer_.Stop();
  compiler_contexts_.Add(std::move(context), std::move(receiver));
}

void WebNNCompilerServiceImpl::OnCompilerContextDisconnected() {
  if (compiler_contexts_.empty()) {
    idle_timer_.Start(FROM_HERE, kIdleTimeout,
                      base::BindOnce(&WebNNCompilerServiceImpl::OnIdleTimeout,
                                     base::Unretained(this)));
  }
}

void WebNNCompilerServiceImpl::OnIdleTimeout() {
  // Re-check in case a new context was added between the timer firing and
  // this callback running.
  if (!compiler_contexts_.empty()) {
    return;
  }
  receiver_.ResetWithReason(
      static_cast<uint32_t>(CompilerDisconnectReason::kIdleShutdown),
      "Idle shutdown: no active compiler contexts");
}

}  // namespace webnn
