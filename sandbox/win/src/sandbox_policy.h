// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_WIN_SRC_SANDBOX_POLICY_H_
#define SANDBOX_WIN_SRC_SANDBOX_POLICY_H_

#include <stddef.h>
#include <stdint.h>

#include "base/containers/span.h"
#include "sandbox/win/src/sandbox_types.h"
#include "sandbox/win/src/security_level.h"

namespace sandbox {

class AppContainer;

// Desktop used to launch child, controls GetDesktop().
enum class Desktop {
  // Child is launched without changing the desktop.
  kDefault,
  // Child is launched using the alternate desktop.
  kAlternateDesktop,
  // Child is launched using the anternate desktop and window station.
  kAlternateWinstation,
};

// Allowable semantics when an AllowFileAccess() rule is matched.
enum class FileSemantics {
  kAllowAny,       // Allows open or create for any kind of access that
                   // the file system supports.
  kAllowReadonly,  // Allows open or create with read access only
                   // (includes access to query the attributes of a file).
};

// Configures sandbox policy to close a given handle or set of handles in the
// target just before entering lockdown.
enum class HandleToClose {
  // Closes any Section ending with the name `\windows_shell_global_counters`.
  kWindowsShellGlobalCounters,
  // Closes any File with the full name `\Device\DeviceApi`.
  kDeviceApi,
  // Closes any File with the full name `\Device\KsecDD`.
  kKsecDD,
  // Closes all handles of type `ALPC Port` and closes the Csrss heap.
  kDisconnectCsrss,
};

// Policy configuration that can be shared over multiple targets of the same tag
// (see BrokerServicesBase::CreatePolicy(tag)). Methods in TargetConfig will
// only need to be called the first time a TargetPolicy object with a given tag
// is configured.
//
// We need [[clang::lto_visibility_public]] because instances of this class are
// passed across module boundaries. This means different modules must have
// compatible definitions of the class even when LTO is enabled.
class [[clang::lto_visibility_public]] TargetConfig {
 public:
  virtual ~TargetConfig() {}

  // Returns `true` when the TargetConfig of this policy object has been
  // populated. Methods in TargetConfig should not be called.
  //
  // Returns `false` if TargetConfig methods do need to be called to configure
  // this policy object.
  virtual bool IsConfigured() const = 0;

  // Sets the security level for the target process' two tokens.
  // This setting is permanent and cannot be changed once the target process is
  // spawned.
  // initial: the security level for the initial token. This is the token that
  //   is used by the process from the creation of the process until the moment
  //   the process calls TargetServices::LowerToken() or the process calls
  //   win32's RevertToSelf(). Once this happens the initial token is no longer
  //   available and the lockdown token is in effect. Using an initial token is
  //   not compatible with AppContainer, see SetAppContainer.
  // lockdown: the security level for the token that comes into force after the
  //   process calls TargetServices::LowerToken() or the process calls
  //   RevertToSelf(). See the explanation of each level in the TokenLevel
  //   definition.
  // Return value: SBOX_ALL_OK if the setting succeeds and false otherwise.
  //   Returns false if the lockdown value is more permissive than the initial
  //   value.
  //
  // Important: most of the sandbox-provided security relies on this single
  // setting. The caller should strive to set the lockdown level as restricted
  // as possible.
  [[nodiscard]] virtual ResultCode SetTokenLevel(TokenLevel initial,
                                                 TokenLevel lockdown) = 0;

  // Returns the initial token level.
  virtual TokenLevel GetInitialTokenLevel() const = 0;

  // Returns the lockdown token level.
  virtual TokenLevel GetLockdownTokenLevel() const = 0;

