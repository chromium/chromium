// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/gpu/shell_content_gpu_client.h"

#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "content/shell/common/power_monitor_test_impl.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace content {

ShellContentGpuClient::ShellContentGpuClient() = default;

ShellContentGpuClient::~ShellContentGpuClient() = default;

void ShellContentGpuClient::ExposeInterfacesToBrowser(
    const gpu::GpuPreferences& gpu_preferences,
    const gpu::GpuDriverBugWorkarounds& gpu_workarounds,
    mojo::BinderMap* binders) {
  binders->Add<mojom::PowerMonitorTest>(
      base::BindRepeating(&PowerMonitorTestImpl::MakeSelfOwnedReceiver),
      base::SingleThreadTaskRunner::GetCurrentDefault());
}

}  // namespace content
