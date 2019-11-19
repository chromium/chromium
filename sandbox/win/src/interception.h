// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines InterceptionManager, the class in charge of setting up interceptions
// for the sandboxed process. For more details see
// http://dev.chromium.org/developers/design-documents/sandbox .

#ifndef SANDBOX_SRC_INTERCEPTION_H_
#define SANDBOX_SRC_INTERCEPTION_H_

#include <stddef.h>

#include <list>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "sandbox/win/src/interceptors.h"
#include "sandbox/win/src/sandbox_types.h"

namespace sandbox {

class TargetProcess;

// Internal structures used for communication between the broker and the target.
struct DllPatchInfo;
struct DllInterceptionData;

// The InterceptionManager executes on the parent application, and it is in
// charge of setting up the desired interceptions, and placing the Interception
// Agent into the child application.
//
// The exposed API consists of two methods: AddToPatchedFunctions to set up a
// particular interception, and InitializeInterceptions to actually go ahead and
// perform all interceptions and transfer data to the child application.
//
// The typical usage is something like this:
//
// InterceptionManager interception_manager(child);
// if (!interception_manager.AddToPatchedFunctions(
//         L"ntdll.dll", "NtCreateFile",
//         sandbox::INTERCEPTION_SERVICE_CALL, &MyNtCreateFile, MY_ID_1))
//   return false;
//
// if (!interception_manager.AddToPatchedFunctions(
//         L"kernel32.dll", "CreateDirectoryW",
//         sandbox::INTERCEPTION_EAT, L"MyCreateDirectoryW@12", MY_ID_2))
//   return false;
//
// sandbox::ResultCode rc = interception_manager.InitializeInterceptions();
// if (rc != sandbox::SBOX_ALL_OK) {
//   DWORD error = ::GetLastError();
//   return rc;
// }
//
// Any required syncronization must be performed outside this class. Also, it is
// not possible to perform further interceptions after InitializeInterceptions
// is called.
//
class InterceptionManager {
  // The unit test will access private members.
  // Allow tests to be marked DISABLED_. Note that FLAKY_ and FAILS_ prefixes
  // do not work with sandbox tests.
  FRIEND_TEST_ALL_PREFIXES(InterceptionManagerTest, BufferLayout1);
  FRIEND_TEST_ALL_PREFIXES(InterceptionManagerTest, BufferLayout2);

 public:
  // An interception manager performs interceptions on a given child process.
  // If we are allowed to intercept functions that have been patched by somebody
  // else, relaxed should be set to true.
  // Note: We increase the child's reference count internally.
  InterceptionManager(TargetProcess* child_process, bool relaxed);
  ~InterceptionManager();

  // Patches function_name inside dll_name to point to replacement_code_address.
  // function_name has to be an exported symbol of dll_name.
  // Returns true on success.
  //
  // The new function should match the prototype and calling convention of the
  // function to intercept except for one extra argument (the first one) that
  // contains a pointer to the original function, to simplify the development
  // of interceptors (for IA32). In x64, there is no extra argument to the
  // interceptor, so the provided InterceptorId is used to keep a table of
  // intercepted functions so that the interceptor can index that table to get
  // the pointer that would have been the first argument (g_originals[id]).
  //
  // For example, to intercept NtClose, the following code could be used:
  //
  // typedef NTSTATUS (WINAPI *NtCloseFunction) (IN HANDLE Handle);
  // NTSTATUS WINAPI MyNtCose(IN NtCloseFunction OriginalClose,
  //                          IN HANDLE Handle) {
  //   // do something
  //   // call the original function
  //   return OriginalClose(Handle);
  // }
  //
  // And in x64:
  //
  // typedef NTSTATUS (WINAPI *NtCloseFunction) (IN HANDLE Handle);
  // NTSTATUS WINAPI MyNtCose64(IN HANDLE Handle) {
  //   // do something
  //   // call the original function
  //   NtCloseFunction OriginalClose = g_originals[NT_CLOSE_ID];
  //   return OriginalClose(Handle);
  // }
  bool AddToPatchedFunctions(const wchar_t* dll_name,
                             const char* function_name,
                             InterceptionType interception_type,
                             const void* replacement_code_address,
                             InterceptorId id);

  // Patches function_name inside dll_name to point to
  // replacement_function_name.
  bool AddToPatchedFunctions(const wchar_t* dll_name,
                             const char* function_name,
                             InterceptionType interception_type,
                             const char* replacement_function_name,
                             InterceptorId id);

  // The interception agent will unload the dll with dll_name.
  bool AddToUnloadModules(const wchar_t* dll_name);

  // Initializes all interceptions on the client.
  // Returns SBOX_ALL_OK on success, or an appropriate error code.
  //
  // The child process must be created suspended, and cannot be resumed until
  // after this method returns. In addition, no action should be performed on
  // the child that may cause it to resume momentarily, such as injecting
  // threads or APCs.
  //
  // This function must be called only once, after all interceptions have been
  // set up using AddToPatchedFunctions.
  ResultCode InitializeInterceptions();

 private:
  // Used to store the interception information until the actual set-up.
  struct InterceptionData {
    InterceptionData();
    InterceptionData(const InterceptionData& other);
    ~InterceptionData();

    InterceptionType type;            // Interception type.
    InterceptorId id;                 // Interceptor id.
    std::wstring dll;                 // Name of dll to intercept.
    std::string function;             // Name of function to intercept.
    std::string interceptor;          // Name of interceptor function.
    const void* interceptor_address;  // Interceptor's entry point.
  };

  // Calculates the size of the required configuration buffer.
  size_t GetBufferSize() const;