  // Sets the security level of the Job Object to which the target process will
  // belong. This setting is permanent and cannot be changed once the target
  // process is spawned. The job controls the global security settings which
  // can not be specified in the token security profile.
  // job_level: the security level for the job. See the explanation of each
  //   level in the JobLevel definition.
  // ui_exceptions: specify what specific rights that are disabled in the
  //   chosen job_level that need to be granted. Use this parameter to avoid
  //   selecting the next permissive job level unless you need all the rights
  //   that are granted in such level.
  //   The exceptions can be specified as a combination of the following
  //   constants:
  // JOB_OBJECT_UILIMIT_HANDLES : grant access to all user-mode handles. These
  //   include windows, icons, menus and various GDI objects. In addition the
  //   target process can set hooks, and broadcast messages to other processes
  //   that belong to the same desktop.
  // JOB_OBJECT_UILIMIT_READCLIPBOARD : grant read-only access to the clipboard.
  // JOB_OBJECT_UILIMIT_WRITECLIPBOARD : grant write access to the clipboard.
  // JOB_OBJECT_UILIMIT_SYSTEMPARAMETERS : allow changes to the system-wide
  //   parameters as defined by the Win32 call SystemParametersInfo().
  // JOB_OBJECT_UILIMIT_DISPLAYSETTINGS : allow programmatic changes to the
  //  display settings.
  // JOB_OBJECT_UILIMIT_GLOBALATOMS : allow access to the global atoms table.
  // JOB_OBJECT_UILIMIT_DESKTOP : allow the creation of new desktops.
  // JOB_OBJECT_UILIMIT_EXITWINDOWS : allow the call to ExitWindows().
  //
  // Return value: SBOX_ALL_OK if the setting succeeds and false otherwise.
  //
  // Note: JOB_OBJECT_XXXX constants are defined in winnt.h and documented at
  // length in:
  //   http://msdn2.microsoft.com/en-us/library/ms684152.aspx
  //
  // Note: the recommended level is JobLevel::kLockdown.
  [[nodiscard]] virtual ResultCode SetJobLevel(JobLevel job_level,
                                               uint32_t ui_exceptions) = 0;

  // Returns the job level.
  virtual JobLevel GetJobLevel() const = 0;

  // Sets a hard limit on the size of the commit set for the sandboxed process.
  // If the limit is reached, the process will be terminated with
  // SBOX_FATAL_MEMORY_EXCEEDED (7012).
  virtual void SetJobMemoryLimit(size_t memory_limit) = 0;

  // Adds a policy rule effective for processes spawned using this policy.
  // Files matching `pattern` can be opened following FileSemantics.
  //
  // pattern: A specific full path or a full path with wildcard patterns.
  //   The valid wildcards are:
  //   '*' : Matches zero or more character. Only one in series allowed.
  //   '?' : Matches a single character. One or more in series are allowed.
  // Examples:
  //   "c:\\documents and settings\\vince\\*.dmp"
  //   "c:\\documents and settings\\*\\crashdumps\\*.dmp"
  //   "c:\\temp\\app_log_?????_chrome.txt"
  //
  // Note: Do not add new uses of this function - instead proxy file handles
  // into your process via normal Chrome IPC.
  [[nodiscard]] virtual ResultCode AllowFileAccess(FileSemantics semantics,
                                                   const wchar_t* pattern) = 0;

  // Adds a policy rule effective for processes spawned using this policy.
  // Modules patching `pattern` (see AllowFileAccess) can still be loaded under
  // Code-Integrity Guard (MITIGATION_FORCE_MS_SIGNED_BINS).
  [[nodiscard]] virtual ResultCode AllowExtraDlls(const wchar_t* pattern) = 0;

  // Adds a policy rule effective for processes spawned using this policy.
  // Fake gdi init to allow user32 and gdi32 to initialize under Win32 Lockdown.
  [[nodiscard]] virtual ResultCode SetFakeGdiInit() = 0;

  // Adds a dll that will be unloaded in the target process before it gets
  // a chance to initialize itself. Typically, dlls that cause the target
  // to crash go here.
  virtual void AddDllToUnload(const wchar_t* dll_name) = 0;

  // Sets the integrity level of the process in the sandbox. Both the initial
  // token and the main token will be affected by this. If the integrity level
  // is set to a level higher than the current level, the sandbox will fail
  // to start.
  [[nodiscard]] virtual ResultCode SetIntegrityLevel(IntegrityLevel level) = 0;

  // Returns the initial integrity level used.
  virtual IntegrityLevel GetIntegrityLevel() const = 0;

  // Sets the integrity level of the process in the sandbox. The integrity level
  // will not take effect before you call LowerToken. User Interface Privilege
  // Isolation is not affected by this setting and will remain off for the
  // process in the sandbox. If the integrity level is set to a level higher
  // than the current level, the sandbox will fail to start.
  virtual void SetDelayedIntegrityLevel(IntegrityLevel level) = 0;

