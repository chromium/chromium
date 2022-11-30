// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/sandbox_status_service.h"

#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "sandbox/policy/linux/sandbox_linux.h"

namespace content {

// static
void SandboxStatusService::MakeSelfOwnedReceiver(
    mojo::PendingReceiver<mojom::SandboxStatusService> receiver) {
  mojo::MakeSelfOwnedReceiver(std::make_unique<SandboxStatusService>(),
                              std::move(receiver));
}

SandboxStatusService::SandboxStatusService() = default;

SandboxStatusService::~SandboxStatusService() = default;

void SandboxStatusService::GetSandboxStatus(GetSandboxStatusCallback callback) {
  std::move(callback).Run(
      sandbox::policy::SandboxLinux::GetInstance()->GetStatus());
}

}  // namespace content
