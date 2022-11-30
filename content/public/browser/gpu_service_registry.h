// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_GPU_SERVICE_REGISTRY_H_
#define CONTENT_PUBLIC_BROWSER_GPU_SERVICE_REGISTRY_H_

#include <string>
#include <utility>

#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "sandbox/policy/mojom/sandbox.mojom.h"

namespace content {

CONTENT_EXPORT void BindInterfaceInGpuProcess(
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe);

// Bind to an interface exposed by the GPU process. The mojo interface must be
// marked with a [ServiceSandbox=sandbox.mojom.Sandbox.kGpu] attribute.
template <typename Interface>
void BindInterfaceInGpuProcess(mojo::PendingReceiver<Interface> receiver) {
  using ProvidedSandboxType = decltype(Interface::kServiceSandbox);
  static_assert(
      std::is_same<ProvidedSandboxType, const sandbox::mojom::Sandbox>::value,
      "This interface does not declare a proper ServiceSandbox attribute. "
      "See //docs/mojo_and_services.md (Specifying a sandbox).");
  static_assert(Interface::kServiceSandbox == sandbox::mojom::Sandbox::kGpu,
                "This interface must have [ServiceSandbox=kGpu].");

  BindInterfaceInGpuProcess(Interface::Name_, receiver.PassPipe());
}

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_GPU_SERVICE_REGISTRY_H_
