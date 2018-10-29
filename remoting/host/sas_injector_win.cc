// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/sas_injector.h"

#include <windows.h>
#include <sas.h>

#include <memory>
#include <utility>

#include "base/logging.h"
#include "base/macros.h"
#include "base/win/registry.h"

namespace remoting {

namespace {

// The registry key and value holding the policy controlling software SAS
// generation.
const wchar_t kSystemPolicyKeyName[] =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\System";
const wchar_t kSoftwareSasValueName[] = L"SoftwareSASGeneration";

const DWORD kEnableSoftwareSasByServices = 1;

// Toggles the default software SAS generation policy to enable SAS generation
// by services. Non-default policy is not changed.
class ScopedSoftwareSasPolicy {
 public:
  ScopedSoftwareSasPolicy();
  ~ScopedSoftwareSasPolicy();

  bool Apply();

 private:
  // The handle of the registry key were SoftwareSASGeneration policy is stored.
  base::win::RegKey system_policy_;

  // True if the policy needs to be restored.
  bool restore_policy_ = false;

  DISALLOW_COPY_AND_ASSIGN(ScopedSoftwareSasPolicy);
};

ScopedSoftwareSasPolicy::ScopedSoftwareSasPolicy() = default;

ScopedSoftwareSasPolicy::~ScopedSoftwareSasPolicy() {
  // Restore the default policy by deleting the value that we have set.
  if (restore_policy_) {
    LONG result = system_policy_.DeleteValue(kSoftwareSasValueName);
    if (result != ERROR_SUCCESS) {
      SetLastError(result);
      PLOG(ERROR) << "Failed to restore the software SAS generation policy";
    }
  }
}

bool ScopedSoftwareSasPolicy::Apply() {
  // Query the currently set SoftwareSASGeneration policy.
  LONG result = system_policy_.Open(HKEY_LOCAL_MACHINE,
                                    kSystemPolicyKeyName,
                                    KEY_QUERY_VALUE | KEY_SET_VALUE |
                                        KEY_WOW64_64KEY);
  if (result != ERROR_SUCCESS) {
    SetLastError(result);
    PLOG(ERROR) << "Failed to open 'HKLM\\" << kSystemPolicyKeyName << "'";
    return false;
  }

  bool custom_policy = system_policy_.HasValue(kSoftwareSasValueName);

  // Override the default policy (i.e. there is no value in the registry) only.
  if (!custom_policy) {
    result = system_policy_.WriteValue(kSoftwareSasValueName,
                                       kEnableSoftwareSasByServices);
    if (result != ERROR_SUCCESS) {
      SetLastError(result);
      PLOG(ERROR) << "Failed to enable software SAS generation by services";
      return false;
    } else {
      restore_policy_ = true;
    }
  }

  return true;
}

} // namespace

// Sends Secure Attention Sequence.  Checks the current policy before sending.
class SasInjectorWin : public SasInjector {
 public:
  SasInjectorWin();
  ~SasInjectorWin() override;

  // SasInjector implementation.
  bool InjectSas() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(SasInjectorWin);
};

SasInjectorWin::SasInjectorWin() = default;

SasInjectorWin::~SasInjectorWin() = default;

bool SasInjectorWin::InjectSas() {
  // Enable software SAS generation by services and send SAS. SAS can still fail
  // if the policy does not allow services to generate software SAS.
  ScopedSoftwareSasPolicy enable_sas;
  if (!enable_sas.Apply()) {
    return false;
  }

  SendSAS(/*AsUser=*/FALSE);
  return true;
}

std::unique_ptr<SasInjector> SasInjector::Create() {
  return std::make_unique<SasInjectorWin>();
}

} // namespace remoting
