// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/setup/daemon_controller.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "remoting/host/setup/daemon_controller_delegate_linux_multi_process.h"
#include "remoting/host/setup/daemon_controller_delegate_linux_single_process.h"

namespace remoting {

namespace {

static DaemonController::DelegateType g_delegate_type =
    DaemonController::DelegateType::kAuto;

}  // namespace

// static
void DaemonController::SetDelegateType(DelegateType type) {
  g_delegate_type = type;
}

// static
scoped_refptr<DaemonController> DaemonController::Create() {
  std::unique_ptr<DaemonController::Delegate> delegate;
  switch (g_delegate_type) {
    case DelegateType::kSingleProcess:
      delegate = std::make_unique<DaemonControllerDelegateLinuxSingleProcess>();
      break;

    case DelegateType::kMultiProcess:
      delegate = std::make_unique<DaemonControllerDelegateLinuxMultiProcess>();
      break;

    case DelegateType::kAuto:
      delegate = std::make_unique<DaemonControllerDelegateLinuxMultiProcess>();
      auto state = delegate->GetState();
      if (state == DaemonController::STATE_STARTING ||
          state == DaemonController::STATE_STARTED) {
        break;
      }
      delegate = std::make_unique<DaemonControllerDelegateLinuxSingleProcess>();
      break;
  }

  return base::MakeRefCounted<DaemonController>(std::move(delegate));
}

}  // namespace remoting
