// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/policy/win/sandbox_win.h"

#include <stddef.h>

#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/cxx17_backports.h"
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
#include "base/process/process.h"
#include "base/strings/strcat.h"
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
#include "base/win/sid.h"
#include "base/win/win_util.h"
#include "base/win/windows_version.h"
#include "printing/buildflags/buildflags.h"
#include "sandbox/policy/features.h"
#include "sandbox/policy/mojom/sandbox.mojom.h"
#include "sandbox/policy/sandbox_type.h"
#include "sandbox/policy/switches.h"
#include "sandbox/policy/win/lpac_capability.h"
#include "sandbox/policy/win/sandbox_diagnostics.h"
#include "sandbox/win/src/app_container.h"
#include "sandbox/win/src/job.h"
#include "sandbox/win/src/process_mitigations.h"
#include "sandbox/win/src/sandbox.h"
#include "sandbox/win/src/sandbox_nt_util.h"
#include "sandbox/win/src/sandbox_policy_base.h"
#include "sandbox/win/src/sandbox_policy_diagnostic.h"
#include "sandbox/win/src/win_utils.h"

namespace sandbox {
namespace policy {
using sandbox::mojom::Sandbox;

namespace {

BrokerServices* g_broker_services = NULL;

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
  explicit PolicyTraceHelper(TargetPolicy* policy) {
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

DWORD GetSessionId() {
  DWORD session_id;
  CHECK(::ProcessIdToSessionId(::GetCurrentProcessId(), &session_id));
  return session_id;
}

// Returns the object path prepended with the current logon session.
std::wstring PrependWindowsSessionPath(const wchar_t* object) {
  // Cache this because it can't change after process creation.
  static DWORD s_session_id = GetSessionId();
  return base::StringPrintf(L"\\Sessions\\%lu%ls", s_session_id, object);
}

// Checks if the sandbox can be let to run without a job object assigned.
// Returns true if the job object has to be applied to the sandbox and false
// otherwise.
bool ShouldSetJobLevel(bool allow_no_sandbox_job) {
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
    return !allow_no_sandbox_job;
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

ResultCode AddDefaultPolicyForSandboxedProcess(TargetPolicy* policy) {
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

NtQueryObjectFunction g_QueryObject = NULL;

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

  return base::FeatureList::IsEnabled(features::kRendererAppContainer);
}

ResultCode SetJobMemoryLimit(const base::CommandLine& cmd_line,
                             TargetPolicy* policy) {
  DCHECK_NE(policy->GetJobLevel(), JOB_NONE);

#ifdef _WIN64
  size_t memory_limit = static_cast<size_t>(kDataSizeLimit);

  // Note that this command line flag hasn't been fetched by all
  // callers of SetJobLevel, only those in this file.
  Sandbox sandbox_type = SandboxTypeFromCommandLine(cmd_line);
  if (sandbox_type == Sandbox::kGpu || sandbox_type == Sandbox::kRenderer) {
    int64_t GB = 1024 * 1024 * 1024;
    // Allow the GPU/RENDERER process's sandbox to access more physical memory
    // if it's available on the system.
    //
    // Renderer processes are allowed to access 16 GB; the GPU process, up
    // to 64 GB.
    int64_t physical_memory = base::SysInfo::AmountOfPhysicalMemory();
    if (sandbox_type == Sandbox::kGpu && physical_memory > 64 * GB) {
      memory_limit = 64 * GB;
    } else if (sandbox_type == Sandbox::kGpu && physical_memory > 32 * GB) {
      memory_limit = 32 * GB;
    } else if (physical_memory > 16 * GB) {
      memory_limit = 16 * GB;
    } else {
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
std::wstring GetAppContainerProfileName(const std::string& appcontainer_id,
                                        Sandbox sandbox_type) {
  std::string sandbox_base_name;
  switch (sandbox_type) {
    case Sandbox::kXrCompositing:
      sandbox_base_name = std::string("cr.sb.xr");
      break;
    case Sandbox::kGpu:
      sandbox_base_name = std::string("cr.sb.gpu");
      break;
    case Sandbox::kMediaFoundationCdm:
      sandbox_base_name = std::string("cr.sb.cdm");
      break;
    case Sandbox::kNetwork:
      sandbox_base_name = std::string("cr.sb.net");
      break;
    case Sandbox::kWindowsSystemProxyResolver:
      sandbox_base_name = std::string("cr.sb.pxy");
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

ResultCode SetupAppContainerProfile(AppContainer* container,
                                    const base::CommandLine& command_line,
                                    Sandbox sandbox_type) {
  if (sandbox_type != Sandbox::kMediaFoundationCdm &&
      sandbox_type != Sandbox::kGpu &&
      sandbox_type != Sandbox::kXrCompositing &&
      sandbox_type != Sandbox::kNetwork &&
      sandbox_type != Sandbox::kWindowsSystemProxyResolver) {
    return SBOX_ERROR_UNSUPPORTED;
  }

  if (sandbox_type == Sandbox::kGpu &&
      !container->AddImpersonationCapability(L"chromeInstallFiles")) {
    DLOG(ERROR) << "AppContainer::AddImpersonationCapability("
                   "chromeInstallFiles) failed";
    return SBOX_ERROR_CREATE_APPCONTAINER_CAPABILITY;
  }

  if ((sandbox_type == Sandbox::kXrCompositing ||
       sandbox_type == Sandbox::kGpu) &&
      !container->AddCapability(L"lpacPnpNotifications")) {
    DLOG(ERROR) << "AppContainer::AddCapability(lpacPnpNotifications) failed";
    return SBOX_ERROR_CREATE_APPCONTAINER_CAPABILITY;
  }

  if (sandbox_type == Sandbox::kXrCompositing &&
      !container->AddCapability(L"chromeInstallFiles")) {
    DLOG(ERROR) << "AppContainer::AddCapability(chromeInstallFiles) failed";
    return SBOX_ERROR_CREATE_APPCONTAINER_CAPABILITY;
  }

  if (sandbox_type == Sandbox::kMediaFoundationCdm) {
    // Please refer to the following design doc on why we add the capabilities:
    // https://docs.google.com/document/d/19Y4Js5v3BlzA5uSuiVTvcvPNIOwmxcMSFJWtuc1A-w8/edit#heading=h.iqvhsrml3gl9
    if (!container->AddCapability(
            base::win::WellKnownCapability::kPrivateNetworkClientServer) ||
        !container->AddCapability(
            base::win::WellKnownCapability::kInternetClient)) {
      DLOG(ERROR)
          << "AppContainer::AddCapability() - "
          << "Sandbox::kMediaFoundationCdm internet capabilities failed";
      return sandbox::SBOX_ERROR_CREATE_APPCONTAINER_CAPABILITY;
    }

    if (!container->AddCapability(L"lpacCom") ||
        !container->AddCapability(L"lpacIdentityServices") ||
        !container->AddCapability(L"lpacMedia") ||
        !container->AddCapability(L"lpacPnPNotifications") ||
        !container->AddCapability(L"lpacServicesManagement") ||
        !container->AddCapability(L"lpacSessionManagement") ||
        !container->AddCapability(L"lpacAppExperience") ||
        !container->AddCapability(L"lpacInstrumentation") ||
        !container->AddCapability(L"lpacCryptoServices") ||
        !container->AddCapability(L"lpacEnterprisePolicyChangeNotifications") ||
        !container->AddCapability(L"mediaFoundationCdmFiles")) {
      DLOG(ERROR) << "AppContainer::AddCapability() - "
                  << "Sandbox::kMediaFoundationCdm lpac capabilities failed";
      return sandbox::SBOX_ERROR_CREATE_APPCONTAINER_CAPABILITY;
    }
  }

  if (sandbox_type == Sandbox::kWindowsSystemProxyResolver) {
    if (!container->AddCapability(
            base::win::WellKnownCapability::kInternetClient)) {
      DLOG(ERROR) << "AppContainer::AddCapability() - "
                  << "Sandbox::kWindowsSystemProxyResolver internet "
                     "capabilities failed";
      return sandbox::SBOX_ERROR_CREATE_APPCONTAINER_CAPABILITY;
    }

    if (!container->AddCapability(L"lpacServicesManagement") ||
        !container->AddCapability(L"lpacEnterprisePolicyChangeNotifications")) {
      DLOG(ERROR) << "AppContainer::AddCapability() - "
                  << "Sandbox::kWindowsSystemProxyResolver lpac "
                     "capabilities failed";
      return sandbox::SBOX_ERROR_CREATE_APPCONTAINER_CAPABILITY;
    }
  }

  std::vector<std::wstring> base_caps = {
      L"lpacChromeInstallFiles",
      L"registryRead",
  };

  if (sandbox_type == Sandbox::kGpu) {
    auto cmdline_caps = base::SplitString(
        command_line.GetSwitchValueNative(switches::kAddGpuAppContainerCaps),
        L",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    base_caps.insert(base_caps.end(), cmdline_caps.begin(), cmdline_caps.end());
  }

  if (sandbox_type == Sandbox::kXrCompositing) {
    auto cmdline_caps = base::SplitString(
        command_line.GetSwitchValueNative(switches::kAddXrAppContainerCaps),
        L",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    base_caps.insert(base_caps.end(), cmdline_caps.begin(), cmdline_caps.end());
  }

  for (const auto& cap : base_caps) {
    if (!container->AddCapability(cap.c_str())) {
      DLOG(ERROR) << "AppContainer::AddCapability() failed";
      return SBOX_ERROR_CREATE_APPCONTAINER_CAPABILITY;
    }
  }

  // Enable LPAC for GPU process, but not for XRCompositor service.
  if (sandbox_type == Sandbox::kGpu &&
      base::FeatureList::IsEnabled(features::kGpuLPAC)) {
    container->SetEnableLowPrivilegeAppContainer(true);
  }

  // Enable LPAC for Network service.
  if (sandbox_type == Sandbox::kNetwork) {
    container->AddCapability(
        base::win::WellKnownCapability::kPrivateNetworkClientServer);
    container->AddCapability(base::win::WellKnownCapability::kInternetClient);
    container->AddCapability(
        base::win::WellKnownCapability::kEnterpriseAuthentication);
    container->AddCapability(L"lpacIdentityServices");
    container->AddCapability(L"lpacCryptoServices");
    container->SetEnableLowPrivilegeAppContainer(true);
  }

  if (sandbox_type == Sandbox::kMediaFoundationCdm) {
    container->AddCapability(kMediaFoundationCdmData);
    container->SetEnableLowPrivilegeAppContainer(true);
  }

  if (sandbox_type == Sandbox::kWindowsSystemProxyResolver) {
    container->SetEnableLowPrivilegeAppContainer(true);
  }

  return SBOX_ALL_OK;
}

// Launches outside of the sandbox - the process will not be associated with
// a Policy or TargetProcess. This supports both kNoSandbox and the --no-sandbox
// command line flag.
ResultCode LaunchWithoutSandbox(
    const base::CommandLine& cmd_line,
    const base::HandlesToInheritVector& handles_to_inherit,
    SandboxDelegate* delegate,
    base::Process* process) {
  base::LaunchOptions options;
  options.handles_to_inherit = handles_to_inherit;
  // Network process runs in a job even when unsandboxed. This is to ensure it
  // does not outlive the browser, which could happen if there is a lot of I/O
  // on process shutdown, in which case TerminateProcess can fail. See
  // https://crbug.com/820996.
  if (delegate->ShouldUnsandboxedRunInJob()) {
    BOOL in_job = true;
    // Prior to Windows 8 nested jobs aren't possible.
    if (base::win::GetVersion() >= base::win::Version::WIN8 ||
        (::IsProcessInJob(::GetCurrentProcess(), nullptr, &in_job) &&
         !in_job)) {
      static HANDLE job_object_handle = nullptr;
      if (!job_object_handle) {
        Job job_obj;
        DWORD result = job_obj.Init(JOB_UNPROTECTED, nullptr, 0, 0);
        if (result != ERROR_SUCCESS)
          return SBOX_ERROR_CANNOT_INIT_JOB;
        job_object_handle = job_obj.Take().Take();
      }
      options.job_handle = job_object_handle;
    }
  }

  // Chromium binaries are marked as CET Compatible but some processes
  // are not. When --no-sandbox is specified we disable CET for all children.
  // Otherwise we are here because the sandbox type is kNoSandbox, and allow
  // the process delegate to indicate if it is compatible with CET.
  if (cmd_line.HasSwitch(switches::kNoSandbox) ||
      base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kNoSandbox)) {
    options.disable_cetcompat = true;
  } else if (!delegate->CetCompatible()) {
    options.disable_cetcompat = true;
  }

  *process = base::LaunchProcess(cmd_line, options);
  if (!process->IsValid())
    return SBOX_ERROR_CANNOT_LAUNCH_UNSANDBOXED_PROCESS;
  return SBOX_ALL_OK;
}

bool IsUnsandboxedProcess(
    Sandbox sandbox_type,
    const base::CommandLine& cmd_line,
    const base::CommandLine& launcher_process_command_line) {
  if (IsUnsandboxedSandboxType(sandbox_type))
    return true;
  if (cmd_line.HasSwitch(switches::kNoSandbox))
    return true;
  if (launcher_process_command_line.HasSwitch(switches::kNoSandbox))
    return true;
  return false;
}

}  // namespace

// static
ResultCode SandboxWin::SetJobLevel(const base::CommandLine& cmd_line,
                                   JobLevel job_level,
                                   uint32_t ui_exceptions,
                                   TargetPolicy* policy) {
  if (!ShouldSetJobLevel(policy->GetAllowNoSandboxJob()))
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
  std::wstring object_path = PrependWindowsSessionPath(
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
ResultCode SandboxWin::AddWin32kLockdownPolicy(TargetPolicy* policy) {
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

  return policy->AddRule(TargetPolicy::SUBSYS_WIN32K_LOCKDOWN,
                         TargetPolicy::FAKE_USER_GDI_INIT, nullptr);
#else
  return SBOX_ALL_OK;
#endif
}

// static
ResultCode SandboxWin::AddAppContainerProfileToPolicy(
    const base::CommandLine& command_line,
    Sandbox sandbox_type,
    const std::string& appcontainer_id,
    TargetPolicy* policy) {
  if (base::win::GetVersion() < base::win::Version::WIN10_RS1)
    return SBOX_ALL_OK;
  std::wstring profile_name =
      GetAppContainerProfileName(appcontainer_id, sandbox_type);
  ResultCode result =
      policy->AddAppContainerProfile(profile_name.c_str(), true);
  if (result != SBOX_ALL_OK)
    return result;

  scoped_refptr<AppContainer> container = policy->GetAppContainer();
  result =
      SetupAppContainerProfile(container.get(), command_line, sandbox_type);
  if (result != SBOX_ALL_OK)
    return result;

  DWORD granted_access;
  BOOL granted_access_status;
  bool access_check =
      container->AccessCheck(command_line.GetProgram().value().c_str(),
                             SecurityObjectType::kFile,
                             GENERIC_READ | GENERIC_EXECUTE, &granted_access,
                             &granted_access_status) &&
      granted_access_status;
  if (!access_check) {
    PLOG(ERROR) << "Sandbox cannot access executable. Check filesystem "
                   "permissions are valid. See https://bit.ly/31yqMJR.";
    return SBOX_ERROR_CREATE_APPCONTAINER_ACCESS_CHECK;
  }

  return SBOX_ALL_OK;
}

// static
bool SandboxWin::IsAppContainerEnabledForSandbox(
    const base::CommandLine& command_line,
    Sandbox sandbox_type) {
  if (base::win::GetVersion() < base::win::Version::WIN10_RS1)
    return false;

  if (sandbox_type == Sandbox::kMediaFoundationCdm)
    return true;

  if (sandbox_type == Sandbox::kGpu)
    return base::FeatureList::IsEnabled(features::kGpuAppContainer);

  if (sandbox_type == Sandbox::kNetwork) {
    return true;
  }

  if (sandbox_type == Sandbox::kWindowsSystemProxyResolver)
    return true;

  return false;
}

// static
bool SandboxWin::InitBrokerServices(BrokerServices* broker_services) {
  // TODO(abarth): DCHECK(CalledOnValidThread());
  //               See <http://b/1287166>.
  DCHECK(broker_services);
  DCHECK(!g_broker_services);
  ResultCode init_result = broker_services->Init();
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

  return SBOX_ALL_OK == init_result;
}

// static
bool SandboxWin::InitTargetServices(TargetServices* target_services) {
  DCHECK(target_services);
  ResultCode result = target_services->Init();
  return SBOX_ALL_OK == result;
}

// static
ResultCode SandboxWin::GeneratePolicyForSandboxedProcess(
    const base::CommandLine& cmd_line,
    const std::string& process_type,
    const base::HandlesToInheritVector& handles_to_inherit,
    SandboxDelegate* delegate,
    const scoped_refptr<TargetPolicy>& policy) {
  const base::CommandLine& launcher_process_command_line =
      *base::CommandLine::ForCurrentProcess();

  Sandbox sandbox_type = delegate->GetSandboxType();
  // --no-sandbox and kNoSandbox are launched without a policy.
  if (IsUnsandboxedProcess(sandbox_type, cmd_line,
                           launcher_process_command_line)) {
    return ResultCode::SBOX_ERROR_UNSANDBOXED_PROCESS;
  }

  // Allow no sandbox job if the --allow-no-sandbox-job switch is present.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAllowNoSandboxJob)) {
    policy->SetAllowNoSandboxJob();
  }

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

  if (base::FeatureList::IsEnabled(features::kWinSboxDisableKtmComponent))
    mitigations |= MITIGATION_KTM_COMPONENT;

  // CET is enabled with the CETCOMPAT bit on chrome.exe so must be
  // disabled for processes we know are not compatible.
  if (!delegate->CetCompatible())
    mitigations |= MITIGATION_CET_DISABLED;

  ResultCode result = policy->SetProcessMitigations(mitigations);
  if (result != SBOX_ALL_OK)
    return result;

  if (process_type == switches::kRendererProcess) {
    result = SandboxWin::AddWin32kLockdownPolicy(policy.get());
    if (result != SBOX_ALL_OK)
      return result;
  }

  // Post-startup mitigations.
  mitigations = MITIGATION_DLL_SEARCH_ORDER;
  if (!cmd_line.HasSwitch(switches::kAllowThirdPartyModules) &&
      sandbox_type != Sandbox::kSpeechRecognition) {
    mitigations |= MITIGATION_FORCE_MS_SIGNED_BINS;
  }

  if (sandbox_type == Sandbox::kNetwork || sandbox_type == Sandbox::kAudio ||
      sandbox_type == Sandbox::kIconReader) {
    mitigations |= MITIGATION_DYNAMIC_CODE_DISABLE;
  }

  result = policy->SetDelayedProcessMitigations(mitigations);
  if (result != SBOX_ALL_OK)
    return result;

  result = SetJobLevel(cmd_line, JOB_LOCKDOWN, 0, policy.get());
  if (result != SBOX_ALL_OK)
    return result;

  if (!delegate->DisableDefaultPolicy()) {
    result = AddDefaultPolicyForSandboxedProcess(policy.get());
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
      sandbox_type == Sandbox::kPrintCompositor) {
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
  if (IsAppContainerEnabledForSandbox(cmd_line, sandbox_type) &&
      delegate->GetAppContainerId(&appcontainer_id)) {
    result = AddAppContainerProfileToPolicy(cmd_line, sandbox_type,
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

  if (sandbox_type == Sandbox::kMediaFoundationCdm) {
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

  return result;
}

// static
ResultCode SandboxWin::StartSandboxedProcess(
    const base::CommandLine& cmd_line,
    const std::string& process_type,
    const base::HandlesToInheritVector& handles_to_inherit,
    SandboxDelegate* delegate,
    base::Process* process) {
  scoped_refptr<TargetPolicy> policy = g_broker_services->CreatePolicy();
  ResultCode result = GeneratePolicyForSandboxedProcess(
      cmd_line, process_type, handles_to_inherit, delegate, policy);

  if (ResultCode::SBOX_ERROR_UNSANDBOXED_PROCESS == result) {
    return LaunchWithoutSandbox(cmd_line, handles_to_inherit, delegate,
                                process);
  }
  if (SBOX_ALL_OK != result)
    return result;

  TRACE_EVENT_BEGIN0("startup", "StartProcessWithAccess::LAUNCHPROCESS");

  PROCESS_INFORMATION temp_process_info = {};
  ResultCode last_warning = sandbox::SBOX_ALL_OK;
  DWORD last_error = ERROR_SUCCESS;
  result = g_broker_services->SpawnTarget(
      cmd_line.GetProgram().value().c_str(),
      cmd_line.GetCommandLineString().c_str(), policy, &last_warning,
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
                                 cmd_line.GetCommandLineString());
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
std::string SandboxWin::GetSandboxTypeInEnglish(Sandbox sandbox_type) {
  switch (sandbox_type) {
    case Sandbox::kNoSandbox:
      return "Unsandboxed";
    case Sandbox::kNoSandboxAndElevatedPrivileges:
      return "Unsandboxed (Elevated)";
    case Sandbox::kXrCompositing:
      return "XR Compositing";
    case Sandbox::kRenderer:
      return "Renderer";
    case Sandbox::kUtility:
      return "Utility";
    case Sandbox::kGpu:
      return "GPU";
    case Sandbox::kPpapi:
      return "PPAPI";
    case Sandbox::kNetwork:
      return "Network";
    case Sandbox::kCdm:
      return "CDM";
    case Sandbox::kPrintCompositor:
      return "Print Compositor";
#if BUILDFLAG(ENABLE_PRINTING)
    case Sandbox::kPrintBackend:
      return "Print Backend";
#endif
    case Sandbox::kAudio:
      return "Audio";
    case Sandbox::kSpeechRecognition:
      return "Speech Recognition";
    case Sandbox::kPdfConversion:
      return "PDF Conversion";
    case Sandbox::kMediaFoundationCdm:
      return "Media Foundation CDM";
    case Sandbox::kService:
      return "Service";
    case Sandbox::kIconReader:
      return "Icon Reader";
    case Sandbox::kWindowsSystemProxyResolver:
      return "Windows System Proxy Resolver";
  }
}

}  // namespace policy
}  // namespace sandbox
