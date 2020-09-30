// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/policy/win/sandbox_win.h"

#include <stddef.h>

#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/debug/activity_tracker.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/hash/hash.h"
#include "base/hash/sha1.h"
#include "base/logging.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/stl_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "base/trace_event/trace_arguments.h"
#include "base/trace_event/trace_event.h"
#include "base/win/iat_patch_function.h"
#include "base/win/scoped_handle.h"
#include "base/win/win_util.h"
#include "base/win/windows_version.h"
#include "sandbox/policy/features.h"
#include "sandbox/policy/sandbox_type.h"
#include "sandbox/policy/switches.h"
#include "sandbox/policy/win/sandbox_diagnostics.h"
#include "sandbox/win/src/app_container_profile.h"
#include "sandbox/win/src/job.h"
#include "sandbox/win/src/process_mitigations.h"
#include "sandbox/win/src/sandbox.h"
#include "sandbox/win/src/sandbox_nt_util.h"
#include "sandbox/win/src/sandbox_policy_base.h"
#include "sandbox/win/src/sandbox_policy_diagnostic.h"
#include "sandbox/win/src/win_utils.h"

namespace sandbox {
namespace policy {
namespace {

BrokerServices* g_broker_services = NULL;

HANDLE g_job_object_handle = NULL;

// The DLLs listed here are known (or under strong suspicion) of causing crashes
// when they are loaded in the renderer. Note: at runtime we generate short
// versions of the dll name only if the dll has an extension.
// For more information about how this list is generated, and how to get off
// of it, see:
// https://sites.google.com/a/chromium.org/dev/Home/third-party-developers
const wchar_t* const kTroublesomeDlls[] = {
    L"adialhk.dll",                // Kaspersky Internet Security.
    L"acpiz.dll",                  // Unknown.
    L"activedetect32.dll",         // Lenovo One Key Theater (crbug.com/536056).
    L"activedetect64.dll",         // Lenovo One Key Theater (crbug.com/536056).
    L"airfoilinject3.dll",         // Airfoil.
    L"akinsofthook32.dll",         // Akinsoft Software Engineering.
    L"assistant_x64.dll",          // Unknown.
    L"atcuf64.dll",                // Bit Defender Internet Security x64.
    L"avcuf64.dll",                // Bit Defender Internet Security x64.
    L"avgrsstx.dll",               // AVG 8.
    L"babylonchromepi.dll",        // Babylon translator.
    L"btkeyind.dll",               // Widcomm Bluetooth.
    L"cmcsyshk.dll",               // CMC Internet Security.
    L"cmsetac.dll",                // Unknown (suspected malware).
    L"cooliris.dll",               // CoolIris.
    L"cplushook.dll",              // Unknown (suspected malware).
    L"dockshellhook.dll",          // Stardock Objectdock.
    L"easyhook32.dll",             // GDIPP and others.
    L"easyhook64.dll",             // Symantec BlueCoat and others.
    L"esspd.dll",                  // Samsung Smart Security ESCORT.
    L"googledesktopnetwork3.dll",  // Google Desktop Search v5.
    L"fwhook.dll",                 // PC Tools Firewall Plus.
    L"guard64.dll",                // Comodo Internet Security x64.
    L"hookprocesscreation.dll",    // Blumentals Program protector.
    L"hookterminateapis.dll",      // Blumentals and Cyberprinter.
    L"hookprintapis.dll",          // Cyberprinter.
    L"imon.dll",                   // NOD32 Antivirus.
    L"icatcdll.dll",               // Samsung Smart Security ESCORT.
    L"icdcnl.dll",                 // Samsung Smart Security ESCORT.
    L"ioloHL.dll",                 // Iolo (System Mechanic).
    L"kloehk.dll",                 // Kaspersky Internet Security.
    L"lawenforcer.dll",            // Spyware-Browser AntiSpyware (Spybro).
    L"libdivx.dll",                // DivX.
    L"lvprcinj01.dll",             // Logitech QuickCam.
    L"madchook.dll",               // Madshi (generic hooking library).
    L"mdnsnsp.dll",                // Bonjour.
    L"moonsysh.dll",               // Moon Secure Antivirus.
    L"mpk.dll",                    // KGB Spy.
    L"n64hooks.dll",               // Neilsen//NetRatings NetSight.
    L"npdivx32.dll",               // DivX.
    L"npggNT.des",                 // GameGuard 2008.
    L"npggNT.dll",                 // GameGuard (older).
    L"nphooks.dll",                // Neilsen//NetRatings NetSight.
    L"oawatch.dll",                // Online Armor.
    L"pastali32.dll",              // PastaLeads.
    L"pavhook.dll",                // Panda Internet Security.
    L"pavlsphook.dll",             // Panda Antivirus.
    L"pavshook.dll",               // Panda Antivirus.
    L"pavshookwow.dll",            // Panda Antivirus.
    L"pctavhook.dll",              // PC Tools Antivirus.
    L"pctgmhk.dll",                // PC Tools Spyware Doctor.
    L"picrmi32.dll",               // PicRec.
    L"picrmi64.dll",               // PicRec.
    L"prntrack.dll",               // Pharos Systems.
    L"prochook.dll",               // Unknown (GBill-Tools?) (crbug.com/974722).
    L"protector.dll",              // Unknown (suspected malware).
    L"radhslib.dll",               // Radiant Naomi Internet Filter.
    L"radprlib.dll",               // Radiant Naomi Internet Filter.
    L"rapportnikko.dll",           // Trustware Rapport.
    L"rlhook.dll",                 // Trustware Bufferzone.
    L"rooksdol.dll",               // Trustware Rapport.
    L"rndlpepperbrowserrecordhelper.dll",  // RealPlayer.
    L"rpchromebrowserrecordhelper.dll",    // RealPlayer.
    L"r3hook.dll",                         // Kaspersky Internet Security.
    L"sahook.dll",                         // McAfee Site Advisor.
    L"sbrige.dll",                         // Unknown.
    L"sc2hook.dll",                        // Supercopier 2.
    L"sdhook32.dll",             // Spybot - Search & Destroy Live Protection.
    L"sguard.dll",               // Iolo (System Guard).
    L"smum32.dll",               // Spyware Doctor version 6.
    L"smumhook.dll",             // Spyware Doctor version 5.
    L"ssldivx.dll",              // DivX.
    L"syncor11.dll",             // SynthCore Midi interface.
    L"systools.dll",             // Panda Antivirus.
    L"tfwah.dll",                // Threatfire (PC tools).
    L"wblind.dll",               // Stardock Object desktop.
    L"wbhelp.dll",               // Stardock Object desktop.
    L"windowsapihookdll32.dll",  // Lenovo One Key Theater (crbug.com/536056).
    L"windowsapihookdll64.dll",  // Lenovo One Key Theater (crbug.com/536056).
    L"winstylerthemehelper.dll"  // Tuneup utilities 2006.
};

// This is for finch. See also crbug.com/464430 for details.
const base::Feature kEnableCsrssLockdownFeature{
    "EnableCsrssLockdown", base::FEATURE_DISABLED_BY_DEFAULT};

// Helps emit trace events for sandbox policy. This mediates memory between
// chrome.exe and chrome.dll.
class PolicyTraceHelper : public base::trace_event::ConvertableToTraceFormat {
 public:
  PolicyTraceHelper(TargetPolicy* policy) {
    // |info| must live until JsonString() output is copied.
    std::unique_ptr<PolicyInfo> info = policy->GetPolicyInfo();
    json_string_ = std::string(info->JsonString());
  }
  ~PolicyTraceHelper() override = default;

