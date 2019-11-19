// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_GPU_SERVICE_REGISTRY_H_
#define CONTENT_PUBLIC_BROWSER_GPU_SERVICE_REGISTRY_H_

#include <string>
#include <utility>

#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/system/message_pipe.h"

namespace content {

CONTENT_EXPORT void BindInterfaceInGpuProcess(
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe);

// Bind to an interface exposed by the GPU process.
template <typename Interface>
void BindInterfaceInGpuProcess(mojo::PendingReceiver<Interface> receiver) {
  BindInterfaceInGpuProcess(Interface::Name_, receiver.PassPipe());
}

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_GPU_SERVICE_REGISTRY_H_
