// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_POLICY_SANDBOX_DELEGATE_H_
#define SANDBOX_POLICY_SANDBOX_DELEGATE_H_

#include <string>

#include "base/process/process.h"
#include "build/build_config.h"

namespace sandbox {
namespace mojom {
enum class Sandbox;
}  // namespace mojom

class TargetConfig;
class TargetPolicy;

namespace policy {

class SandboxDelegate {
 public:
  virtual ~SandboxDelegate() {}

  // Returns the Sandbox to enforce on the process, or
  // Sandbox::kNoSandbox to run without a sandbox policy.
  virtual sandbox::mojom::Sandbox GetSandboxType() = 0;

#if BUILDFLAG(IS_WIN)
  // Returns a tag for the sandbox. All targets with the same tag will share
  // their TargetConfig configuration - the delegate can call
  // TargetConfig::IsConfigured() to skip setting this configuration after the
  // first such policy has been configured. Provide an empty string to force
  // every policy to be unique.
  virtual std::string GetSandboxTag() = 0;

  // Whether to disable the default policy specified in
  // AddPolicyForSandboxedProcess.
  virtual bool DisableDefaultPolicy() = 0;

  // Get the AppContainer ID for the sandbox. If this returns false then the
  // AppContainer will not be enabled for the process.
  virtual bool GetAppContainerId(std::string* appcontainer_id) = 0;

  // Called to initialize the target configuration for the process.
  virtual bool InitializeConfig(TargetConfig* config) = 0;

  // Called right before spawning the process. Returns false on failure.
  // Methods in TargetConfig only need to be called if IsConfigured() returns
  // false.
  virtual bool PreSpawnTarget(TargetPolicy* policy) = 0;

  // Called right after the process is launched, but before its thread is run.
  virtual void PostSpawnTarget(base::ProcessHandle process) = 0;

  // Whether this process should run inside a Job if running unsandboxed.
  virtual bool ShouldUnsandboxedRunInJob() = 0;

  // Whether this process will be compatible with Control-flow Enforcement
  // Technology (CET) / Hardware-enforced Stack Protection.
  virtual bool CetCompatible() = 0;

#endif  // BUILDFLAG(IS_WIN)
};

}  // namespace policy
}  // namespace sandbox

#endif  // SANDBOX_POLICY_SANDBOX_DELEGATE_H_
