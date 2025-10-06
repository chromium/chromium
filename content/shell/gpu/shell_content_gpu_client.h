// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_GPU_SHELL_CONTENT_GPU_CLIENT_H_
#define CONTENT_SHELL_GPU_SHELL_CONTENT_GPU_CLIENT_H_

#include "content/public/gpu/content_gpu_client.h"

namespace content {

class ShellContentGpuClient : public ContentGpuClient {
 public:
  ShellContentGpuClient();

  ShellContentGpuClient(const ShellContentGpuClient&) = delete;
  ShellContentGpuClient& operator=(const ShellContentGpuClient&) = delete;

  ~ShellContentGpuClient() override;

  // ContentGpuClient:
  void ExposeInterfacesToBrowser(
      viz::GpuServiceImpl* gpu_service,
      const gpu::GpuPreferences& gpu_preferences,
      const gpu::GpuDriverBugWorkarounds& gpu_workarounds,
      mojo::BinderMap* binders) override;
};

}  // namespace content

#endif  // CONTENT_SHELL_GPU_SHELL_CONTENT_GPU_CLIENT_H_