  // Rounds up the size of a given buffer, considering alignment (padding).
  // value is the current size of the buffer, and alignment is specified in
  // bytes.
  static inline size_t RoundUpToMultiple(size_t value, size_t alignment) {
    return ((value + alignment - 1) / alignment) * alignment;
  }

  // Sets up a given buffer with all the information that has to be transfered
  // to the child.
  // Returns true on success.
  //
  // The buffer size should be at least the value returned by GetBufferSize
  bool SetupConfigBuffer(void* buffer, size_t buffer_bytes);

  // Fills up the part of the transfer buffer that corresponds to information
  // about one dll to patch.
  // data is the first recorded interception for this dll.
  // Returns true on success.
  //
  // On successful return, buffer will be advanced from it's current position
  // to the point where the next block of configuration data should be written
  // (the actual interception info), and the current size of the buffer will
  // decrease to account the space used by this method.
  bool SetupDllInfo(const InterceptionData& data,
                    void** buffer,
                    size_t* buffer_bytes) const;

  // Fills up the part of the transfer buffer that corresponds to a single
  // function to patch.
  // dll_info points to the dll being updated with the interception stored on
  // data. The buffer pointer and remaining size are updated by this call.
  // Returns true on success.
  bool SetupInterceptionInfo(const InterceptionData& data,
                             void** buffer,
                             size_t* buffer_bytes,
                             DllPatchInfo* dll_info) const;

  // Returns true if this interception is to be performed by the child
  // as opposed to from the parent.
  bool IsInterceptionPerformedByChild(const InterceptionData& data) const;

  // Allocates a buffer on the child's address space (returned on
  // remote_buffer), and fills it with the contents of a local buffer.
  // Returns SBOX_ALL_OK on success.
  ResultCode CopyDataToChild(const void* local_buffer,
                             size_t buffer_bytes,
                             void** remote_buffer) const;

  // Performs the cold patch (from the parent) of ntdll.
  // Returns SBOX_ALL_OK on success.
  //
  // This method will insert additional interceptions to launch the interceptor
  // agent on the child process, if there are additional interceptions to do.
  ResultCode PatchNtdll(bool hot_patch_needed);

  // Peforms the actual interceptions on ntdll.
  // thunks is the memory to store all the thunks for this dll (on the child),
  // and dll_data is a local buffer to hold global dll interception info.
  // Returns SBOX_ALL_OK on success.
  ResultCode PatchClientFunctions(DllInterceptionData* thunks,
                                  size_t thunk_bytes,
                                  DllInterceptionData* dll_data);

  // The process to intercept.
  TargetProcess* child_;
  // Holds all interception info until the call to initialize (perform the
  // actual patch).
  std::list<InterceptionData> interceptions_;

  // Keep track of patches added by name.
  bool names_used_;

  // true if we are allowed to patch already-patched functions.
  bool relaxed_;

  DISALLOW_COPY_AND_ASSIGN(InterceptionManager);
};

// This macro simply calls interception_manager.AddToPatchedFunctions with
// the given service to intercept (INTERCEPTION_SERVICE_CALL), and assumes that
// the interceptor is called "TargetXXX", where XXX is the name of the service.
// Note that num_params is the number of bytes to pop out of the stack for
// the exported interceptor, following the calling convention of a service call
// (WINAPI = with the "C" underscore).
#if SANDBOX_EXPORTS
#if defined(_WIN64)
#define MAKE_SERVICE_NAME(service, params) "Target" #service "64"
#else
#define MAKE_SERVICE_NAME(service, params) "_Target" #service "@" #params
#endif

#define ADD_NT_INTERCEPTION(service, id, num_params)        \
  AddToPatchedFunctions(kNtdllName, #service,               \
                        sandbox::INTERCEPTION_SERVICE_CALL, \
                        MAKE_SERVICE_NAME(service, num_params), id)

#define INTERCEPT_NT(manager, service, id, num_params)                        \
  ((&Target##service) ? manager->ADD_NT_INTERCEPTION(service, id, num_params) \
                      : false)

// When intercepting the EAT it is important that the patched version of the
// function not call any functions imported from system libraries unless
// |TargetServices::InitCalled()| returns true, because it is only then that
// we are guaranteed that our IAT has been initialized.
#define INTERCEPT_EAT(manager, dll, function, id, num_params)             \
  ((&Target##function) ? manager->AddToPatchedFunctions(                  \
                             dll, #function, sandbox::INTERCEPTION_EAT,   \
                             MAKE_SERVICE_NAME(function, num_params), id) \
                       : false)
#else  // SANDBOX_EXPORTS
#if defined(_WIN64)
#define MAKE_SERVICE_NAME(service) &Target##service##64
#else
#define MAKE_SERVICE_NAME(service) &Target##service
#endif

#define ADD_NT_INTERCEPTION(service, id, num_params)            \
  AddToPatchedFunctions(                                        \
      kNtdllName, #service, sandbox::INTERCEPTION_SERVICE_CALL, \
      reinterpret_cast<void*>(MAKE_SERVICE_NAME(service)), id)

#define INTERCEPT_NT(manager, service, id, num_params) \
  manager->ADD_NT_INTERCEPTION(service, id, num_params)

// When intercepting the EAT it is important that the patched version of the
// function not call any functions imported from system libraries unless
// |TargetServices::InitCalled()| returns true, because it is only then that
// we are guaranteed that our IAT has been initialized.
#define INTERCEPT_EAT(manager, dll, function, id, num_params) \
  manager->AddToPatchedFunctions(                             \
      dll, #function, sandbox::INTERCEPTION_EAT,              \
      reinterpret_cast<void*>(MAKE_SERVICE_NAME(function)), id)
#endif  // SANDBOX_EXPORTS

}  // namespace sandbox

#endif  // SANDBOX_SRC_INTERCEPTION_H_
