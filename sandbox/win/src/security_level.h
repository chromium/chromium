// Copyright 2006-2008 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_WIN_SRC_SECURITY_LEVEL_H_
#define SANDBOX_WIN_SRC_SECURITY_LEVEL_H_

#include <stdint.h>

namespace sandbox {

// List of all the integrity levels supported in the sandbox.
// The integrity level of the sandboxed process can't be set to a level higher
// than the broker process.
//
// Note: These levels map to SIDs under the hood.
// INTEGRITY_LEVEL_SYSTEM:      "S-1-16-16384" System Mandatory Level
// INTEGRITY_LEVEL_HIGH:        "S-1-16-12288" High Mandatory Level
// INTEGRITY_LEVEL_MEDIUM:      "S-1-16-8192"  Medium Mandatory Level
// INTEGRITY_LEVEL_MEDIUM_LOW:  "S-1-16-6144"
// INTEGRITY_LEVEL_LOW:         "S-1-16-4096"  Low Mandatory Level
// INTEGRITY_LEVEL_BELOW_LOW:   "S-1-16-2048"
// INTEGRITY_LEVEL_UNTRUSTED:   "S-1-16-0"     Untrusted Mandatory Level
//
// Not defined:                 "S-1-16-20480" Protected Process Mandatory Level
// Not defined:                 "S-1-16-28672" Secure Process Mandatory Level
enum IntegrityLevel {
  INTEGRITY_LEVEL_SYSTEM,
  INTEGRITY_LEVEL_HIGH,
  INTEGRITY_LEVEL_MEDIUM,
  INTEGRITY_LEVEL_MEDIUM_LOW,
  INTEGRITY_LEVEL_LOW,
  INTEGRITY_LEVEL_BELOW_LOW,
  INTEGRITY_LEVEL_UNTRUSTED,
  INTEGRITY_LEVEL_LAST
};

// The Token level specifies a set of  security profiles designed to
// provide the bulk of the security of sandbox.
//
//  TokenLevel                 |Restricting   |Deny Only       |Privileges|
//                             |Sids          |Sids            |          |
// ----------------------------|--------------|----------------|----------|
// USER_LOCKDOWN               | Null Sid     | All            | None     |
// ----------------------------|--------------|----------------|----------|
// USER_LIMITED                | Users        | All except:    | Traverse |
//                             | Everyone     | Users          |          |
//                             | RESTRICTED   | Everyone       |          |
//                             |              | Interactive    |          |
// ----------------------------|--------------|----------------|----------|
// USER_INTERACTIVE            | Users        | All except:    | Traverse |
//                             | Everyone     | Users          |          |
//                             | RESTRICTED   | Everyone       |          |
//                             | Owner        | Interactive    |          |
//                             |              | Local          |          |
//                             |              | Authent-users  |          |
//                             |              | User           |          |
// ----------------------------|--------------|----------------|----------|
// USER_RESTRICTED_NON_ADMIN   | Users        | All except:    | Traverse |
//                             | Everyone     | Users          |          |
//                             | Interactive  | Everyone       |          |
//                             | Local        | Interactive    |          |
//                             | Authent-users| Local          |          |
//                             | User         | Authent-users  |          |
//                             |              | User           |          |
// ----------------------------|--------------|----------------|----------|
// USER_RESTRICTED_SAME_ACCESS | All          | None           | All      |
// ----------------------------|--------------|----------------|----------|
// USER_UNPROTECTED            | None         | None           | All      |
// ----------------------------|--------------|----------------|----------|
//
// The above restrictions are actually a transformation that is applied to
// the existing broker process token. The resulting token that will be
// applied to the target process depends both on the token level selected
// and on the broker token itself.
//
// The LOCKDOWN level is designed to allow access to almost nothing that has
// security associated with and they are the recommended levels to run sandboxed
// code specially if there is a chance that the broker is process might be
// started by a user that belongs to the Admins or power users groups.
enum TokenLevel {
  USER_LOCKDOWN = 0,
  USER_LIMITED,
  USER_INTERACTIVE,
  USER_RESTRICTED_NON_ADMIN,
  USER_RESTRICTED_SAME_ACCESS,
  USER_UNPROTECTED,
  USER_LAST
};

// The Job level specifies a set of decreasing security profiles for the
// Job object that the target process will be placed into.
// This table summarizes the security associated with each level:
//
//  JobLevel        |General                            |Quota               |
//                  |restrictions                       |restrictions        |
// -----------------|---------------------------------- |--------------------|
// kUnprotected     | None                              | *Kill on Job close.|
// -----------------|---------------------------------- |--------------------|
// kInteractive     | *Forbid system-wide changes using |                    |
//                  |  SystemParametersInfo().          | *Kill on Job close.|
//                  | *Forbid the creation/switch of    |                    |
//                  |  Desktops.                        |                    |
//                  | *Forbids calls to ExitWindows().  |                    |
// -----------------|---------------------------------- |--------------------|
// kLimitedUser     | Same as kInteractive plus:        | *One active process|
//                  | *Forbid changes to the display    |  limit.            |
//                  |  settings.                        | *Kill on Job close.|
// -----------------|---------------------------------- |--------------------|
// kLockdown        | Same as kLimitedUser plus:        | *One active process|
//                  | * No read/write to the clipboard. |  limit.            |
//                  | * No access to User Handles that  | *Kill on Job close.|
//                  |   belong to other processes.      | *Kill on unhandled |
//                  | * Forbid message broadcasts.      |  exception.        |
//                  | * Forbid setting global hooks.    |                    |
//                  | * No access to the global atoms   |                    |
//                  |   table.                          |                    |
// -----------------|-----------------------------------|--------------------|
//
// In the context of the above table, 'user handles' refers to the handles of
// windows, bitmaps, menus, etc. Files, treads and registry handles are kernel
// handles and are not affected by the job level settings.
enum class JobLevel { kLockdown = 0, kLimitedUser, kInteractive, kUnprotected };

// These flags correspond to various process-level mitigations (eg. ASLR and
// DEP). Most are implemented via UpdateProcThreadAttribute() plus flags for
// the PROC_THREAD_ATTRIBUTE_MITIGATION_POLICY attribute argument; documented
// here: http://msdn.microsoft.com/en-us/library/windows/desktop/ms686880
// Some mitigations are implemented directly by the sandbox or emulated to
// the greatest extent possible when not directly supported by the OS.
// Flags that are unsupported for the target OS will be silently ignored.
// Flags that are invalid for their application (pre or post startup) will
// return SBOX_ERROR_BAD_PARAMS.
typedef uint64_t MitigationFlags;

// Permanently enables DEP for the target process. Corresponds to
// PROCESS_CREATION_MITIGATION_POLICY_DEP_ENABLE.
const MitigationFlags MITIGATION_DEP = 0x00000001;

// Permanently Disables ATL thunk emulation when DEP is enabled. Valid
// only when MITIGATION_DEP is passed. Corresponds to not passing
// PROCESS_CREATION_MITIGATION_POLICY_DEP_ATL_THUNK_ENABLE.
const MitigationFlags MITIGATION_DEP_NO_ATL_THUNK = 0x00000002;

// Enables Structured exception handling override prevention. Must be
// enabled prior to process start. Corresponds to
// PROCESS_CREATION_MITIGATION_POLICY_SEHOP_ENABLE.
const MitigationFlags MITIGATION_SEHOP = 0x00000004;

// Forces ASLR on all images in the child process. In debug builds, must be
// enabled after startup. Corresponds to
// PROCESS_CREATION_MITIGATION_POLICY_FORCE_RELOCATE_IMAGES_ALWAYS_ON .
const MitigationFlags MITIGATION_RELOCATE_IMAGE = 0x00000008;

// Refuses to load DLLs that cannot support ASLR. In debug builds, must be
// enabled after startup. Corresponds to
// PROCESS_CREATION_MITIGATION_POLICY_FORCE_RELOCATE_IMAGES_ALWAYS_ON_REQ_RELOCS.
const MitigationFlags MITIGATION_RELOCATE_IMAGE_REQUIRED = 0x00000010;

// Terminates the process on Windows heap corruption. Coresponds to
// PROCESS_CREATION_MITIGATION_POLICY_HEAP_TERMINATE_ALWAYS_ON.
const MitigationFlags MITIGATION_HEAP_TERMINATE = 0x00000020;

// Sets a random lower bound as the minimum user address. Must be
// enabled prior to process start. On 32-bit processes this is
// emulated to a much smaller degree. Corresponds to
// PROCESS_CREATION_MITIGATION_POLICY_BOTTOM_UP_ASLR_ALWAYS_ON.
const MitigationFlags MITIGATION_BOTTOM_UP_ASLR = 0x00000040;

// Increases the randomness range of bottom-up ASLR to up to 1TB. Must be
// enabled prior to process start and with MITIGATION_BOTTOM_UP_ASLR.
// Corresponds to
// PROCESS_CREATION_MITIGATION_POLICY_HIGH_ENTROPY_ASLR_ALWAYS_ON
const MitigationFlags MITIGATION_HIGH_ENTROPY_ASLR = 0x00000080;

// Immediately raises an exception on a bad handle reference. Must be
// enabled after startup. Corresponds to
// PROCESS_CREATION_MITIGATION_POLICY_STRICT_HANDLE_CHECKS_ALWAYS_ON.
const MitigationFlags MITIGATION_STRICT_HANDLE_CHECKS = 0x00000100;

// Strengthens the DLL search order. See
// http://msdn.microsoft.com/en-us/library/windows/desktop/hh310515. In a
// component build - sets this to LOAD_LIBRARY_SEARCH_DEFAULT_DIRS allowing
// additional directories to be added via Windows AddDllDirectory() function,
// but preserving current load order. In a non-component build, all DLLs should
// be loaded manually, so strenthen to LOAD_LIBRARY_SEARCH_SYSTEM32 |
// LOAD_LIBRARY_SEARCH_USER_DIRS, removing LOAD_LIBRARY_SEARCH_APPLICATION_DIR,
// preventing DLLs being implicitly loaded from the application path. Must be
// enabled after startup.
const MitigationFlags MITIGATION_DLL_SEARCH_ORDER = 0x00000200;

// Changes the mandatory integrity level policy on the current process' token
// to enable no-read and no-execute up. This prevents a lower IL process from
// opening the process token for impersonate/duplicate/assignment.
const MitigationFlags MITIGATION_HARDEN_TOKEN_IL_POLICY = 0x00000400;

// Prevents the process from making Win32k calls. Corresponds to
// PROCESS_CREATION_MITIGATION_POLICY_WIN32K_SYSTEM_CALL_DISABLE_ALWAYS_ON.
//
// Applications linked to user32.dll or gdi32.dll make Win32k calls during
// setup, even if Win32k is not otherwise used. So they also need to add a rule
// with SUBSYS_WIN32K_LOCKDOWN and semantics FAKE_USER_GDI_INIT to allow the
// initialization to succeed.
const MitigationFlags MITIGATION_WIN32K_DISABLE = 0x00000800;

// Prevents certain built-in third party extension points from being used.
// - App_Init DLLs
// - Winsock Layered Service Providers (LSPs)
// - Global Windows Hooks (NOT thread-targeted hooks)
// - Legacy Input Method Editors (IMEs)
// I.e.: Disable legacy hooking mechanisms.  Corresponds to
// PROCESS_CREATION_MITIGATION_POLICY_EXTENSION_POINT_DISABLE_ALWAYS_ON.
const MitigationFlags MITIGATION_EXTENSION_POINT_DISABLE = 0x00001000;

// Prevents the process from generating dynamic code or modifying executable
// code. Second option to allow thread-specific opt-out.
// - VirtualAlloc with PAGE_EXECUTE_*
// - VirtualProtect with PAGE_EXECUTE_*
// - MapViewOfFile with FILE_MAP_EXECUTE | FILE_MAP_WRITE
// - SetProcessValidCallTargets for CFG
// Corresponds to
// PROCESS_CREATION_MITIGATION_POLICY_PROHIBIT_DYNAMIC_CODE_ALWAYS_ON and
// PROCESS_CREATION_MITIGATION_POLICY_PROHIBIT_DYNAMIC_CODE_ALWAYS_ON_ALLOW_OPT_OUT.
const MitigationFlags MITIGATION_DYNAMIC_CODE_DISABLE = 0x00002000;
const MitigationFlags MITIGATION_DYNAMIC_CODE_DISABLE_WITH_OPT_OUT = 0x00004000;
// The following per-thread flag can be used with the
// ApplyMitigationsToCurrentThread API.  Requires the above process mitigation
// to be set on the current process.
const MitigationFlags MITIGATION_DYNAMIC_CODE_OPT_OUT_THIS_THREAD = 0x00008000;

// Prevents the process from loading non-system fonts into GDI.
// Corresponds to
// PROCESS_CREATION_MITIGATION_POLICY_FONT_DISABLE_ALWAYS_ON
const MitigationFlags MITIGATION_NONSYSTEM_FONT_DISABLE = 0x00010000;

// Prevents the process from loading binaries NOT signed by MS.
// Corresponds to
// PROCESS_CREATION_MITIGATION_POLICY_BLOCK_NON_MICROSOFT_BINARIES_ALWAYS_ON
const MitigationFlags MITIGATION_FORCE_MS_SIGNED_BINS = 0x00020000;

// Blocks mapping of images from remote devices. Corresponds to
// PROCESS_CREATION_MITIGATION_POLICY_IMAGE_LOAD_NO_REMOTE_ALWAYS_ON.
const MitigationFlags MITIGATION_IMAGE_LOAD_NO_REMOTE = 0x00040000;

// Blocks mapping of images that have the low manditory label. Corresponds to
// PROCESS_CREATION_MITIGATION_POLICY_IMAGE_LOAD_NO_LOW_LABEL_ALWAYS_ON.
const MitigationFlags MITIGATION_IMAGE_LOAD_NO_LOW_LABEL = 0x00080000;

// Forces image load preference to prioritize the Windows install System32
// folder before dll load dir, application dir and any user dirs set.
// - Affects IAT resolution standard search path only, NOT direct LoadLibrary or
//   executable search path.
// PROCESS_CREATION_MITIGATION_POLICY_IMAGE_LOAD_PREFER_SYSTEM32_ALWAYS_ON.
const MitigationFlags MITIGATION_IMAGE_LOAD_PREFER_SYS32 = 0x00100000;

// Prevents hyperthreads from interfering with indirect branch predictions.
// (SPECTRE Variant 2 mitigation.)  Corresponds to
// PROCESS_CREATION_MITIGATION_POLICY2_RESTRICT_INDIRECT_BRANCH_PREDICTION_ALWAYS_ON.
const MitigationFlags MITIGATION_RESTRICT_INDIRECT_BRANCH_PREDICTION =
    0x00200000;

// Turns off CET for the process. This allows chrome.exe to
// be turned 'on' using IFEO or through build settings but children we know to
// have issues can be turned off. Corresponds to
// PROCESS_CREATION_MITIGATION_POLICY2_CET_USER_SHADOW_STACKS_ALWAYS_OFF.
const MitigationFlags MITIGATION_CET_DISABLED = 0x00400000;

// Enable KTM component mitigation. When enabled, it locks down all function
// calls to consume the kernel transaction manager.
const MitigationFlags MITIGATION_KTM_COMPONENT = 0x00800000;

// CET in default state (i.e. not disabled where it is supported) and
// CetDynamicApisOutOfProcOnly will be false inside the process. Should not
// be mixed with MITIGATION_CET_DISABLED or MITIGATION_DYNAMIC_CODE_DISABLE
// as it does not make sense without CET, nor where dynamic code cannot be
// created in the first place. Corresponds to
// PROCESS_CREATION_MITIGATION_POLICY2_CET_DYNAMIC_APIS_OUT_OF_PROC_ONLY_ALWAYS_OFF.
const MitigationFlags MITIGATION_CET_ALLOW_DYNAMIC_APIS = 0x01000000;

// CET in strict mode. Be cautious if applying to processes that might
// include third party code. Corresponds to
// PROCESS_CREATION_MITIGATION_POLICY2_CET_USER_SHADOW_STACKS_STRICT_MODE.
const MitigationFlags MITIGATION_CET_STRICT_MODE = 0x02000000;

// Prevents application from sending FSCTL* control codes to NtFsControlFile,
// with a few exceptions for named pipes as documented on MSDN. Corresponds to
// PROCESS_CREATION_MITIGATION_POLICY2_FSCTL_SYSTEM_CALL_DISABLE_ALWAYS_ON.
const MitigationFlags MITIGATION_FSCTL_DISABLED = 0x04000000;

// Restrict a process thread to never share a CPU core with another process
// thread outside of its security domain. Corresponds to
// PROCESS_CREATION_MITIGATION_POLICY2_RESTRICT_CORE_SHARING_ALWAYS_ON.
const MitigationFlags MITIGATION_RESTRICT_CORE_SHARING = 0x08000000;

}  // namespace sandbox

#endif  // SANDBOX_WIN_SRC_SECURITY_LEVEL_H_
