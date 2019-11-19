// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_GPU_SHELL_CONTENT_GPU_CLIENT_H_
#define CONTENT_SHELL_GPU_SHELL_CONTENT_GPU_CLIENT_H_

#include <memory>

#include "base/macros.h"
#include "content/public/gpu/content_gpu_client.h"
#include "services/network/public/mojom/network_service_test.mojom.h"

namespace content {

class ShellContentGpuClient : public ContentGpuClient {
 public:
  ShellContentGpuClient();
  ~ShellContentGpuClient() override;

  // ContentGpuClient:
  void ExposeInterfacesToBrowser(const gpu::GpuPreferences& gpu_preferences,
                                 mojo::BinderMap* binders) override;

  DISALLOW_COPY_AND_ASSIGN(ShellContentGpuClient);
};

}  // namespace content

#endif  // CONTENT_SHELL_GPU_SHELL_CONTENT_GPU_CLIENT_H_
