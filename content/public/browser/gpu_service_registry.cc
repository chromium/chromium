// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/gpu_service_registry.h"

#include "components/viz/host/gpu_host_impl.h"
#include "content/browser/gpu/gpu_process_host.h"

namespace content {

void BindInterfaceInGpuProcess(const std::string& interface_name,
                               mojo::ScopedMessagePipeHandle interface_pipe) {
  auto* host = GpuProcessHost::Get()->gpu_host();
  return host->BindInterface(interface_name, std::move(interface_pipe));
}

}  // namespace content
