// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_WIN_SRC_PROCESS_MITIGATIONS_H_
#define SANDBOX_WIN_SRC_PROCESS_MITIGATIONS_H_

#include <windows.h>

#include <stddef.h>

#include "sandbox/win/src/security_level.h"

namespace sandbox {

// Sets the mitigation policy for the current process, ignoring any settings
// that are invalid for the current version of Windows.
bool ApplyProcessMitigationsToCurrentProcess(MitigationFlags flags);

// Sets the mitigation policy for the current thread, ignoring any settings
// that are invalid for the current version of Windows.
bool ApplyMitigationsToCurrentThread(MitigationFlags flags);

// Returns the flags that must be enforced after startup for the current OS
// version.
MitigationFlags FilterPostStartupProcessMitigations(MitigationFlags flags);

// Converts sandbox flags to the PROC_THREAD_ATTRIBUTE_SECURITY_CAPABILITIES
// policy flags used by UpdateProcThreadAttribute().
// - |policy_flags| must be a two-element DWORD64 array.
// - |size| is a size_t so that it can be passed directly into
//   UpdateProcThreadAttribute().
void ConvertProcessMitigationsToPolicy(MitigationFlags flags,
                                       DWORD64* policy_flags,
                                       size_t* size);

// Converts sandbox flags to COMPONENT_FILTER so that it can be passed directly
// to UpdateProcThreadAttribute().
void ConvertProcessMitigationsToComponentFilter(MitigationFlags flags,
                                                COMPONENT_FILTER* filter);

// Adds mitigations that need to be performed on the suspended target process
// before execution begins.
bool ApplyProcessMitigationsToSuspendedProcess(HANDLE process,
                                               MitigationFlags flags);

// Returns the list of process mitigations which can be enabled post startup.
MitigationFlags GetAllowedPostStartupProcessMitigations();

// Returns true if all the supplied flags can be set after a process starts.
bool CanSetProcessMitigationsPostStartup(MitigationFlags flags);

// Returns true if all the supplied flags can be set before a process starts.
bool CanSetProcessMitigationsPreStartup(MitigationFlags flags);

// Returns true if all the supplied flags can be set on the current thread.
bool CanSetMitigationsPerThread(MitigationFlags flags);

}  // namespace sandbox

#endif  // SANDBOX_WIN_SRC_PROCESS_MITIGATIONS_H_
