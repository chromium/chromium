// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Sandbox is a sandbox library for windows processes. Use when you want a
// 'privileged' process and a 'locked down process' to interact with.
// The privileged process is called the broker and it is started by external
// means (such as the user starting it). The 'sandboxed' process is called the
// target and it is started by the broker. There can be many target processes
// started by a single broker process. This library provides facilities
// for both the broker and the target.
//
// The design rationale and relevant documents can be found at http://go/sbox.
//
// Note: this header does not include the SandboxFactory definitions because
// there are cases where the Sandbox library is linked against the main .exe
// while its API needs to be used in a DLL.

#ifndef SANDBOX_WIN_SRC_SANDBOX_H_
#define SANDBOX_WIN_SRC_SANDBOX_H_

#include <stddef.h>

#include <memory>
#include <optional>
#include <string_view>
#include <vector>

#include "base/containers/span.h"
#include "base/win/scoped_process_information.h"
#include "base/win/windows_types.h"
#include "sandbox/win/src/sandbox_policy.h"
#include "sandbox/win/src/sandbox_types.h"

// sandbox: Google User-Land Application Sandbox
namespace sandbox {

class BrokerServicesDelegate;
class BrokerServicesTargetTracker;
class PolicyDiagnosticsReceiver;
class ProcessState;
class TargetPolicy;
class TargetServices;
enum class Desktop;

// BrokerServices exposes all the broker API.
// The basic use is to start the target(s) and wait for them to end.
//
// This API is intended to be called in the following order
// (error checking omitted):
//  BrokerServices* broker = SandboxFactory::GetBrokerServices();
//  broker->Init();
//  PROCESS_INFORMATION target;
//  broker->SpawnTarget(target_exe_path, target_args, &target);
//  ::ResumeThread(target->hThread);
//  // -- later you can call:
//  broker->WaitForAllTargets(option);
//
// We need [[clang::lto_visibility_public]] because instances of this class are
// passed across module boundaries. This means different modules must have
// compatible definitions of the class even when LTO is enabled.
class [[clang::lto_visibility_public]] BrokerServices {
 public:
  // The callback used for receiving the SpawnTarget() process launch result.
  // The parameters include the new process and thread handle, the Win32 last
  // error code, and the sandbox ResultCode.
  using SpawnTargetCallback = base::OnceCallback<
      void(base::win::ScopedProcessInformation, DWORD, ResultCode)>;

  // Initializes the broker. Must be called before any other on this class.
  // The `delegate` configures parallel or synchronous process launching, and
  // implements tracing callbacks.
  // returns ALL_OK if successful. All other return values imply failure. If the
  // return is ERROR_GENERIC, you can call ::GetLastError() to get more
  // information.
  virtual ResultCode Init(std::unique_ptr<BrokerServicesDelegate> delegate) = 0;

  // May be called in place of Init in test code to add a tracker that validates
  // job notifications and signals an event when all tracked processes are done.
  virtual ResultCode InitForTesting(
      std::unique_ptr<BrokerServicesDelegate> delegate,
      std::unique_ptr<BrokerServicesTargetTracker> target_tracker) = 0;

  // Pre-creates an alternate desktop. Must be called before a non-default
  // desktop is used by any process.
  [[nodiscard]] virtual ResultCode CreateAlternateDesktop(Desktop desktop) = 0;
  // Destroys all desktops created for this Broker.
  virtual void DestroyDesktops() = 0;
  // Returns the name of the alternate desktop used. If an alternate window
  // station is specified, the name is prepended by the window station name,
  // followed by a backslash.
  virtual std::wstring GetDesktopName(Desktop desktop) = 0;

  // Returns the interface pointer to a new, empty policy object. Use this
  // interface to specify the sandbox policy for new processes created by
  // SpawnTarget().
  virtual std::unique_ptr<TargetPolicy> CreatePolicy() = 0;

