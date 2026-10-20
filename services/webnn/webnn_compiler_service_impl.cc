// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/webnn_compiler_service_impl.h"

#include "base/command_line.h"
#include "build/build_config.h"
#include "services/webnn/webnn_switches.h"

#if BUILDFLAG(IS_WIN)
#include "services/webnn/ort/compiler_context_impl_ort.h"
#include "services/webnn/public/cpp/ep_device_info.h"
#elif BUILDFLAG(IS_APPLE)
#include "services/webnn/coreml/compiler_context_impl_coreml.h"
#endif

namespace webnn {

WebNNCompilerServiceImpl::WebNNCompilerServiceImpl(
    mojo::PendingReceiver<mojom::WebNNCompilerService> receiver)
#if BUILDFLAG(IS_WIN)
    // The switch is already parsed and validated in PreSandboxInit() so it is
    // guaranteed to be valid.
    : target_device_(EpDeviceInfo::FromSwitchValue(
          base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
              switches::kWebNNCompilerEpDeviceInfo))),
      receiver_(this, std::move(receiver))
#else
    : receiver_(this, std::move(receiver))
#endif
{
}

WebNNCompilerServiceImpl::~WebNNCompilerServiceImpl() = default;

void WebNNCompilerServiceImpl::CreateCompilerContext(
    mojom::CreateContextOptionsPtr context_options,
    const ContextProperties& context_properties,
    mojo::PendingRemote<mojom::WebNNModelLoader> model_loader,
    mojo::PendingReceiver<mojom::WebNNCompilerContext> receiver,
    CreateCompilerContextCallback callback) {
#if BUILDFLAG(IS_WIN)
  auto compiler_context = std::make_unique<ort::CompilerContextImplOrt>(
      *this, target_device_, std::move(context_options), context_properties,
      std::move(model_loader));
  ort::CompilerContextImplOrt* compiler_context_ptr = compiler_context.get();
  mojo::ReceiverId receiver_id =
      compiler_contexts_.Add(std::move(compiler_context), std::move(receiver));

  // Bind the context's lifetime to the ModelLoader pipe.
  compiler_context_ptr->SetId(receiver_id,
                              base::PassKey<WebNNCompilerServiceImpl>());
  std::move(callback).Run(true);
#elif BUILDFLAG(IS_APPLE)
  if (__builtin_available(macOS 14.4, *)) {
    auto compiler_context = std::make_unique<coreml::CompilerContextImplCoreml>(
        *this, std::move(context_options), context_properties,
        std::move(model_loader));
    coreml::CompilerContextImplCoreml* compiler_context_ptr =
        compiler_context.get();
    mojo::ReceiverId receiver_id = compiler_contexts_.Add(
        std::move(compiler_context), std::move(receiver));

    // Bind the context's lifetime to the ModelLoader pipe.
    compiler_context_ptr->SetId(receiver_id,
                                base::PassKey<WebNNCompilerServiceImpl>());
    std::move(callback).Run(true);
    return;
  }
  std::move(callback).Run(false);
#endif
}

#if BUILDFLAG(IS_WIN)
void WebNNCompilerServiceImpl::RemoveCompilerContext(
    mojo::ReceiverId receiver_id,
    base::PassKey<ort::CompilerContextImplOrt> /*pass_key*/) {
  compiler_contexts_.Remove(receiver_id);
}
#elif BUILDFLAG(IS_APPLE)
void API_AVAILABLE(macos(14.4)) WebNNCompilerServiceImpl::RemoveCompilerContext(
    mojo::ReceiverId receiver_id,
    base::PassKey<coreml::CompilerContextImplCoreml> /*pass_key*/) {
  compiler_contexts_.Remove(receiver_id);
}
#endif

}  // namespace webnn