  // Sets the LowBox token for sandboxed process. This is mutually exclusive
  // with SetAppContainer method.
  [[nodiscard]] virtual ResultCode SetLowBox(const wchar_t* sid) = 0;

  // Sets the mitigations enabled when the process is created. Most of these
  // are implemented as attributes passed via STARTUPINFOEX. So they take
  // effect before any thread in the target executes. The declaration of
  // MitigationFlags is followed by a detailed description of each flag.
  [[nodiscard]] virtual ResultCode SetProcessMitigations(
      MitigationFlags flags) = 0;

  // Returns the currently set mitigation flags.
  virtual MitigationFlags GetProcessMitigations() = 0;

  // Sets process mitigation flags that don't take effect before the call to
  // LowerToken().
  [[nodiscard]] virtual ResultCode SetDelayedProcessMitigations(
      MitigationFlags flags) = 0;

  // Returns the currently set delayed mitigation flags.
  virtual MitigationFlags GetDelayedProcessMitigations() const = 0;

  // Adds a restricting random SID to the restricted SIDs list as well as
  // the default DACL.
  virtual void AddRestrictingRandomSid() = 0;

  // Locks down the default DACL of the created lockdown and initial tokens
  // to restrict what other processes are allowed to access a process' kernel
  // resources.
  virtual void SetLockdownDefaultDacl() = 0;

  // Configure policy to use an AppContainer profile. |package_name| is the
  // name of the profile to use.
  [[nodiscard]] virtual ResultCode AddAppContainerProfile(
      const wchar_t* package_name) = 0;

  // Get the configured AppContainer. The returned object lasts only as long as
  // the containing TargetConfig.
  virtual AppContainer* GetAppContainer() = 0;

  // Adds a handle type to close in the child. See HandleToClose for supported
  // types.
  virtual void AddKernelObjectToClose(HandleToClose handle_info) = 0;

  // Disconnect the target from CSRSS when TargetServices::LowerToken() is
  // called inside the target if supported by the OS and platform.
  virtual void SetDisconnectCsrss() = 0;

  // Specifies the desktop on which the application is going to run. The
  // requested alternate desktop must have been created via the TargetPolicy
  // interface before any processes are spawned.
  virtual void SetDesktop(Desktop desktop) = 0;

  // Sets whether or not the environment for the target will be filtered. If an
  // environment for a target is filtered, then only a fixed list of
  // environment variables will be copied from the broker to the target. These
  // are:
  //  * "Path", "SystemDrive", "SystemRoot", "TEMP", "TMP": Needed for normal
  //    operation and tests.
  //  * "LOCALAPPDATA": Needed for App Container processes.
  //  * "CHROME_CRASHPAD_PIPE_NAME": Needed for crashpad.
  virtual void SetFilterEnvironment(bool filter) = 0;

  // Obtains whether or not the environment for this target should be filtered.
  // See above for the variables that are allowed.
  virtual bool GetEnvironmentFiltered() = 0;

  // Zeroes pShimData in the child's PEB.
  virtual void SetZeroAppShim() = 0;
};

// We need [[clang::lto_visibility_public]] because instances of this class are
// passed across module boundaries. This means different modules must have
// compatible definitions of the class even when LTO is enabled.
class [[clang::lto_visibility_public]] TargetPolicy {
 public:
  virtual ~TargetPolicy() {}

  // Fetches the backing TargetConfig for this policy.
  virtual TargetConfig* GetConfig() = 0;

  // Set the handles the target process should inherit for stdout and
  // stderr.  The handles the caller passes must remain valid for the
  // lifetime of the policy object.  This only has an effect on
  // Windows Vista and later versions.  These methods accept pipe and
  // file handles, but not console handles.
  virtual ResultCode SetStdoutHandle(HANDLE handle) = 0;
  virtual ResultCode SetStderrHandle(HANDLE handle) = 0;

  // Adds a handle that will be shared with the target process. Does not take
  // ownership of the handle.
  virtual void AddHandleToShare(HANDLE handle) = 0;

  // Adds a blob of data that will be made available in the child early in
  // startup via sandbox::GetDelegateData(). The contents of this data should
  // not vary between children with the same TargetConfig().
  virtual void AddDelegateData(base::span<const uint8_t> data) = 0;
};

}  // namespace sandbox

#endif  // SANDBOX_WIN_SRC_SANDBOX_POLICY_H_
