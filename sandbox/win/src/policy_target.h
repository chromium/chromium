// Copyright 2006-2008 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_WIN_SRC_POLICY_TARGET_H_
#define SANDBOX_WIN_SRC_POLICY_TARGET_H_

#include "sandbox/win/src/ipc_tags.h"
#include "sandbox/win/src/nt_internals.h"
#include "sandbox/win/src/sandbox_types.h"

namespace sandbox {

struct CountedParameterSetBase;

// Performs a policy lookup and returns true if the request should be passed to
// the broker process.
bool QueryBroker(IpcTag ipc_id, CountedParameterSetBase* params);

extern "C" {

// Interception of NtSetInformationThread on the child process.
// It should never be called directly.
SANDBOX_INTERCEPT NTSTATUS WINAPI TargetNtSetInformationThread(
    NtSetInformationThreadFunction orig_SetInformationThread,
    HANDLE thread,
    THREADINFOCLASS thread_info_class,
    PVOID thread_information,
    ULONG thread_information_bytes);

// Interception of NtOpenThreadToken on the child process.
// It should never be called directly
SANDBOX_INTERCEPT NTSTATUS WINAPI TargetNtOpenThreadToken(
    NtOpenThreadTokenFunction orig_OpenThreadToken, HANDLE thread,
    ACCESS_MASK desired_access, BOOLEAN open_as_self, PHANDLE token);

// Interception of NtOpenThreadTokenEx on the child process.
// It should never be called directly
SANDBOX_INTERCEPT NTSTATUS WINAPI TargetNtOpenThreadTokenEx(
    NtOpenThreadTokenExFunction orig_OpenThreadTokenEx, HANDLE thread,
    ACCESS_MASK desired_access, BOOLEAN open_as_self, ULONG handle_attributes,
    PHANDLE token);

}  // extern "C"

}  // namespace sandbox

#endif  // SANDBOX_WIN_SRC_POLICY_TARGET_H_
