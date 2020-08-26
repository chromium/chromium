// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PERFORMANCE_MANAGER_MECHANISMS_USERSPACE_SWAP_RENDERER_INITIALIZATION_IMPL_H_
#define CONTENT_RENDERER_PERFORMANCE_MANAGER_MECHANISMS_USERSPACE_SWAP_RENDERER_INITIALIZATION_IMPL_H_

#include "base/files/scoped_file.h"
#include "base/macros.h"
#include "chromeos/memory/userspace_swap/userspace_swap.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace performance_manager {
namespace mechanism {

class UserspaceSwapRendererInitializationImpl {
 public:
  UserspaceSwapRendererInitializationImpl();
  ~UserspaceSwapRendererInitializationImpl();

  static bool UserspaceSwapSupportedAndEnabled();

  // PreSandboxSetup() is responsible for creating any resources that might be
  // needed before we enter the sandbox.
  bool PreSandboxSetup();

  // TransferFDsOrCleanup should be called after the sandbox has been entered.
  void TransferFDsOrCleanup();

 private:
  int uffd_errno_ = 0;
  base::ScopedFD uffd_;

  DISALLOW_COPY_AND_ASSIGN(UserspaceSwapRendererInitializationImpl);
};

}  // namespace mechanism
}  // namespace performance_manager

#endif  // CONTENT_RENDERER_PERFORMANCE_MANAGER_MECHANISMS_USERSPACE_SWAP_RENDERER_INITIALIZATION_IMPL_H_
