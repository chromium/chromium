// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_WIN_SRC_SANDBOX_TYPES_H_
#define SANDBOX_WIN_SRC_SANDBOX_TYPES_H_

#include "base/process/kill.h"
#include "base/process/launch.h"

namespace sandbox {

// Operation result codes returned by the sandbox API.
//
// Note: These codes are listed in a histogram and any new codes should be added
// at the end. If the underlying type is changed then the forward declaration in
// sandbox_init.h must be updated.
//
enum ResultCode : int {
  SBOX_ALL_OK = 0,
  // Error is originating on the win32 layer. Call GetlastError() for more
  // information.
  SBOX_ERROR_GENERIC = 1,
  // An invalid combination of parameters was given to the API.
  SBOX_ERROR_BAD_PARAMS = 2,
  // The desired operation is not supported at this time.
  SBOX_ERROR_UNSUPPORTED = 3,
  // The request requires more memory that allocated or available.
  SBOX_ERROR_NO_SPACE = 4,
  // The ipc service requested does not exist.
  SBOX_ERROR_INVALID_IPC = 5,
  // The ipc service did not complete.
  SBOX_ERROR_FAILED_IPC = 6,
  // The requested handle was not found.
  SBOX_ERROR_NO_HANDLE = 7,
  // This function was not expected to be called at this time.
  SBOX_ERROR_UNEXPECTED_CALL = 8,
  // WaitForAllTargets is already called.
  SBOX_ERROR_WAIT_ALREADY_CALLED = 9,
  // A channel error prevented DoCall from executing.
  SBOX_ERROR_CHANNEL_ERROR = 10,
  // Failed to create the alternate desktop.
  SBOX_ERROR_CANNOT_CREATE_DESKTOP = 11,
  // Failed to create the alternate window station.
  SBOX_ERROR_CANNOT_CREATE_WINSTATION = 12,
  // Failed to switch back to the interactive window station.
  SBOX_ERROR_FAILED_TO_SWITCH_BACK_WINSTATION = 13,
  // The supplied AppContainer is not valid.
  SBOX_ERROR_INVALID_APP_CONTAINER = 14,
  // The supplied capability is not valid.
  SBOX_ERROR_INVALID_CAPABILITY = 15,
  // There is a failure initializing the AppContainer.
  SBOX_ERROR_CANNOT_INIT_APPCONTAINER = 16,
  // Initializing or updating ProcThreadAttributes failed.
  SBOX_ERROR_PROC_THREAD_ATTRIBUTES = 17,
  // Error in creating process.
  SBOX_ERROR_CREATE_PROCESS = 18,
  // Failure calling delegate PreSpawnTarget.
  SBOX_ERROR_DELEGATE_PRE_SPAWN = 19,
  // Could not assign process to job object.
  SBOX_ERROR_ASSIGN_PROCESS_TO_JOB_OBJECT = 20,
  // Could not assign process to job object.
  SBOX_ERROR_SET_THREAD_TOKEN = 21,
  // Could not get thread context of new process.
  SBOX_ERROR_GET_THREAD_CONTEXT = 22,
  // Could not duplicate target info of new process.
  SBOX_ERROR_DUPLICATE_TARGET_INFO = 23,
  // Could not set low box token.
  SBOX_ERROR_SET_LOW_BOX_TOKEN = 24,
  // Could not create file mapping for IPC dispatcher.
  SBOX_ERROR_CREATE_FILE_MAPPING = 25,
  // Could not duplicate shared section into target process for IPC dispatcher.
  SBOX_ERROR_DUPLICATE_SHARED_SECTION = 26,
  // Could not map view of shared memory in broker.
  SBOX_ERROR_MAP_VIEW_OF_SHARED_SECTION = 27,
  // Could not apply ASLR mitigations to target process.
  SBOX_ERROR_APPLY_ASLR_MITIGATIONS = 28,
  // Could not setup one of the required interception services.
  SBOX_ERROR_SETUP_BASIC_INTERCEPTIONS = 29,
  // Could not setup basic interceptions.
  SBOX_ERROR_SETUP_INTERCEPTION_SERVICE = 30,
  // Could not initialize interceptions. This usually means 3rd party software
  // is stomping on our hooks, or can sometimes mean the syscall format has
  // changed.
  SBOX_ERROR_INITIALIZE_INTERCEPTIONS = 31,
  // Could not setup the imports for ntdll in target process.
  SBOX_ERROR_SETUP_NTDLL_IMPORTS = 32,
  // Could not setup the handle closer in target process.
  SBOX_ERROR_SETUP_HANDLE_CLOSER = 33,
  // Cannot get the current Window Station.
  SBOX_ERROR_CANNOT_GET_WINSTATION = 34,
  // Cannot query the security attributes of the current Window Station.
  SBOX_ERROR_CANNOT_QUERY_WINSTATION_SECURITY = 35,
  // Cannot get the current Desktop.
  SBOX_ERROR_CANNOT_GET_DESKTOP = 36,
  // Cannot query the security attributes of the current Desktop.
  SBOX_ERROR_CANNOT_QUERY_DESKTOP_SECURITY = 37,
  // Cannot setup the interception manager config buffer.
  SBOX_ERROR_CANNOT_SETUP_INTERCEPTION_CONFIG_BUFFER = 38,
  // Cannot copy data to the child process.
  SBOX_ERROR_CANNOT_COPY_DATA_TO_CHILD = 39,
  // Cannot setup the interception thunk.
  SBOX_ERROR_CANNOT_SETUP_INTERCEPTION_THUNK = 40,
  // Cannot resolve the interception thunk.
  SBOX_ERROR_CANNOT_RESOLVE_INTERCEPTION_THUNK = 41,
  // Cannot write interception thunk to child process.
  SBOX_ERROR_CANNOT_WRITE_INTERCEPTION_THUNK = 42,
  // Cannot find the base address of the new process.
  SBOX_ERROR_CANNOT_FIND_BASE_ADDRESS = 43,
  // Cannot create the AppContainer profile.
  SBOX_ERROR_CREATE_APPCONTAINER_PROFILE = 44,
  // Cannot create the AppContainer as the main executable can't be accessed.
  SBOX_ERROR_CREATE_APPCONTAINER_PROFILE_ACCESS_CHECK = 45,
  // Cannot create the AppContainer as adding a capability failed.
  SBOX_ERROR_CREATE_APPCONTAINER_PROFILE_CAPABILITY = 46,
  // Cannot initialize a job object.
  SBOX_ERROR_CANNOT_INIT_JOB = 47,
  // Invalid LowBox SID string.
  SBOX_ERROR_INVALID_LOWBOX_SID = 48,
  // Cannot create restricted token.
  SBOX_ERROR_CANNOT_CREATE_RESTRICTED_TOKEN = 49,
  // Cannot set the integrity level on a desktop object.
  SBOX_ERROR_CANNOT_SET_DESKTOP_INTEGRITY = 50,
  // Cannot create a LowBox token.
  SBOX_ERROR_CANNOT_CREATE_LOWBOX_TOKEN = 51,
  // Cannot modify LowBox token's DACL.
  SBOX_ERROR_CANNOT_MODIFY_LOWBOX_TOKEN_DACL = 52,
  // Cannot create restricted impersonation token.
  SBOX_ERROR_CANNOT_CREATE_RESTRICTED_IMP_TOKEN = 53,
  // Cannot duplicate target process handle.
  SBOX_ERROR_CANNOT_DUPLICATE_PROCESS_HANDLE = 54,
  // Cannot load executable for variable transfer.
  SBOX_ERROR_CANNOT_LOADLIBRARY_EXECUTABLE = 55,
  // Cannot find variable address for transfer.
  SBOX_ERROR_CANNOT_FIND_VARIABLE_ADDRESS = 56,
  // Cannot write variable value.
  SBOX_ERROR_CANNOT_WRITE_VARIABLE_VALUE = 57,
  // Short write to variable.
  SBOX_ERROR_INVALID_WRITE_VARIABLE_SIZE = 58,
  // Cannot initialize BrokerServices.
  SBOX_ERROR_CANNOT_INIT_BROKERSERVICES = 59,
  // Placeholder for last item of the enum.
  SBOX_ERROR_LAST
};

// If the sandbox cannot create a secure environment for the target, the
// target will be forcibly terminated. These are the process exit codes.
enum TerminationCodes {
  SBOX_FATAL_INTEGRITY = 7006,        // Could not set the integrity level.
  SBOX_FATAL_DROPTOKEN = 7007,        // Could not lower the token.
  SBOX_FATAL_FLUSHANDLES = 7008,      // Failed to flush registry handles.
  SBOX_FATAL_CACHEDISABLE = 7009,     // Failed to forbid HCKU caching.
  SBOX_FATAL_CLOSEHANDLES = 7010,     // Failed to close pending handles.
  SBOX_FATAL_MITIGATION = 7011,       // Could not set the mitigation policy.
  SBOX_FATAL_MEMORY_EXCEEDED = 7012,  // Exceeded the job memory limit.
  SBOX_FATAL_WARMUP = 7013,           // Failed to warmup.
  SBOX_FATAL_LAST
};

#if !defined(SANDBOX_FUZZ_TARGET)
static_assert(SBOX_FATAL_MEMORY_EXCEEDED ==
                  base::win::kSandboxFatalMemoryExceeded,
              "Value for SBOX_FATAL_MEMORY_EXCEEDED must match base.");
#endif  // !defined(SANDBOX_FUZZ_TARGET)

class BrokerServices;
class TargetServices;

// Contains the pointer to a target or broker service.
struct SandboxInterfaceInfo {
  BrokerServices* broker_services;
  TargetServices* target_services;
};

#if SANDBOX_EXPORTS
#define SANDBOX_INTERCEPT extern "C" __declspec(dllexport)
#else
#define SANDBOX_INTERCEPT extern "C"
#endif

enum InterceptionType {
  INTERCEPTION_INVALID = 0,
  INTERCEPTION_SERVICE_CALL,  // Trampoline of an NT native call
  INTERCEPTION_EAT,
  INTERCEPTION_SIDESTEP,        // Preamble patch
  INTERCEPTION_SMART_SIDESTEP,  // Preamble patch but bypass internal calls
  INTERCEPTION_UNLOAD_MODULE,   // Unload the module (don't patch)
  INTERCEPTION_LAST             // Placeholder for last item in the enumeration
};

}  // namespace sandbox

#endif  // SANDBOX_WIN_SRC_SANDBOX_TYPES_H_
