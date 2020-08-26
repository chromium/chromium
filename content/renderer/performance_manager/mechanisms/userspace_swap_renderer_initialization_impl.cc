// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/performance_manager/mechanisms/userspace_swap_renderer_initialization_impl.h"

#include "chromeos/memory/userspace_swap/userfaultfd.h"
#include "chromeos/memory/userspace_swap/userspace_swap.h"
#include "chromeos/memory/userspace_swap/userspace_swap.mojom.h"
#include "content/public/renderer/render_thread.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace performance_manager {
namespace mechanism {

namespace {
using chromeos::memory::userspace_swap::UserfaultFD;

mojo::Remote<::userspace_swap::mojom::UserspaceSwapInitialization>
ConnectUserspaceSwapInitializationToBrowser() {
  mojo::Remote<::userspace_swap::mojom::UserspaceSwapInitialization> remote;
  content::RenderThread::Get()->BindHostReceiver(
      remote.BindNewPipeAndPassReceiver());

  return remote;
}

}  // namespace

UserspaceSwapRendererInitializationImpl::
    UserspaceSwapRendererInitializationImpl() {
  CHECK(UserspaceSwapRendererInitializationImpl::
            UserspaceSwapSupportedAndEnabled());
}

UserspaceSwapRendererInitializationImpl::
    ~UserspaceSwapRendererInitializationImpl() = default;

bool UserspaceSwapRendererInitializationImpl::PreSandboxSetup() {
  // The caller should have verified platform support before invoking
  // PreSandboxSetup.
  CHECK(UserspaceSwapRendererInitializationImpl::
            UserspaceSwapSupportedAndEnabled());

  std::unique_ptr<UserfaultFD> uffd =
      UserfaultFD::Create(static_cast<UserfaultFD::Features>(
          UserfaultFD::Features::kFeatureUnmap |
          UserfaultFD::Features::kFeatureRemap |
          UserfaultFD::Features::kFeatureRemove));

  if (!uffd) {
    uffd_errno_ = errno;
  }

  uffd_ = uffd->ReleaseFD();
  return uffd_.is_valid();
}

void UserspaceSwapRendererInitializationImpl::TransferFDsOrCleanup() {
  auto remote = ConnectUserspaceSwapInitializationToBrowser();
  remote->TransferUserfaultFD(uffd_errno_,
                              mojo::PlatformHandle(std::move(uffd_)));
  uffd_errno_ = 0;
}

// Static
bool UserspaceSwapRendererInitializationImpl::
    UserspaceSwapSupportedAndEnabled() {
  return chromeos::memory::userspace_swap::UserspaceSwapSupportedAndEnabled();
}

}  // namespace mechanism
}  // namespace performance_manager
