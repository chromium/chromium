// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/webnn_compiler_service_impl.h"

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "services/webnn/ort/compiler_context_impl_ort.h"
#include "services/webnn/public/cpp/compiler_disconnect_reason.h"
#include "services/webnn/public/cpp/ep_device_info.h"
#include "services/webnn/webnn_switches.h"

namespace webnn {

namespace {

// How long to wait after the last compiler context disconnects before
// shutting down the compiler process.
constexpr base::TimeDelta kIdleTimeout = base::Seconds(30);

}  // namespace

WebNNCompilerServiceImpl::WebNNCompilerServiceImpl(
    mojo::PendingReceiver<mojom::WebNNCompilerService> receiver)
    // The switch is already parsed and validated in PreSandboxInit() so it is
    // guaranteed to be valid.
    : target_device_(EpDeviceInfo::FromSwitchValue(
          base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
              switches::kWebNNCompilerEpDeviceInfo))),
      receiver_(this, std::move(receiver)) {
  compiler_contexts_.set_disconnect_handler(base::BindRepeating(
      &WebNNCompilerServiceImpl::OnCompilerContextDisconnected,
      base::Unretained(this)));

  // Start the idle timer immediately as a safety net. Currently the process
  // is only launched when a context is requested (lazy launch in
  // WebNNBrowserHostImpl::RequestCompilerContext), so CreateCompilerContext()
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
    mojo::PendingRemote<mojom::WebNNModelLoader> model_loader,
    mojo::PendingReceiver<mojom::WebNNCompilerContext> receiver,
    CreateCompilerContextCallback callback) {
  // A new context is being added — cancel any pending idle shutdown.
  idle_timer_.Stop();
  // WebNNCompilerContext instances should be created based on the context
  // options. Currently the compiler service is only used by the ORT backend, so
  // here create CompilerContextImplOrt directly.
  compiler_contexts_.Add(std::make_unique<ort::CompilerContextImplOrt>(
                             target_device_, std::move(context_options),
                             context_properties, std::move(model_loader)),
                         std::move(receiver));
  std::move(callback).Run(true);
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
      "No active compiler contexts");
}

}  // namespace webnn