  // Returns the interface pointer to a new, empty policy object. Use this
  // interface to specify the sandbox policy for new processes created by
  // SpawnTarget().
  //
  // The first time a specific value of `tag` is provided an empty policy will
  // be returned, and both TargetConfig and TargetPolicy methods should be
  // called to populate the object before passing it to SpawnTarget().
  //
  // The second and subsequent times a given `tag` is provided, the object will
  // share the backing data for state configured by TargetConfig methods (with
  // the first instance) and those methods should not be called for this policy.
  // TargetConfig::IsConfigured() will return `true` for the second and
  // subsequent objects created with a given `tag`. Methods on TargetPolicy
  // should continue to be called to populate the per-instance configuration.
  //
  // Provide an empty `tag` (or call CreatePolicy() with no tag) to create a
  // policy which never shares its TargetConfig state with another policy
  // object. For such an object both its TargetConfig and TargetPolicy methods
  // must be called every time.
  virtual std::unique_ptr<TargetPolicy> CreatePolicy(std::string_view tag) = 0;

  // Creates a new target (child process) in a suspended state and takes
  // ownership of |policy|.
  // Parameters:
  //   exe_path: This is the full path to the target binary. This parameter
  //   can be null and in this case the exe path must be the first argument
  //   of the command_line.
  //   command_line: The arguments to be passed as command line to the new
  //   process. This can be null if the exe_path parameter is not null.
  //   policy: This is the pointer to the policy object for the sandbox to
  //   be created.
  //   last_error: If an error or warning is returned from this method this
  //   parameter will hold the last Win32 error value.
  //   target: returns the resulting target process information such as process
  //   handle and PID just as if CreateProcess() had been called. The caller is
  //   responsible for closing the handles returned in this structure.
  // Returns:
  //   ALL_OK if successful. All other return values imply failure.
  virtual ResultCode SpawnTarget(const wchar_t* exe_path,
                                 const wchar_t* command_line,
                                 std::unique_ptr<TargetPolicy> policy,
                                 DWORD* last_error,
                                 PROCESS_INFORMATION* target) = 0;

  // Async version of SpawnTarget that supports parallel process launching.
  // Target creation happens on the thread pool when parallel launching is
  // enabled (controlled by BrokerServicesDelegate). This function is the same
  // as SpawnTarget, except the out parameters `last_error`, `target` and
  // ResultCode are passed to `result_callback`.
  virtual void SpawnTargetAsync(const wchar_t* exe_path,
                                const wchar_t* command_line,
                                std::unique_ptr<TargetPolicy> policy,
                                SpawnTargetCallback result_callback) = 0;

  // This call creates a snapshot of policies managed by the sandbox and
  // returns them via a helper class.
  // Parameters:
  //   receiver: The |PolicyDiagnosticsReceiver| implementation will be
  //   called to accept the results of the call.
  // Returns:
  //   ALL_OK if the request was dispatched. All other return values
  //   imply failure, and the responder will not receive its completion
  //   callback.
  virtual ResultCode GetPolicyDiagnostics(
      std::unique_ptr<PolicyDiagnosticsReceiver> receiver) = 0;

  // For the broker, we have some mitigations set early in startup. In
  // order to properly track those settings, SetStartingMitigations should be
  // called before other mitigations are set by RatchetDownSecurityMitigations
  virtual void SetStartingMitigations(MitigationFlags starting_mitigations) = 0;

  // RatchetDownSecurityMitigations is then called by the broker process to
  // gradually increase our security as startup continues. It's designed to
  // be called multiple times. If you don't call SetStartingMitigations first
  // and there were mitigations applied early in startup, the new mitigations
  // may not be applied.
  virtual bool RatchetDownSecurityMitigations(
      MitigationFlags additional_flags) = 0;

 protected:
  ~BrokerServices() {}
};

// TargetServices models the current process from the perspective
// of a target process. To obtain a pointer to it use
// Sandbox::GetTargetServices(). Note that this call returns a non-null
// pointer only if this process is in fact a target. A process is a target
// only if the process was spawned by a call to BrokerServices::SpawnTarget().
//
// This API allows the target to gain access to resources with a high
// privilege token and then when it is ready to perform dangerous activities
// (such as download content from the web) it can lower its token and
// enter into locked-down (sandbox) mode.
// The typical usage is as follows:
//
//   TargetServices* target_services = Sandbox::GetTargetServices();
//   if (target_services) {
//     // We are the target.
//     target_services->Init();
//     // Do work that requires high privileges here.
//     // ....
//     // When ready to enter lock-down mode call LowerToken:
//     target_services->LowerToken();
//   }
//
// For more information see the BrokerServices API documentation.
class [[clang::lto_visibility_public]] TargetServices {
 public:
  // Initializes the target. Must call this function before any other.
  // returns ALL_OK if successful. All other return values imply failure.
  // If the return is ERROR_GENERIC, you can call ::GetLastError() to get
  // more information.
  virtual ResultCode Init() = 0;

