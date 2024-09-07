// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/app/mojo_ipc_support.h"

#include <utility>

#include "base/command_line.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "content/browser/browser_process_io_thread.h"
#include "content/browser/startup_data_impl.h"
#include "content/common/features.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/core/embedder/scoped_ipc_support.h"

namespace content {

MojoIpcSupport::MojoIpcSupport(
    std::unique_ptr<BrowserProcessIOThread> io_thread)
    : io_thread_(std::move(io_thread)) {
  scoped_refptr<base::SingleThreadTaskRunner> mojo_ipc_task_runner =
      io_thread_->task_runner();
  if (base::FeatureList::IsEnabled(features::kMojoDedicatedThread)) {
    mojo_ipc_thread_.StartWithOptions(
        base::Thread::Options(base::MessagePumpType::IO, 0));
    mojo_ipc_task_runner = mojo_ipc_thread_.task_runner();
  }
  mojo_ipc_support_ = std::make_unique<mojo::core::ScopedIPCSupport>(
      mojo_ipc_task_runner, mojo::core::ScopedIPCSupport::ShutdownPolicy::FAST);
}

MojoIpcSupport::~MojoIpcSupport() = default;

std::unique_ptr<StartupDataImpl> MojoIpcSupport::CreateBrowserStartupData() {
  auto startup_data = std::make_unique<StartupDataImpl>();
  startup_data->io_thread = std::move(io_thread_);
  startup_data->mojo_ipc_support = std::move(mojo_ipc_support_);
  return startup_data;
}

}  // namespace content
