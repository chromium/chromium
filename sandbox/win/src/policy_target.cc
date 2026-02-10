// Copyright 2006-2008 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/policy_target.h"

#include <ntstatus.h>
#include <stddef.h>

#include <tuple>

#include "base/compiler_specific.h"
#include "sandbox/win/src/crosscall_client.h"
#include "sandbox/win/src/ipc_tags.h"
#include "sandbox/win/src/policy_engine_processor.h"
#include "sandbox/win/src/policy_low_level.h"
#include "sandbox/win/src/policy_params.h"
#include "sandbox/win/src/sandbox_factory.h"
#include "sandbox/win/src/sandbox_nt_util.h"
#include "sandbox/win/src/sharedmem_ipc_client.h"
#include "sandbox/win/src/target_services.h"

namespace sandbox {

// Policy data.
extern void* volatile g_shared_policy_memory;
SANDBOX_INTERCEPT size_t g_shared_policy_size;

namespace {

std::optional<std::tuple<EvalResult, uintptr_t>> EvaluatePolicy(
    IpcTag ipc_id,
    CountedParameterSetBase* params) {
  DCHECK_NT(ipc_id <= IpcTag::kMaxValue);

  if (ipc_id <= IpcTag::UNUSED || ipc_id > IpcTag::kMaxValue) {
    return std::nullopt;
  }

  // Policy is only sent if required.
  if (!g_shared_policy_memory) {
    CHECK_NT(g_shared_policy_size);
    return std::nullopt;
  }

  PolicyGlobal* global_policy =
      reinterpret_cast<PolicyGlobal*>(g_shared_policy_memory);

  if (!UNSAFE_TODO(global_policy->entry[static_cast<size_t>(ipc_id)])) {
    return std::nullopt;
  }

  PolicyBuffer* policy = reinterpret_cast<PolicyBuffer*>(
      UNSAFE_TODO(reinterpret_cast<char*>(g_shared_policy_memory) +
                  reinterpret_cast<size_t>(
                      global_policy->entry[static_cast<size_t>(ipc_id)])));

  if ((reinterpret_cast<size_t>(
           UNSAFE_TODO(global_policy->entry[static_cast<size_t>(ipc_id)])) >
       global_policy->data_size) ||
      (g_shared_policy_size < global_policy->data_size)) {
    NOTREACHED_NT();
    return std::nullopt;
  }

  for (size_t i = 0; i < params->count; i++) {
    if (!UNSAFE_TODO(params->parameters[i]).IsValid()) {
      NOTREACHED_NT();
      return std::nullopt;
    }
  }

  PolicyProcessor processor(policy);
  PolicyResult result = processor.Evaluate(params->parameters, params->count);
  DCHECK_NT(POLICY_ERROR != result);
  if (result != POLICY_MATCH) {
    return std::nullopt;
  }
  return std::make_tuple(processor.GetAction(), processor.GetConstant());
}

}  // namespace

bool QueryBroker(IpcTag ipc_id, CountedParameterSetBase* params) {
  auto result = EvaluatePolicy(ipc_id, params);
  return result && std::get<EvalResult>(*result) == ASK_BROKER;
}

std::optional<uintptr_t> QueryReturnConst(IpcTag ipc_id,
                                          CountedParameterSetBase* params) {
  auto result = EvaluatePolicy(ipc_id, params);
  if (!result || std::get<EvalResult>(*result) != RETURN_CONST) {
    return std::nullopt;
  }
  return std::get<uintptr_t>(*result);
}

// -----------------------------------------------------------------------

// Hooks NtSetInformationThread to block RevertToSelf from being
// called before the actual call to LowerToken.
NTSTATUS WINAPI TargetNtSetInformationThread(
    NtSetInformationThreadFunction orig_SetInformationThread,
    HANDLE thread,
    THREADINFOCLASS thread_info_class,
    PVOID thread_information,
    ULONG thread_information_bytes) {
  do {
    if (SandboxFactory::GetTargetServices()->GetState()->RevertedToSelf())
      break;
    if (ThreadImpersonationToken != thread_info_class)
      break;
    // This is a revert to self.
    return STATUS_SUCCESS;
  } while (false);

  return orig_SetInformationThread(
      thread, thread_info_class, thread_information, thread_information_bytes);
}

// Hooks NtOpenThreadToken to force the open_as_self parameter to be set to
// false if we are still running with the impersonation token. open_as_self set
// to true means that the token will be open using the process token instead of
// the impersonation token. This is bad because the process token does not have
// access to open the thread token.
NTSTATUS WINAPI
TargetNtOpenThreadToken(NtOpenThreadTokenFunction orig_OpenThreadToken,
                        HANDLE thread,
                        ACCESS_MASK desired_access,
                        BOOLEAN open_as_self,
                        PHANDLE token) {
  if (!SandboxFactory::GetTargetServices()->GetState()->RevertedToSelf())
    open_as_self = false;

  return orig_OpenThreadToken(thread, desired_access, open_as_self, token);
}

// See comment for TargetNtOpenThreadToken
NTSTATUS WINAPI
TargetNtOpenThreadTokenEx(NtOpenThreadTokenExFunction orig_OpenThreadTokenEx,
                          HANDLE thread,
                          ACCESS_MASK desired_access,
                          BOOLEAN open_as_self,
                          ULONG handle_attributes,
                          PHANDLE token) {
  if (!SandboxFactory::GetTargetServices()->GetState()->RevertedToSelf())
    open_as_self = false;

  return orig_OpenThreadTokenEx(thread, desired_access, open_as_self,
                                handle_attributes, token);
}

}  // namespace sandbox