  // ConvertableToTraceFormat.
  void AppendAsTraceFormat(std::string* out) const override {
    out->append(json_string_);
  }

 private:
  std::string json_string_;
};  // PolicyTraceHelper

#if !defined(NACL_WIN64)
// Adds the policy rules for the path and path\ with the semantic |access|.
// If |children| is set to true, we need to add the wildcard rules to also
// apply the rule to the subfiles and subfolders.
bool AddDirectory(int path,
                  const wchar_t* sub_dir,
                  bool children,
                  TargetPolicy::Semantics access,
                  TargetPolicy* policy) {
  base::FilePath directory;
  if (!base::PathService::Get(path, &directory))
    return false;

  if (sub_dir)
    directory = base::MakeAbsoluteFilePath(directory.Append(sub_dir));

  ResultCode result;
  result = policy->AddRule(TargetPolicy::SUBSYS_FILES, access,
                           directory.value().c_str());
  if (result != SBOX_ALL_OK)
    return false;

  std::wstring directory_str = directory.value() + L"\\";
  if (children)
    directory_str += L"*";
  // Otherwise, add the version of the path that ends with a separator.

  result = policy->AddRule(TargetPolicy::SUBSYS_FILES, access,
                           directory_str.c_str());
  if (result != SBOX_ALL_OK)
    return false;

  return true;
}
#endif  // !defined(NACL_WIN64)

// Compares the loaded |module| file name matches |module_name|.
bool IsExpandedModuleName(HMODULE module, const wchar_t* module_name) {
  wchar_t path[MAX_PATH];
  DWORD sz = ::GetModuleFileNameW(module, path, base::size(path));
  if ((sz == base::size(path)) || (sz == 0)) {
    // XP does not set the last error properly, so we bail out anyway.
    return false;
  }
  if (!::GetLongPathName(path, path, base::size(path)))
    return false;
  base::FilePath fname(path);
  return (fname.BaseName().value() == module_name);
}

std::vector<std::wstring> GetShortNameVariants(const std::wstring& name) {
  std::vector<std::wstring> alt_names;
  size_t period = name.rfind(L'.');
  DCHECK_NE(std::string::npos, period);
  DCHECK_LE(3U, (name.size() - period));
  if (period <= 8)
    return alt_names;

  // The module could have been loaded with a 8.3 short name. We check
  // the three most common cases: 'thelongname.dll' becomes
  // 'thelon~1.dll', 'thelon~2.dll' and 'thelon~3.dll'.
  alt_names.reserve(3);
  for (wchar_t ix = '1'; ix <= '3'; ++ix) {
    const wchar_t suffix[] = {'~', ix, 0};
    alt_names.push_back(
        base::StrCat({name.substr(0, 6), suffix, name.substr(period)}));
  }
  return alt_names;
}

// Adds a single dll by |module_name| into the |policy| blocklist.
// If |check_in_browser| is true we only add an unload policy only if the dll
// is also loaded in this process.
void BlocklistAddOneDll(const wchar_t* module_name,
                        bool check_in_browser,
                        TargetPolicy* policy) {
  if (check_in_browser) {
    HMODULE module = ::GetModuleHandleW(module_name);
    if (module) {
      policy->AddDllToUnload(module_name);
      DVLOG(1) << "dll to unload found: " << module_name;
    } else {
      for (const auto& alt_name : GetShortNameVariants(module_name)) {
        module = ::GetModuleHandleW(alt_name.c_str());
        // We found it, but because it only has 6 significant letters, we
        // want to make sure it is the right one.
        if (module && IsExpandedModuleName(module, module_name)) {
          // Found a match. We add both forms to the policy.
          policy->AddDllToUnload(alt_name.c_str());
          policy->AddDllToUnload(module_name);
          return;
        }
      }
    }
  } else {
    policy->AddDllToUnload(module_name);
    for (const auto& alt_name : GetShortNameVariants(module_name)) {
      policy->AddDllToUnload(alt_name.c_str());
    }
  }
}

// Adds policy rules for unloaded the known dlls that cause chrome to crash.
// Eviction of injected DLLs is done by the sandbox so that the injected module
// does not get a chance to execute any code.
void AddGenericDllEvictionPolicy(TargetPolicy* policy) {
  for (int ix = 0; ix != base::size(kTroublesomeDlls); ++ix)
    BlocklistAddOneDll(kTroublesomeDlls[ix], true, policy);
}

// Returns the object path prepended with the current logon session.
base::string16 PrependWindowsSessionPath(const base::char16* object) {
  // Cache this because it can't change after process creation.
  static DWORD s_session_id = 0;
  if (s_session_id == 0) {
    HANDLE token;
    DWORD session_id_length;
    DWORD session_id = 0;

    CHECK(::OpenProcessToken(::GetCurrentProcess(), TOKEN_QUERY, &token));
    CHECK(::GetTokenInformation(token, TokenSessionId, &session_id,
                                sizeof(session_id), &session_id_length));
    CloseHandle(token);
    if (session_id)
      s_session_id = session_id;
  }

  return base::StringPrintf(L"\\Sessions\\%lu%ls", s_session_id, object);
}

// Checks if the sandbox can be let to run without a job object assigned.
// Returns true if the job object has to be applied to the sandbox and false
// otherwise.
bool ShouldSetJobLevel(const base::CommandLine& cmd_line) {
  // Windows 8 allows nested jobs so we don't need to check if we are in other
  // job.
  if (base::win::GetVersion() >= base::win::Version::WIN8)
    return true;

  BOOL in_job = true;
  // Either there is no job yet associated so we must add our job,
  if (!::IsProcessInJob(::GetCurrentProcess(), NULL, &in_job))
    NOTREACHED() << "IsProcessInJob failed. " << GetLastError();
  if (!in_job)
    return true;

  // ...or there is a job but the JOB_OBJECT_LIMIT_BREAKAWAY_OK limit is set.
  JOBOBJECT_EXTENDED_LIMIT_INFORMATION job_info = {};
  if (!::QueryInformationJobObject(NULL, JobObjectExtendedLimitInformation,
                                   &job_info, sizeof(job_info), NULL)) {
    NOTREACHED() << "QueryInformationJobObject failed. " << GetLastError();
    return true;
  }
  if (job_info.BasicLimitInformation.LimitFlags & JOB_OBJECT_LIMIT_BREAKAWAY_OK)
    return true;

  // Lastly in place of the flag which was supposed to be used only for running
  // Chrome in remote sessions we do this check explicitly here.
  // According to MS this flag can be false for a remote session only on Windows
  // Server 2012 and newer so if we do the check last we should be on the safe
  // side. See: https://msdn.microsoft.com/en-us/library/aa380798.aspx.
  if (!::GetSystemMetrics(SM_REMOTESESSION)) {
    // TODO(pastarmovj): Even though the number are low, this flag is still
    // necessary in some limited set of cases. Remove it once Windows 7 is no
    // longer supported together with the rest of the checks in this function.
    return !cmd_line.HasSwitch(switches::kAllowNoSandboxJob);
  }

  // Allow running without the sandbox in this case. This slightly reduces the
  // ability of the sandbox to protect its children from spawning new processes
  // or preventing them from shutting down Windows or accessing the clipboard.
  return false;
}

// Adds the generic policy rules to a sandbox TargetPolicy.
ResultCode AddGenericPolicy(sandbox::TargetPolicy* policy) {
  ResultCode result;

  // Add the policy for the client side of a pipe. It is just a file
  // in the \pipe\ namespace. We restrict it to pipes that start with
  // "chrome." so the sandboxed process cannot connect to system services.
  result =
      policy->AddRule(TargetPolicy::SUBSYS_FILES, TargetPolicy::FILES_ALLOW_ANY,
                      L"\\??\\pipe\\chrome.*");
  if (result != SBOX_ALL_OK)
    return result;

  // Allow the server side of sync sockets, which are pipes that have
  // the "chrome.sync" namespace and a randomly generated suffix.
  result = policy->AddRule(TargetPolicy::SUBSYS_NAMED_PIPES,
                           TargetPolicy::NAMEDPIPES_ALLOW_ANY,
                           L"\\\\.\\pipe\\chrome.sync.*");
  if (result != SBOX_ALL_OK)
    return result;

// Add the policy for debug message only in debug
#ifndef NDEBUG
  base::FilePath app_dir;
  if (!base::PathService::Get(base::DIR_MODULE, &app_dir))
    return SBOX_ERROR_GENERIC;

  wchar_t long_path_buf[MAX_PATH];
  DWORD long_path_return_value =
      GetLongPathName(app_dir.value().c_str(), long_path_buf, MAX_PATH);
  if (long_path_return_value == 0 || long_path_return_value >= MAX_PATH)
    return SBOX_ERROR_NO_SPACE;

  base::FilePath debug_message(long_path_buf);
  debug_message = debug_message.AppendASCII("debug_message.exe");
  result = policy->AddRule(TargetPolicy::SUBSYS_PROCESS,
                           TargetPolicy::PROCESS_MIN_EXEC,
                           debug_message.value().c_str());
  if (result != SBOX_ALL_OK)
    return result;
#endif  // NDEBUG

// Add the policy for read-only PDB file access for stack traces.
#if !defined(OFFICIAL_BUILD)
  base::FilePath exe;
  if (!base::PathService::Get(base::FILE_EXE, &exe))
    return SBOX_ERROR_GENERIC;
  base::FilePath pdb_path = exe.DirName().Append(L"*.pdb");
  result = policy->AddRule(TargetPolicy::SUBSYS_FILES,
                           TargetPolicy::FILES_ALLOW_READONLY,
                           pdb_path.value().c_str());
  if (result != SBOX_ALL_OK)
    return result;
#endif

#if defined(SANITIZER_COVERAGE)
  DWORD coverage_dir_size =
      ::GetEnvironmentVariable(L"SANITIZER_COVERAGE_DIR", NULL, 0);
  if (coverage_dir_size == 0) {
    LOG(WARNING) << "SANITIZER_COVERAGE_DIR was not set, coverage won't work.";
  } else {
    std::wstring coverage_dir;
    wchar_t* coverage_dir_str =
        base::WriteInto(&coverage_dir, coverage_dir_size);
    coverage_dir_size = ::GetEnvironmentVariable(
        L"SANITIZER_COVERAGE_DIR", coverage_dir_str, coverage_dir_size);
    CHECK(coverage_dir.size() == coverage_dir_size);
    base::FilePath sancov_path =
        base::FilePath(coverage_dir).Append(L"*.sancov");
    result = policy->AddRule(TargetPolicy::SUBSYS_FILES,
                             TargetPolicy::FILES_ALLOW_ANY,
                             sancov_path.value().c_str());
    if (result != SBOX_ALL_OK)
      return result;
  }
#endif

  AddGenericDllEvictionPolicy(policy);
  return SBOX_ALL_OK;
}

void LogLaunchWarning(ResultCode last_warning, DWORD last_error) {
  base::UmaHistogramSparse("Process.Sandbox.Launch.WarningResultCode",
                           last_warning);
  base::UmaHistogramSparse("Process.Sandbox.Launch.Warning", last_error);
}

ResultCode AddPolicyForSandboxedProcess(TargetPolicy* policy) {
  ResultCode result = sandbox::SBOX_ALL_OK;

  // Win8+ adds a device DeviceApi that we don't need.
  if (base::win::GetVersion() >= base::win::Version::WIN8)
    result = policy->AddKernelObjectToClose(L"File", L"\\Device\\DeviceApi");
  if (result != SBOX_ALL_OK)
    return result;

  // On 2003/Vista+ the initial token has to be restricted if the main
  // token is restricted.
  result = policy->SetTokenLevel(USER_RESTRICTED_SAME_ACCESS, USER_LOCKDOWN);
  if (result != SBOX_ALL_OK)
    return result;
  // Prevents the renderers from manipulating low-integrity processes.
  result = policy->SetDelayedIntegrityLevel(INTEGRITY_LEVEL_UNTRUSTED);
  if (result != SBOX_ALL_OK)
    return result;
  result = policy->SetIntegrityLevel(INTEGRITY_LEVEL_LOW);
  if (result != SBOX_ALL_OK)
    return result;
  policy->SetLockdownDefaultDacl();

  result = policy->SetAlternateDesktop(true);
  if (result != SBOX_ALL_OK) {
    // We ignore the result of setting the alternate desktop, however log
    // a launch warning.
    LogLaunchWarning(result, ::GetLastError());
    DLOG(WARNING) << "Failed to apply desktop security to the renderer";
    result = SBOX_ALL_OK;
  }

  return result;
}

// This code is test only, and attempts to catch unsafe uses of
// DuplicateHandle() that copy privileged handles into sandboxed processes.
#if !defined(OFFICIAL_BUILD) && !defined(COMPONENT_BUILD)
base::win::IATPatchFunction& GetIATPatchFunctionHandle() {
  static base::NoDestructor<base::win::IATPatchFunction>
      iat_patch_duplicate_handle;
  return *iat_patch_duplicate_handle;
}

typedef BOOL(WINAPI* DuplicateHandleFunctionPtr)(HANDLE source_process_handle,
                                                 HANDLE source_handle,
                                                 HANDLE target_process_handle,
                                                 LPHANDLE target_handle,
                                                 DWORD desired_access,
                                                 BOOL inherit_handle,
                                                 DWORD options);

DuplicateHandleFunctionPtr g_iat_orig_duplicate_handle;

NtQueryObject g_QueryObject = NULL;

static const char* kDuplicateHandleWarning =
    "You are attempting to duplicate a privileged handle into a sandboxed"
    " process.\n Please contact security@chromium.org for assistance.";

void CheckDuplicateHandle(HANDLE handle) {
  // Get the object type (32 characters is safe; current max is 14).
  BYTE buffer[sizeof(OBJECT_TYPE_INFORMATION) + 32 * sizeof(wchar_t)];
  OBJECT_TYPE_INFORMATION* type_info =
      reinterpret_cast<OBJECT_TYPE_INFORMATION*>(buffer);
  ULONG size = sizeof(buffer) - sizeof(wchar_t);
  NTSTATUS error;
  error = g_QueryObject(handle, ObjectTypeInformation, type_info, size, &size);
  CHECK(NT_SUCCESS(error));
  type_info->Name.Buffer[type_info->Name.Length / sizeof(wchar_t)] = L'\0';

  // Get the object basic information.
  OBJECT_BASIC_INFORMATION basic_info;
  size = sizeof(basic_info);
  error =
      g_QueryObject(handle, ObjectBasicInformation, &basic_info, size, &size);
  CHECK(NT_SUCCESS(error));

  CHECK(!(basic_info.GrantedAccess & WRITE_DAC)) << kDuplicateHandleWarning;

  if (0 == _wcsicmp(type_info->Name.Buffer, L"Process")) {
    const ACCESS_MASK kDangerousMask =
        ~static_cast<DWORD>(PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE);
    CHECK(!(basic_info.GrantedAccess & kDangerousMask))
        << kDuplicateHandleWarning;
  }
}

BOOL WINAPI DuplicateHandlePatch(HANDLE source_process_handle,
                                 HANDLE source_handle,
                                 HANDLE target_process_handle,
                                 LPHANDLE target_handle,
                                 DWORD desired_access,
                                 BOOL inherit_handle,
                                 DWORD options) {
  // Duplicate the handle so we get the final access mask.
  if (!g_iat_orig_duplicate_handle(source_process_handle, source_handle,
                                   target_process_handle, target_handle,
                                   desired_access, inherit_handle, options))
    return FALSE;

  // We're not worried about broker handles or not crossing process boundaries.
  if (source_process_handle == target_process_handle ||
      target_process_handle == ::GetCurrentProcess())
    return TRUE;

  // Only sandboxed children are placed in jobs, so just check them.
  BOOL is_in_job = FALSE;
  if (!::IsProcessInJob(target_process_handle, NULL, &is_in_job)) {
    // We need a handle with permission to check the job object.
    if (ERROR_ACCESS_DENIED == ::GetLastError()) {
      HANDLE temp_handle;
      CHECK(g_iat_orig_duplicate_handle(
          ::GetCurrentProcess(), target_process_handle, ::GetCurrentProcess(),
          &temp_handle, PROCESS_QUERY_INFORMATION, FALSE, 0));
      base::win::ScopedHandle process(temp_handle);
      CHECK(::IsProcessInJob(process.Get(), NULL, &is_in_job));
    }
  }

  if (is_in_job) {
    // We never allow inheritable child handles.
    CHECK(!inherit_handle) << kDuplicateHandleWarning;

    // Duplicate the handle again, to get the final permissions.
    HANDLE temp_handle;
    CHECK(g_iat_orig_duplicate_handle(target_process_handle, *target_handle,
                                      ::GetCurrentProcess(), &temp_handle, 0,
                                      FALSE, DUPLICATE_SAME_ACCESS));
    base::win::ScopedHandle handle(temp_handle);

    // Callers use CHECK macro to make sure we get the right stack.
    CheckDuplicateHandle(handle.Get());
  }

  return TRUE;
}
#endif

bool IsAppContainerEnabled() {
  if (base::win::GetVersion() < base::win::Version::WIN8)
    return false;

  return base::FeatureList::IsEnabled(
      {"RendererAppContainer", base::FEATURE_DISABLED_BY_DEFAULT});
}

ResultCode SetJobMemoryLimit(const base::CommandLine& cmd_line,
                             TargetPolicy* policy) {
  DCHECK_NE(policy->GetJobLevel(), JOB_NONE);

#ifdef _WIN64
  size_t memory_limit = static_cast<size_t>(kDataSizeLimit);

  // Note that this command line flag hasn't been fetched by all
  // callers of SetJobLevel, only those in this file.
  SandboxType sandbox_type = SandboxTypeFromCommandLine(cmd_line);
  if (sandbox_type == SandboxType::kGpu ||
      sandbox_type == SandboxType::kRenderer) {
    int64_t GB = 1024 * 1024 * 1024;
    // Allow the GPU/RENDERER process's sandbox to access more physical memory
    // if it's available on the system.
    //
    // Renderer processes are allowed to access 16 GB; the GPU process, up
    // to 64 GB.
    int64_t physical_memory = base::SysInfo::AmountOfPhysicalMemory();
    if (sandbox_type == SandboxType::kGpu && physical_memory > 64 * GB) {
      memory_limit = 64 * GB;
    } else if (sandbox_type == SandboxType::kGpu && physical_memory > 32 * GB) {
      memory_limit = 32 * GB;
    } else if (physical_memory > 16 * GB) {
      memory_limit = 16 * GB;
    } else if (physical_memory > 8 * GB) {
      memory_limit = 8 * GB;
    }
  }
  return policy->SetJobMemoryLimit(memory_limit);
#else
  return SBOX_ALL_OK;
#endif
}

// Generate a unique sandbox AC profile for the appcontainer based on the SHA1
// hash of the appcontainer_id. This does not need to be secure so using SHA1
// isn't a security concern.
base::string16 GetAppContainerProfileName(const std::string& appcontainer_id,
                                          SandboxType sandbox_type) {
  std::string sandbox_base_name;
  switch (sandbox_type) {
    case SandboxType::kXrCompositing:
      sandbox_base_name = std::string("cr.sb.xr");
      break;
    case SandboxType::kGpu:
      sandbox_base_name = std::string("cr.sb.gpu");
      break;
    case SandboxType::kMediaFoundationCdm:
      sandbox_base_name = std::string("cr.sb.cdm");
      break;
    default:
      DCHECK(0);
  }

  auto sha1 = base::SHA1HashString(appcontainer_id);
  std::string profile_name = base::StrCat(
      {sandbox_base_name, base::HexEncode(sha1.data(), sha1.size())});
  // CreateAppContainerProfile requires that the profile name is at most 64
  // characters but 50 on WCOS systems.  The size of sha1 is a constant 40,
  // so validate that the base names are sufficiently short that the total
  // length is valid on all systems.
  DCHECK_LE(profile_name.length(), 50U);
  return base::UTF8ToWide(profile_name);
}

ResultCode SetupAppContainerProfile(AppContainerProfile* profile,
                                    const base::CommandLine& command_line,
                                    SandboxType sandbox_type) {
  if (sandbox_type != SandboxType::kMediaFoundationCdm &&
      sandbox_type != SandboxType::kGpu &&
      sandbox_type != SandboxType::kXrCompositing)
    return SBOX_ERROR_UNSUPPORTED;

  if (sandbox_type == SandboxType::kGpu &&
      !profile->AddImpersonationCapability(L"chromeInstallFiles")) {
    DLOG(ERROR) << "AppContainerProfile::AddImpersonationCapability("
                   "chromeInstallFiles) failed";
    return SBOX_ERROR_CREATE_APPCONTAINER_PROFILE_CAPABILITY;
  }

  if ((sandbox_type == SandboxType::kXrCompositing ||
       sandbox_type == SandboxType::kGpu) &&
      !profile->AddCapability(L"lpacPnpNotifications")) {
    DLOG(ERROR)
        << "AppContainerProfile::AddCapability(lpacPnpNotifications) failed";
    return SBOX_ERROR_CREATE_APPCONTAINER_PROFILE_CAPABILITY;
  }

  if (sandbox_type == SandboxType::kXrCompositing &&
      !profile->AddCapability(L"chromeInstallFiles")) {
    DLOG(ERROR)
        << "AppContainerProfile::AddCapability(chromeInstallFiles) failed";
    return SBOX_ERROR_CREATE_APPCONTAINER_PROFILE_CAPABILITY;
  }

  if (sandbox_type == SandboxType::kMediaFoundationCdm) {
    // Please refer to the following design doc on why we add the capabilities:
    // https://docs.google.com/document/d/19Y4Js5v3BlzA5uSuiVTvcvPNIOwmxcMSFJWtuc1A-w8/edit#heading=h.iqvhsrml3gl9
    if (!profile->AddCapability(
            sandbox::WellKnownCapabilities::kPrivateNetworkClientServer) ||
        !profile->AddCapability(
            sandbox::WellKnownCapabilities::kInternetClient)) {
      DLOG(ERROR)
          << "AppContainerProfile::AddCapability() - "
          << "SandboxType::kMediaFoundationCdm internet capabilities failed";
      return sandbox::SBOX_ERROR_CREATE_APPCONTAINER_PROFILE_CAPABILITY;
    }

    if (!profile->AddCapability(L"lpacCom") ||
        !profile->AddCapability(L"lpacIdentityServices") ||
        !profile->AddCapability(L"lpacMedia") ||
        !profile->AddCapability(L"lpacPnPNotifications") ||
        !profile->AddCapability(L"lpacServicesManagement") ||
        !profile->AddCapability(L"lpacSessionManagement") ||
        !profile->AddCapability(L"lpacAppExperience") ||
        !profile->AddCapability(L"lpacAppServices") ||
        !profile->AddCapability(L"lpacCryptoServices") ||
        !profile->AddCapability(L"lpacEnterprisePolicyChangeNotifications")) {
      DLOG(ERROR)
          << "AppContainerProfile::AddCapability() - "
          << "SandboxType::kMediaFoundationCdm lpac capabilities failed";
      return sandbox::SBOX_ERROR_CREATE_APPCONTAINER_PROFILE_CAPABILITY;
    }
  }

  std::vector<base::string16> base_caps = {
      L"lpacChromeInstallFiles",
      L"registryRead",
  };

  if (sandbox_type == SandboxType::kGpu) {
    auto cmdline_caps = base::SplitString(
        command_line.GetSwitchValueNative(switches::kAddGpuAppContainerCaps),
        L",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    base_caps.insert(base_caps.end(), cmdline_caps.begin(), cmdline_caps.end());
  }

  if (sandbox_type == SandboxType::kXrCompositing) {
    auto cmdline_caps = base::SplitString(
        command_line.GetSwitchValueNative(switches::kAddXrAppContainerCaps),
        L",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    base_caps.insert(base_caps.end(), cmdline_caps.begin(), cmdline_caps.end());
  }

  for (const auto& cap : base_caps) {
    if (!profile->AddCapability(cap.c_str())) {
      DLOG(ERROR) << "AppContainerProfile::AddCapability() failed";
      return SBOX_ERROR_CREATE_APPCONTAINER_PROFILE_CAPABILITY;
    }
  }

  // Enable LPAC for GPU process, but not for XRCompositor service.
  if (sandbox_type == SandboxType::kGpu &&
      base::FeatureList::IsEnabled(features::kGpuLPAC)) {
    profile->SetEnableLowPrivilegeAppContainer(true);
  }

  if (sandbox_type == SandboxType::kMediaFoundationCdm)
    profile->SetEnableLowPrivilegeAppContainer(true);

  return SBOX_ALL_OK;
}

}  // namespace

// static
ResultCode SandboxWin::SetJobLevel(const base::CommandLine& cmd_line,
                                   JobLevel job_level,
                                   uint32_t ui_exceptions,
                                   TargetPolicy* policy) {
  if (!ShouldSetJobLevel(cmd_line))
    return policy->SetJobLevel(JOB_NONE, 0);

  ResultCode ret = policy->SetJobLevel(job_level, ui_exceptions);
  if (ret != SBOX_ALL_OK)
    return ret;

  return SetJobMemoryLimit(cmd_line, policy);
}

// TODO(jschuh): Need get these restrictions applied to NaCl and Pepper.
// Just have to figure out what needs to be warmed up first.
// static
ResultCode SandboxWin::AddBaseHandleClosePolicy(TargetPolicy* policy) {
  if (base::FeatureList::IsEnabled(kEnableCsrssLockdownFeature)) {
    // Close all ALPC ports.
    ResultCode ret = policy->SetDisconnectCsrss();
    if (ret != SBOX_ALL_OK) {
      return ret;
    }
  }

  // TODO(cpu): Add back the BaseNamedObjects policy.
  base::string16 object_path = PrependWindowsSessionPath(
      L"\\BaseNamedObjects\\windows_shell_global_counters");
  return policy->AddKernelObjectToClose(L"Section", object_path.data());
}

// static
ResultCode SandboxWin::AddAppContainerPolicy(TargetPolicy* policy,
                                             const wchar_t* sid) {
  if (IsAppContainerEnabled())
    return policy->SetLowBox(sid);
  return SBOX_ALL_OK;
}

// static
ResultCode SandboxWin::AddWin32kLockdownPolicy(TargetPolicy* policy,
                                               bool enable_opm) {
#if !defined(NACL_WIN64)
  // Win32k Lockdown is supported on Windows 8+.
  if (base::win::GetVersion() < base::win::Version::WIN8)
    return SBOX_ALL_OK;

  MitigationFlags flags = policy->GetProcessMitigations();
  // Check not enabling twice. Should not happen.
  DCHECK_EQ(0U, flags & MITIGATION_WIN32K_DISABLE);

  flags |= MITIGATION_WIN32K_DISABLE;
  ResultCode result = policy->SetProcessMitigations(flags);
  if (result != SBOX_ALL_OK)
    return result;

  result = policy->AddRule(TargetPolicy::SUBSYS_WIN32K_LOCKDOWN,
                           enable_opm ? TargetPolicy::IMPLEMENT_OPM_APIS
                                      : TargetPolicy::FAKE_USER_GDI_INIT,
                           nullptr);
  if (result != SBOX_ALL_OK)
    return result;
  if (enable_opm)
    policy->SetEnableOPMRedirection();

  return result;
#else
  return SBOX_ALL_OK;
#endif
}

// static
ResultCode SandboxWin::AddAppContainerProfileToPolicy(
    const base::CommandLine& command_line,
    SandboxType sandbox_type,
    const std::string& appcontainer_id,
    TargetPolicy* policy) {
  if (base::win::GetVersion() < base::win::Version::WIN10_RS1)
    return SBOX_ALL_OK;
  base::string16 profile_name =
      GetAppContainerProfileName(appcontainer_id, sandbox_type);
  ResultCode result =
      policy->AddAppContainerProfile(profile_name.c_str(), true);
  if (result != SBOX_ALL_OK)
    return result;

  scoped_refptr<AppContainerProfile> profile = policy->GetAppContainerProfile();
  result = SetupAppContainerProfile(profile.get(), command_line, sandbox_type);
  if (result != SBOX_ALL_OK)
    return result;

  DWORD granted_access;
  BOOL granted_access_status;
  bool access_check =
      profile->AccessCheck(command_line.GetProgram().value().c_str(),
                           SE_FILE_OBJECT, GENERIC_READ | GENERIC_EXECUTE,
                           &granted_access, &granted_access_status) &&
      granted_access_status;
  if (!access_check)
    return SBOX_ERROR_CREATE_APPCONTAINER_PROFILE_ACCESS_CHECK;

  return SBOX_ALL_OK;
}

// static
bool SandboxWin::IsAppContainerEnabledForSandbox(
    const base::CommandLine& command_line,
    SandboxType sandbox_type) {
  if (base::win::GetVersion() < base::win::Version::WIN10_RS1)
    return false;

  if (sandbox_type == SandboxType::kMediaFoundationCdm)
    return true;

  if (sandbox_type != SandboxType::kGpu)
    return false;
  return base::FeatureList::IsEnabled(features::kGpuAppContainer);
}

// static
bool SandboxWin::InitBrokerServices(BrokerServices* broker_services) {
  // TODO(abarth): DCHECK(CalledOnValidThread());
  //               See <http://b/1287166>.
  DCHECK(broker_services);
  DCHECK(!g_broker_services);
  ResultCode result = broker_services->Init();
  g_broker_services = broker_services;

// In non-official builds warn about dangerous uses of DuplicateHandle. This
// isn't useful under a component build, since there will be multiple modules,
// each of which may have a slot to patch (if the symbol is even present).
#if !defined(OFFICIAL_BUILD) && !defined(COMPONENT_BUILD)
  BOOL is_in_job = FALSE;
  CHECK(::IsProcessInJob(::GetCurrentProcess(), NULL, &is_in_job));
  if (!is_in_job && !GetIATPatchFunctionHandle().is_patched()) {
    HMODULE module = NULL;
    wchar_t module_name[MAX_PATH];
    CHECK(::GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                              reinterpret_cast<LPCWSTR>(InitBrokerServices),
                              &module));
    DWORD result = ::GetModuleFileNameW(module, module_name, MAX_PATH);
    if (result && (result != MAX_PATH)) {
      ResolveNTFunctionPtr("NtQueryObject", &g_QueryObject);
      result = GetIATPatchFunctionHandle().Patch(
          module_name, "kernel32.dll", "DuplicateHandle",
          reinterpret_cast<void*>(DuplicateHandlePatch));
      CHECK_EQ(0u, result);
      g_iat_orig_duplicate_handle =
          reinterpret_cast<DuplicateHandleFunctionPtr>(
              GetIATPatchFunctionHandle().original_function());
    }
  }
#endif

  return SBOX_ALL_OK == result;
}

// static
bool SandboxWin::InitTargetServices(TargetServices* target_services) {
  DCHECK(target_services);
  ResultCode result = target_services->Init();
  return SBOX_ALL_OK == result;
}

// static
ResultCode SandboxWin::StartSandboxedProcess(
    base::CommandLine* cmd_line,
    const std::string& process_type,
    const base::HandlesToInheritVector& handles_to_inherit,
    SandboxDelegate* delegate,
    base::Process* process) {
  const base::CommandLine& launcher_process_command_line =
      *base::CommandLine::ForCurrentProcess();

  // Propagate the --allow-no-job flag if present.
  if (launcher_process_command_line.HasSwitch(switches::kAllowNoSandboxJob) &&
      !cmd_line->HasSwitch(switches::kAllowNoSandboxJob)) {
    cmd_line->AppendSwitch(switches::kAllowNoSandboxJob);
  }

  SandboxType sandbox_type = delegate->GetSandboxType();
  if (IsUnsandboxedSandboxType(sandbox_type) ||
      cmd_line->HasSwitch(switches::kNoSandbox) ||
      launcher_process_command_line.HasSwitch(switches::kNoSandbox)) {
    base::LaunchOptions options;
    options.handles_to_inherit = handles_to_inherit;
    BOOL in_job = true;
    // Prior to Windows 8 nested jobs aren't possible.
    if (sandbox_type == SandboxType::kNetwork &&
        (base::win::GetVersion() >= base::win::Version::WIN8 ||
         (::IsProcessInJob(::GetCurrentProcess(), nullptr, &in_job) &&
          !in_job))) {
      // Launch the process in a job to ensure that the network process doesn't
      // outlive the browser. This could happen if there is a lot of I/O on
      // process shutdown, in which case TerminateProcess would fail.
      // https://crbug.com/820996
      if (!g_job_object_handle) {
        Job job_obj;
        DWORD result = job_obj.Init(JOB_UNPROTECTED, nullptr, 0, 0);
        if (result != ERROR_SUCCESS)
          return SBOX_ERROR_CANNOT_INIT_JOB;
        g_job_object_handle = job_obj.Take().Take();
      }
      options.job_handle = g_job_object_handle;
    }
    *process = base::LaunchProcess(*cmd_line, options);
    return SBOX_ALL_OK;
  }

  scoped_refptr<TargetPolicy> policy = g_broker_services->CreatePolicy();

  // Add any handles to be inherited to the policy.
  for (HANDLE handle : handles_to_inherit)
    policy->AddHandleToShare(handle);

  // Pre-startup mitigations.
  MitigationFlags mitigations =
      MITIGATION_HEAP_TERMINATE |
      MITIGATION_BOTTOM_UP_ASLR |
      MITIGATION_DEP |
      MITIGATION_DEP_NO_ATL_THUNK |
      MITIGATION_EXTENSION_POINT_DISABLE |
      MITIGATION_SEHOP |
      MITIGATION_NONSYSTEM_FONT_DISABLE |
      MITIGATION_IMAGE_LOAD_NO_REMOTE |
      MITIGATION_IMAGE_LOAD_NO_LOW_LABEL |
      MITIGATION_RESTRICT_INDIRECT_BRANCH_PREDICTION;

  ResultCode result = policy->SetProcessMitigations(mitigations);
  if (result != SBOX_ALL_OK)
    return result;

  if (process_type == switches::kRendererProcess) {
    result = SandboxWin::AddWin32kLockdownPolicy(policy.get(), false);
    if (result != SBOX_ALL_OK)
      return result;
  }

  // Post-startup mitigations.
  mitigations = MITIGATION_DLL_SEARCH_ORDER;
  if (!cmd_line->HasSwitch(switches::kAllowThirdPartyModules) &&
      sandbox_type != SandboxType::kSpeechRecognition) {
    mitigations |= MITIGATION_FORCE_MS_SIGNED_BINS;
  }

  if (sandbox_type == SandboxType::kNetwork ||
      sandbox_type == SandboxType::kAudio ||
      sandbox_type == SandboxType::kIconReader) {
    mitigations |= MITIGATION_DYNAMIC_CODE_DISABLE;
  }
  // TODO(wfh): Relax strict handle checks for network process until root cause
  // for this crash can be resolved. See https://crbug.com/939590.
  if (sandbox_type != SandboxType::kNetwork)
    mitigations |= MITIGATION_STRICT_HANDLE_CHECKS;

  result = policy->SetDelayedProcessMitigations(mitigations);
  if (result != SBOX_ALL_OK)
    return result;

  result = SetJobLevel(*cmd_line, JOB_LOCKDOWN, 0, policy.get());
  if (result != SBOX_ALL_OK)
    return result;

  if (!delegate->DisableDefaultPolicy()) {
    result = AddPolicyForSandboxedProcess(policy.get());
    if (result != SBOX_ALL_OK)
      return result;
  }

  if (process_type == switches::kGpuProcess &&
      base::FeatureList::IsEnabled(
          {"GpuLockdownDefaultDacl", base::FEATURE_ENABLED_BY_DEFAULT})) {
    policy->SetLockdownDefaultDacl();
    policy->AddRestrictingRandomSid();
  }

#if !defined(NACL_WIN64)
  if (process_type == switches::kRendererProcess ||
      process_type == switches::kPpapiPluginProcess ||
      sandbox_type == SandboxType::kPrintCompositor) {
    AddDirectory(base::DIR_WINDOWS_FONTS, NULL, true,
                 TargetPolicy::FILES_ALLOW_READONLY, policy.get());
  }
#endif

  result = AddGenericPolicy(policy.get());
  if (result != SBOX_ALL_OK) {
    NOTREACHED();
    return result;
  }

  std::string appcontainer_id;
  if (IsAppContainerEnabledForSandbox(*cmd_line, sandbox_type) &&
      delegate->GetAppContainerId(&appcontainer_id)) {
    result = AddAppContainerProfileToPolicy(*cmd_line, sandbox_type,
                                            appcontainer_id, policy.get());
    DCHECK(result == SBOX_ALL_OK);
    if (result != SBOX_ALL_OK)
      return result;
  }

  // Allow the renderer, gpu and utility processes to access the log file.
  if (process_type == switches::kRendererProcess ||
      process_type == switches::kGpuProcess ||
      process_type == switches::kUtilityProcess) {
    if (logging::IsLoggingToFileEnabled()) {
      DCHECK(base::FilePath(logging::GetLogFileFullPath()).IsAbsolute());
      result = policy->AddRule(TargetPolicy::SUBSYS_FILES,
                               TargetPolicy::FILES_ALLOW_ANY,
                               logging::GetLogFileFullPath().c_str());
      if (result != SBOX_ALL_OK)
        return result;
    }
  }

  if (sandbox_type == SandboxType::kMediaFoundationCdm) {
    // Set a policy that would normally allow for process creation. This allows
    // the mf cdm process to launch the protected media pipeline process
    // (mfpmp.exe) without process interception.
    result = policy->SetJobLevel(JOB_INTERACTIVE, 0);
    if (result != SBOX_ALL_OK)
      return result;
  }

#if !defined(OFFICIAL_BUILD)
  // If stdout/stderr point to a Windows console, these calls will
  // have no effect. These calls can fail with SBOX_ERROR_BAD_PARAMS.
  policy->SetStdoutHandle(GetStdHandle(STD_OUTPUT_HANDLE));
  policy->SetStderrHandle(GetStdHandle(STD_ERROR_HANDLE));
#endif

  if (!delegate->PreSpawnTarget(policy.get()))
    return SBOX_ERROR_DELEGATE_PRE_SPAWN;

  TRACE_EVENT_BEGIN0("startup", "StartProcessWithAccess::LAUNCHPROCESS");

  PROCESS_INFORMATION temp_process_info = {};
  ResultCode last_warning = sandbox::SBOX_ALL_OK;
  DWORD last_error = ERROR_SUCCESS;
  result = g_broker_services->SpawnTarget(
      cmd_line->GetProgram().value().c_str(),
      cmd_line->GetCommandLineString().c_str(), policy, &last_warning,
      &last_error, &temp_process_info);

  base::win::ScopedProcessInformation target(temp_process_info);

  TRACE_EVENT_END0("startup", "StartProcessWithAccess::LAUNCHPROCESS");

  // Trace policy as processes are started. Useful for both failure and success.
  TRACE_EVENT_INSTANT2(TRACE_DISABLED_BY_DEFAULT("sandbox"), "processLaunch",
                       TRACE_EVENT_SCOPE_PROCESS, "sandboxType",
                       GetSandboxTypeInEnglish(delegate->GetSandboxType()),
                       "policy",
                       std::make_unique<PolicyTraceHelper>(policy.get()));

  if (SBOX_ALL_OK != result) {
    base::UmaHistogramSparse("Process.Sandbox.Launch.Error", last_error);
    if (result == SBOX_ERROR_GENERIC)
      DPLOG(ERROR) << "Failed to launch process";
    else
      DLOG(ERROR) << "Failed to launch process. Error: " << result;
    return result;
  }

  base::debug::GlobalActivityTracker* tracker =
      base::debug::GlobalActivityTracker::Get();
  if (tracker) {
    tracker->RecordProcessLaunch(target.process_id(),
                                 cmd_line->GetCommandLineString());
  }

  if (SBOX_ALL_OK != last_warning)
    LogLaunchWarning(last_warning, last_error);

  delegate->PostSpawnTarget(target.process_handle());
  CHECK(ResumeThread(target.thread_handle()) != static_cast<DWORD>(-1));

  *process = base::Process(target.TakeProcessHandle());
  return SBOX_ALL_OK;
}

// static
ResultCode SandboxWin::GetPolicyDiagnostics(
    base::OnceCallback<void(base::Value)> response) {
  CHECK(g_broker_services);
  CHECK(!response.is_null());
  auto receiver = std::make_unique<ServiceManagerDiagnosticsReceiver>(
      base::SequencedTaskRunnerHandle::Get(), std::move(response));
  return g_broker_services->GetPolicyDiagnostics(std::move(receiver));
}

void BlocklistAddOneDllForTesting(const wchar_t* module_name,
                                  bool check_in_browser,
                                  TargetPolicy* policy) {
  BlocklistAddOneDll(module_name, check_in_browser, policy);
}

// static
std::string SandboxWin::GetSandboxTypeInEnglish(SandboxType sandbox_type) {
  switch (sandbox_type) {
    case SandboxType::kNoSandbox:
      return "Unsandboxed";
    case SandboxType::kNoSandboxAndElevatedPrivileges:
      return "Unsandboxed (Elevated)";
    case SandboxType::kXrCompositing:
      return "XR Compositing";
    case SandboxType::kRenderer:
      return "Renderer";
    case SandboxType::kUtility:
      return "Utility";
    case SandboxType::kGpu:
      return "GPU";
    case SandboxType::kPpapi:
      return "PPAPI";
    case SandboxType::kNetwork:
      return "Network";
    case SandboxType::kCdm:
      return "CDM";
    case SandboxType::kPrintCompositor:
      return "Print Compositor";
    case SandboxType::kAudio:
      return "Audio";
    case SandboxType::kSpeechRecognition:
      return "Speech Recognition";
    case SandboxType::kProxyResolver:
      return "Proxy Resolver";
    case SandboxType::kPdfConversion:
      return "PDF Conversion";
    case SandboxType::kMediaFoundationCdm:
      return "Media Foundation CDM";
    case SandboxType::kSharingService:
      return "Sharing";
    case SandboxType::kVideoCapture:
      return "Video Capture";
    case SandboxType::kIconReader:
      return "Icon Reader";
  }
}

}  // namespace policy
}  // namespace sandbox