  // Returns a view of the delegate data blob - the target can use this data
  // early in the process's lifetime to set itself up - the format of the data
  // is decided by the embedder, and set using TargetPolicy::AddDelegateData().
  // If no data was provided the span will have a size of zero. This method can
  // be called at any time after Init(), but it is intended to be used sparingly
  // prior to calling LowerToken().
  virtual std::optional<base::span<const uint8_t>> GetDelegateData() = 0;

  // Discards the impersonation token and uses the lower token, call before
  // processing any untrusted data or running third-party code. If this call
  // fails the current process could be terminated immediately.
  virtual void LowerToken() = 0;

  // Returns the ProcessState object. Through that object it's possible to have
  // information about the current state of the process, such as whether
  // LowerToken has been called or not.
  virtual ProcessState* GetState() = 0;

 protected:
  ~TargetServices() {}
};

class [[clang::lto_visibility_public]] PolicyInfo {
 public:
  // Returns a JSON representation of the policy snapshot.
  // This pointer has the same lifetime as this PolicyInfo object.
  virtual const char* JsonString() = 0;
  virtual ~PolicyInfo() {}
};

// This is returned by BrokerServices::GetPolicyDiagnostics().
// PolicyInfo entries need not be ordered.
class [[clang::lto_visibility_public]] PolicyList {
 public:
  virtual std::vector<std::unique_ptr<PolicyInfo>>::iterator begin() = 0;
  virtual std::vector<std::unique_ptr<PolicyInfo>>::iterator end() = 0;
  virtual size_t size() const = 0;
  virtual ~PolicyList() {}
};

// This class mediates calls to BrokerServices::GetPolicyDiagnostics().
class [[clang::lto_visibility_public]] PolicyDiagnosticsReceiver {
 public:
  // ReceiveDiagnostics() should return quickly and should not block the
  // thread on which it is called.
  virtual void ReceiveDiagnostics(std::unique_ptr<PolicyList> policies) = 0;
  // OnError() is passed any errors encountered and |ReceiveDiagnostics|
  // will not be called.
  virtual void OnError(ResultCode code) = 0;
  virtual ~PolicyDiagnosticsReceiver() {}
};

// For tests only -  this class is notified when the sandbox's internal tracking
// thread sees a process added or removed. Methods in this class should complete
// quickly and should not have side effects.
class [[clang::lto_visibility_public]] BrokerServicesTargetTracker {
 public:
  // Called when job notifications indicate that a new process is added.
  virtual void OnTargetAdded() = 0;
  // Called when job notifications indicate that a process has finished.
  virtual void OnTargetRemoved() = 0;
  virtual ~BrokerServicesTargetTracker() {}
};

// Used internally by SpawnTarget() to return process launch info from a task.
struct [[clang::lto_visibility_public]] CreateTargetResult {
  base::win::ScopedProcessInformation process_info;
  DWORD last_error;
  ResultCode result_code;
};

// This class configures BrokerServices to use parallel or synchronous process
// launching, and provides callbacks for implementing tracing.
class [[clang::lto_visibility_public]] BrokerServicesDelegate {
 public:
  // Returns true if parallel launching is enabled, otherwise synchronous
  // launching is used.
  virtual bool ParallelLaunchEnabled() = 0;
  // This method runs `task` on the thread pool, then runs `reply` on the
  // calling sequence with the returned CreateTargetResult. This method must be
  // implemented if ParallelLaunchEnabled() can return true.
  virtual void ParallelLaunchPostTaskAndReplyWithResult(
      const base::Location& from_here,
      base::OnceCallback<CreateTargetResult()> task,
      base::OnceCallback<void(CreateTargetResult)> reply) = 0;
  // Called before a target process is created. If parallel launching is
  // enabled, this will be called on the thread pool.
  virtual void BeforeTargetProcessCreateOnCreationThread(
      const void* trace_id) = 0;
  // Called after a target process is created. If parallel launching is enabled,
  // this will be called on the thread pool.
  virtual void AfterTargetProcessCreateOnCreationThread(const void* trace_id,
                                                        DWORD process_id) = 0;
  virtual ~BrokerServicesDelegate() {}
};

}  // namespace sandbox

#endif  // SANDBOX_WIN_SRC_SANDBOX_H_
