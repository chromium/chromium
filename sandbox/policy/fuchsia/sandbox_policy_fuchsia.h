// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_POLICY_FUCHSIA_SANDBOX_POLICY_FUCHSIA_H_
#define SANDBOX_POLICY_FUCHSIA_SANDBOX_POLICY_FUCHSIA_H_

#include <fuchsia/io/cpp/fidl.h>
#include <lib/fidl/cpp/interface_handle.h>
#include <lib/zx/job.h>

#include "base/threading/sequence_bound.h"
#include "sandbox/policy/export.h"
#include "sandbox/policy/mojom/sandbox.mojom.h"

namespace base {
class FilteredServiceDirectory;
struct LaunchOptions;
}  // namespace base

namespace sandbox {
namespace policy {

class SANDBOX_POLICY_EXPORT SandboxPolicyFuchsia {
 public:
  // Must be called on the IO thread.
  explicit SandboxPolicyFuchsia(sandbox::mojom::Sandbox type);
  ~SandboxPolicyFuchsia();

  SandboxPolicyFuchsia(const SandboxPolicyFuchsia&) = delete;
  SandboxPolicyFuchsia& operator=(const SandboxPolicyFuchsia&) = delete;

  // Modifies the process launch |options| to achieve  the level of
  // isolation appropriate for current the sandbox type. The caller may then add
  // any descriptors or handles afterward to grant additional capabilities
  // to the new process.
  void UpdateLaunchOptionsForSandbox(base::LaunchOptions* options);

 private:
  sandbox::mojom::Sandbox type_;

  // Services directory used for the /svc namespace of the child process.
  base::SequenceBound<base::FilteredServiceDirectory>
      filtered_service_directory_;
  fidl::InterfaceHandle<::fuchsia::io::Directory> service_directory_client_;

  // Job in which the child process is launched.
  zx::job job_;
};

}  // namespace policy
}  // namespace sandbox

#endif  // SANDBOX_POLICY_FUCHSIA_SANDBOX_POLICY_FUCHSIA_H_
