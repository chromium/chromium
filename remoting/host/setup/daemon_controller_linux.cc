// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/setup/daemon_controller.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "remoting/host/linux/host_types.h"
#include "remoting/host/setup/daemon_controller_delegate_linux_multi_process.h"
#include "remoting/host/setup/daemon_controller_delegate_linux_single_process.h"

namespace remoting {

namespace {

const HostType* g_host_type = nullptr;

std::unique_ptr<DaemonController::Delegate> GetDelegateForHostType(
    const HostType& host_type) {
  if (host_type.is_multi_process()) {
    return std::make_unique<DaemonControllerDelegateLinuxMultiProcess>();
  }
  return std::make_unique<DaemonControllerDelegateLinuxSingleProcess>();
}

}  // namespace

// static
void DaemonController::SetHostType(const HostType* type) {
  g_host_type = type;
}

// static
scoped_refptr<DaemonController> DaemonController::Create() {
  std::unique_ptr<DaemonController::Delegate> delegate;
  if (g_host_type) {
    delegate = GetDelegateForHostType(*g_host_type);
  } else {
    delegate = std::make_unique<DaemonControllerDelegateLinuxMultiProcess>();
    auto state = delegate->GetState();
    if (state == DaemonController::STATE_STARTING ||
        state == DaemonController::STATE_STARTED) {
      return base::MakeRefCounted<DaemonController>(std::move(delegate));
    }
    delegate = GetDelegateForHostType(HostType::GetDefaultHostType());
  }

  return base::MakeRefCounted<DaemonController>(std::move(delegate));
}

}  // namespace remoting
