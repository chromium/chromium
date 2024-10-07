// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/policy/win/sandbox_win.h"

#include <windows.h>

#include <stddef.h>
#include <winternl.h>

#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/hash/hash.h"
#include "base/hash/sha1.h"
#include "base/logging.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/metrics_hashes.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/string_util_win.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/win/iat_patch_function.h"
#include "base/win/scoped_handle.h"
#include "base/win/scoped_process_information.h"
#include "base/win/security_util.h"
#include "base/win/sid.h"
#include "base/win/win_util.h"
#include "base/win/windows_version.h"
#include "build/build_config.h"
#include "ppapi/buildflags/buildflags.h"
#include "printing/buildflags/buildflags.h"
#include "sandbox/features.h"
#include "sandbox/policy/features.h"
#include "sandbox/policy/mojom/sandbox.mojom.h"
#include "sandbox/policy/sandbox_type.h"
#include "sandbox/policy/switches.h"
#include "sandbox/policy/win/lpac_capability.h"
#include "sandbox/policy/win/sandbox_diagnostics.h"
#include "sandbox/win/src/app_container.h"
#include "sandbox/win/src/process_mitigations.h"
#include "sandbox/win/src/sandbox.h"
#include "services/screen_ai/buildflags/buildflags.h"

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
    L"btkeyind.dll",               // Widcomm Bluetooth.
    L"dockshellhook.dll",          // Stardock Objectdock.
    L"easyhook32.dll",             // GDIPP and others.
    L"easyhook64.dll",             // Symantec BlueCoat and others.
    L"guard64.dll",                // Comodo Internet Security x64.
    L"mdnsnsp.dll",                // Bonjour.
    L"n64hooks.dll",               // Neilsen//NetRatings NetSight.
    L"pmls64.dll",                 // PremierOpinion and Relevant-Knowledge.
    L"prochook.dll",               // Unknown (GBill-Tools?) (crbug.com/974722).
    L"rlls.dll",                   // PremierOpinion and Relevant-Knowledge.
    L"rlls64.dll",                 // PremierOpinion and Relevant-Knowledge.
    L"rpchromebrowserrecordhelper.dll",    // RealPlayer.
};

// This is for finch. See also crbug.com/464430 for details.
BASE_FEATURE(kEnableCsrssLockdownFeature,
             "EnableCsrssLockdown",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Return a mapping between the long and short names for all loaded modules in
// the current process. The mapping excludes modules which don't have a typical
// short name, e.g. EXAMPL~1.DLL.
std::map<std::wstring, std::wstring> GetShortNameModules() {
  std::vector<HMODULE> modules;
  if (!base::win::GetLoadedModulesSnapshot(::GetCurrentProcess(), &modules)) {
    return {};
  }
  std::map<std::wstring, std::wstring> names;
  for (HMODULE module : modules) {
    wchar_t path[MAX_PATH];
    DWORD sz = ::GetModuleFileNameW(module, path, std::size(path));
    if ((sz == std::size(path)) || (sz == 0)) {
      continue;
    }
    base::FilePath module_path(path);
    base::FilePath name = module_path.BaseName();
    if (name.RemoveExtension().value().size() > 8 ||
        name.Extension().size() > 4 || !base::Contains(name.value(), L"~")) {
      continue;
    }
    base::FilePath fname = base::MakeLongFilePath(module_path);
    names.insert_or_assign(base::ToLowerASCII(fname.BaseName().value()),
                           name.value());
  }
  return names;
}

// Add a block list DLL to a configuration |config| based on the name of the DLL
// passed as |module_name|. The DLL must be loaded in the current process. A
// mapping from long names to short names should also be passed in |modules| to
// attempt to map a long name to the actual loaded name, this can be initialized
// with a call to GetShortNameModules. Returns true if the DLL is loaded and
// will be blocked in the child.
bool BlocklistAddOneDll(const wchar_t* module_name,
                        const std::map<std::wstring, std::wstring>& modules,
                        TargetConfig* config) {
  DCHECK(!config->IsConfigured());
  if (::GetModuleHandleW(module_name) != nullptr) {
    config->AddDllToUnload(module_name);
    DVLOG(1) << "dll to unload found: " << module_name;
    return true;
  } else {
    auto short_name = modules.find(base::ToLowerASCII(module_name));
    if (short_name != modules.end()) {
      config->AddDllToUnload(short_name->second.c_str());
      config->AddDllToUnload(module_name);
      return true;
    }
  }
  return false;
}

// Adds the generic config rules to a sandbox TargetConfig.
ResultCode AddGenericConfig(sandbox::TargetConfig* config) {
  DCHECK(!config->IsConfigured());

// Add the policy for read-only PDB file access for stack traces.
#if !defined(OFFICIAL_BUILD)
  base::FilePath exe;
  if (!base::PathService::Get(base::FILE_EXE, &exe))
    return SBOX_ERROR_GENERIC;
  base::FilePath pdb_path = exe.DirName().Append(L"*.pdb");
  {
    ResultCode result = config->AllowFileAccess(FileSemantics::kAllowReadonly,
                                                pdb_path.value().c_str());
    if (result != SBOX_ALL_OK) {
      return result;
    }
  }
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
    {
      ResultCode result = config->AllowFileAccess(FileSemantics::kAllowAny,
                                                  sancov_path.value().c_str());
      if (result != SBOX_ALL_OK) {
        return result;
      }
    }
  }
#endif

  std::map<std::wstring, std::wstring> modules = GetShortNameModules();
  // Adds policy rules for unloading the known dlls that cause Chrome to crash.
  // Eviction of injected DLLs is done by the sandbox so that the injected
  // module does not get a chance to execute any code.
  for (const wchar_t* blocklist_dll : kTroublesomeDlls) {
    if (BlocklistAddOneDll(blocklist_dll, modules, config)) {
      // Log the module to help with list cleanup.
      base::UmaHistogramSparse("Process.Sandbox.DllBlocked",
                               static_cast<int32_t>(base::HashMetricName(
                                   base::WideToASCII(blocklist_dll))));
    }
  }

  return SBOX_ALL_OK;
}

ResultCode AddDefaultConfigForSandboxedProcess(TargetConfig* config) {
  // Prevents the renderers from manipulating low-integrity processes.
  config->SetDelayedIntegrityLevel(INTEGRITY_LEVEL_UNTRUSTED);
  ResultCode result = config->SetIntegrityLevel(INTEGRITY_LEVEL_LOW);
  if (result != SBOX_ALL_OK)
    return result;

  // The initial token has to be restricted if the main token is restricted.
  result = config->SetTokenLevel(USER_RESTRICTED_SAME_ACCESS, USER_LOCKDOWN);
  if (result != SBOX_ALL_OK)
    return result;

  config->SetLockdownDefaultDacl();
  config->AddKernelObjectToClose(HandleToClose::kDeviceApi);
  config->SetDesktop(Desktop::kAlternateWinstation);

  return SBOX_ALL_OK;
}

// This code is test only, and attempts to catch unsafe uses of
// DuplicateHandle() that copy privileged handles into sandboxed processes.
#if !defined(OFFICIAL_BUILD) && !defined(COMPONENT_BUILD)
base::win::IATPatchFunction& GetIATPatchFunctionHandle() {
  static base::NoDestructor<base::win::IATPatchFunction>
      iat_patch_duplicate_handle;
  return *iat_patch_duplicate_handle;
}

using DuplicateHandleFunctionPtr = decltype(::DuplicateHandle)*;

DuplicateHandleFunctionPtr g_iat_orig_duplicate_handle;

static const char* kDuplicateHandleWarning =
    "You are attempting to duplicate a privileged handle into a sandboxed"
    " process.\n Please contact security@chromium.org for assistance.";

void CheckDuplicateHandle(HANDLE handle) {
  // Get the object type (32 characters is safe; current max is 14).
  BYTE buffer[sizeof(PUBLIC_OBJECT_TYPE_INFORMATION) + 32 * sizeof(wchar_t)];
  PPUBLIC_OBJECT_TYPE_INFORMATION type_info =
      reinterpret_cast<PPUBLIC_OBJECT_TYPE_INFORMATION>(buffer);
  ULONG size = sizeof(buffer);
  NTSTATUS error =
      ::NtQueryObject(handle, ObjectTypeInformation, type_info, size, &size);
  CHECK(NT_SUCCESS(error));
  std::wstring_view type_name(type_info->TypeName.Buffer,
                              type_info->TypeName.Length / sizeof(wchar_t));

  std::optional<ACCESS_MASK> granted_access =
      base::win::GetGrantedAccess(handle);
  CHECK(granted_access.has_value());

  CHECK(!(*granted_access & WRITE_DAC)) << kDuplicateHandleWarning;

  if (base::EqualsCaseInsensitiveASCII(type_name, L"Process")) {
    const ACCESS_MASK kDangerousMask =
        ~static_cast<DWORD>(PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE);
    CHECK(!(*granted_access & kDangerousMask)) << kDuplicateHandleWarning;
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
  if (!sandbox::features::IsAppContainerSandboxSupported())
    return false;

  return base::FeatureList::IsEnabled(features::kRendererAppContainer);
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
    case Sandbox::kOnDeviceModelExecution:
      sandbox_base_name = std::string("cr.sb.odm");
      break;
#if BUILDFLAG(ENABLE_PRINTING)
    case Sandbox::kPrintCompositor:
      sandbox_base_name = std::string("cr.sb.prnc");
      break;
#endif
    case Sandbox::kWindowsSystemProxyResolver:
      sandbox_base_name = std::string("cr.sb.pxy");
      break;
    default:
      DCHECK(0);
  }

  auto sha1 = base::SHA1HashString(appcontainer_id);
  std::string profile_name =
      base::StrCat({sandbox_base_name, base::HexEncode(sha1)});
  // CreateAppContainerProfile requires that the profile name is at most 64
  // characters but 50 on WCOS systems.  The size of sha1 is a constant 40,
  // so validate that the base names are sufficiently short that the total
  // length is valid on all systems.
  DCHECK_LE(profile_name.length(), 50U);
  return base::UTF8ToWide(profile_name);
}

void AddCapabilitiesFromString(AppContainer* container,
                               const std::wstring& caps) {
  for (const std::wstring& cap : base::SplitString(
           caps, L",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
    container->AddCapability(cap.c_str());
  }
}

ResultCode SetupAppContainerProfile(AppContainer* container,
                                    const base::CommandLine& command_line,
                                    Sandbox sandbox_type) {
  if (sandbox_type != Sandbox::kGpu &&
      sandbox_type != Sandbox::kXrCompositing &&
      sandbox_type != Sandbox::kMediaFoundationCdm &&
      sandbox_type != Sandbox::kNetwork &&
      sandbox_type != Sandbox::kOnDeviceModelExecution &&
#if BUILDFLAG(ENABLE_PRINTING)
      !(sandbox_type == Sandbox::kPrintCompositor &&
        base::FeatureList::IsEnabled(
            sandbox::policy::features::kPrintCompositorLPAC)) &&
#endif
      sandbox_type != Sandbox::kWindowsSystemProxyResolver) {
    return SBOX_ERROR_UNSUPPORTED;
  }

  container->AddCapability(kLpacChromeInstallFiles);
  container->AddCapability(kRegistryRead);

  if (sandbox_type == Sandbox::kGpu) {
    container->AddImpersonationCapability(kChromeInstallFiles);
    container->AddCapability(kLpacPnpNotifications);
    AddCapabilitiesFromString(
        container,
        command_line.GetSwitchValueNative(switches::kAddGpuAppContainerCaps));
    container->SetEnableLowPrivilegeAppContainer(
        base::FeatureList::IsEnabled(features::kGpuLPAC));
  }

  if (sandbox_type == Sandbox::kXrCompositing) {
    container->AddCapability(kChromeInstallFiles);
    container->AddCapability(kLpacPnpNotifications);
    AddCapabilitiesFromString(container, command_line.GetSwitchValueNative(
                                             switches::kAddXrAppContainerCaps));
    // Note: does not use LPAC.
  }

  if (sandbox_type == Sandbox::kMediaFoundationCdm) {
    // Please refer to the following design doc on why we add the capabilities:
    // https://docs.google.com/document/d/19Y4Js5v3BlzA5uSuiVTvcvPNIOwmxcMSFJWtuc1A-w8/edit#heading=h.iqvhsrml3gl9
    container->AddCapability(
        base::win::WellKnownCapability::kPrivateNetworkClientServer);
    container->AddCapability(base::win::WellKnownCapability::kInternetClient);
    container->AddCapability(kLpacCom);
    container->AddCapability(kLpacIdentityServices);
    container->AddCapability(kLpacMedia);
    container->AddCapability(kLpacPnPNotifications);
    container->AddCapability(kLpacServicesManagement);
    container->AddCapability(kLpacSessionManagement);
    container->AddCapability(kLpacAppExperience);
    container->AddCapability(kLpacInstrumentation);
    container->AddCapability(kLpacCryptoServices);
    container->AddCapability(kLpacEnterprisePolicyChangeNotifications);
    container->AddCapability(kMediaFoundationCdmFiles);
    container->AddCapability(kMediaFoundationCdmData);
    container->SetEnableLowPrivilegeAppContainer(true);
  }

  if (sandbox_type == Sandbox::kNetwork) {
    container->AddCapability(
        base::win::WellKnownCapability::kPrivateNetworkClientServer);
    container->AddCapability(base::win::WellKnownCapability::kInternetClient);
    container->AddCapability(
        base::win::WellKnownCapability::kEnterpriseAuthentication);
    container->AddCapability(kLpacIdentityServices);
    container->AddCapability(kLpacCryptoServices);
    container->SetEnableLowPrivilegeAppContainer(base::FeatureList::IsEnabled(
        features::kWinSboxNetworkServiceSandboxIsLPAC));
  }

  if (sandbox_type == Sandbox::kOnDeviceModelExecution) {
    container->AddImpersonationCapability(kChromeInstallFiles);
    container->AddCapability(kLpacPnpNotifications);
    container->SetEnableLowPrivilegeAppContainer(true);
  }

#if BUILDFLAG(ENABLE_PRINTING)
  if (sandbox_type == Sandbox::kPrintCompositor) {
    container->AddCapability(kLpacCom);
    container->AddCapability(L"lpacPrinting");
    container->SetEnableLowPrivilegeAppContainer(true);
  }
#endif

  if (sandbox_type == Sandbox::kWindowsSystemProxyResolver) {
    container->AddCapability(base::win::WellKnownCapability::kInternetClient);
    container->AddCapability(kLpacServicesManagement);
    container->AddCapability(kLpacEnterprisePolicyChangeNotifications);
    container->SetEnableLowPrivilegeAppContainer(true);
  }

  return SBOX_ALL_OK;
}

ResultCode GenerateConfigForSandboxedProcess(const base::CommandLine& cmd_line,
                                             SandboxDelegate* delegate,
                                             TargetConfig* config) {
  DCHECK(!config->IsConfigured());

  // Pre-startup mitigations.
  MitigationFlags mitigations =
      MITIGATION_HEAP_TERMINATE | MITIGATION_BOTTOM_UP_ASLR | MITIGATION_DEP |
      MITIGATION_DEP_NO_ATL_THUNK | MITIGATION_EXTENSION_POINT_DISABLE |
      MITIGATION_SEHOP | MITIGATION_NONSYSTEM_FONT_DISABLE |
      MITIGATION_IMAGE_LOAD_NO_REMOTE | MITIGATION_IMAGE_LOAD_NO_LOW_LABEL |
      MITIGATION_RESTRICT_INDIRECT_BRANCH_PREDICTION |
      MITIGATION_KTM_COMPONENT | MITIGATION_FSCTL_DISABLED;

  // CET is enabled with the CETCOMPAT bit on chrome.exe so must be
  // disabled for processes we know are not compatible.
  if (!delegate->CetCompatible())
    mitigations |= MITIGATION_CET_DISABLED;

  const Sandbox sandbox_type = delegate->GetSandboxType();

  if (sandbox_type == Sandbox::kRenderer &&
      base::FeatureList::IsEnabled(
          sandbox::policy::features::kWinSboxRestrictCoreSharingOnRenderer)) {
    mitigations |= MITIGATION_RESTRICT_CORE_SHARING;
  }

  ResultCode result = config->SetProcessMitigations(mitigations);
  if (result != SBOX_ALL_OK)
    return result;

  // Post-startup mitigations.
  mitigations = MITIGATION_DLL_SEARCH_ORDER;
  if (!cmd_line.HasSwitch(switches::kAllowThirdPartyModules) &&
#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
      sandbox_type != Sandbox::kScreenAI &&
#endif
      sandbox_type != Sandbox::kSpeechRecognition &&
      sandbox_type != Sandbox::kMediaFoundationCdm) {
    mitigations |= MITIGATION_FORCE_MS_SIGNED_BINS;
  }

  if (sandbox_type == Sandbox::kNetwork || sandbox_type == Sandbox::kAudio ||
      sandbox_type == Sandbox::kIconReader) {
    mitigations |= MITIGATION_DYNAMIC_CODE_DISABLE;
  }

  result = config->SetDelayedProcessMitigations(mitigations);
  if (result != SBOX_ALL_OK)
    return result;

  if (sandbox_type == Sandbox::kRenderer) {
    // TODO(crbug.com/40088338) Remove if we can reliably not load
    // cryptbase.dll.
    config->AddKernelObjectToClose(HandleToClose::kKsecDD);
    result = SandboxWin::AddWin32kLockdownPolicy(config);
    if (result != SBOX_ALL_OK) {
      return result;
    }
  }

  if (!delegate->DisableDefaultPolicy()) {
    result = AddDefaultConfigForSandboxedProcess(config);
    if (result != SBOX_ALL_OK)
      return result;
  }

  // Disable apphelp for tightly sandboxed processes that are not running
  // in WoW or ARM64 emulated modes.
  if (sandbox_type == Sandbox::kRenderer || sandbox_type == Sandbox::kService) {
    if (base::FeatureList::IsEnabled(
            sandbox::policy::features::kWinSboxZeroAppShim) &&
        base::win::OSInfo::GetInstance()->IsWowDisabled() &&
        !base::win::OSInfo::IsRunningEmulatedOnArm64()) {
      config->SetZeroAppShim();
    }
  }

  result =
      SandboxWin::SetJobLevel(sandbox_type, JobLevel::kLockdown, 0, config);
  if (result != SBOX_ALL_OK)
    return result;

  if (sandbox_type == Sandbox::kGpu) {
    config->SetLockdownDefaultDacl();
    config->AddRestrictingRandomSid();
  }

  result = AddGenericConfig(config);
  if (result != SBOX_ALL_OK) {
    NOTREACHED();
  }

  std::string appcontainer_id;
  if (SandboxWin::IsAppContainerEnabledForSandbox(cmd_line, sandbox_type) &&
      delegate->GetAppContainerId(&appcontainer_id)) {
    result = SandboxWin::AddAppContainerProfileToConfig(
        cmd_line, sandbox_type, appcontainer_id, config);
    DCHECK_EQ(result, SBOX_ALL_OK);
    if (result != SBOX_ALL_OK)
      return result;
  }

  if (sandbox_type == Sandbox::kMediaFoundationCdm) {
    // Set a policy that would normally allow for process creation. This allows
    // the mf cdm process to launch the protected media pipeline process
    // (mfpmp.exe) without process interception.
    result = config->SetJobLevel(JobLevel::kInteractive, 0);
    if (result != SBOX_ALL_OK)
      return result;
  }

  if (!delegate->InitializeConfig(config)) {
    return SBOX_ERROR_DELEGATE_INITIALIZE_CONFIG;
  }

  return SBOX_ALL_OK;
}

// Create the job object for unsandboxed processes.
base::win::ScopedHandle CreateUnsandboxedJob() {
  base::win::ScopedHandle job(::CreateJobObject(nullptr, nullptr));
  if (!job.is_valid()) {
    return base::win::ScopedHandle();
  }

  JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits = {};
  limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
  if (!::SetInformationJobObject(job.get(), JobObjectExtendedLimitInformation,
                                 &limits, sizeof(limits))) {
    return base::win::ScopedHandle();
  }
  return job;
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
    static base::NoDestructor<base::win::ScopedHandle> job_object(
        CreateUnsandboxedJob());
    if (!job_object->is_valid()) {
      return SBOX_ERROR_CANNOT_INIT_JOB;
    }
    options.job_handle = job_object->get();
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

void SandboxLaunchTimer::RecordHistograms() {
  // If these parameters change the histograms should be renamed.
  // We're interested in the happy fast case so have a low maximum.
  const auto kLowBound = base::Microseconds(5);
  const auto kHighBound = base::Microseconds(100000);
  const int kBuckets = 50;

  base::UmaHistogramCustomMicrosecondsTimes(
      "Process.Sandbox.StartSandboxedWin.CreatePolicyDuration", policy_created_,
      kLowBound, kHighBound, kBuckets);
  base::UmaHistogramCustomMicrosecondsTimes(
      "Process.Sandbox.StartSandboxedWin.GeneratePolicyDuration",
      policy_generated_ - policy_created_, kLowBound, kHighBound, kBuckets);
  base::UmaHistogramCustomMicrosecondsTimes(
      "Process.Sandbox.StartSandboxedWin.SpawnTargetDuration",
      process_spawned_ - policy_generated_, kLowBound, kHighBound, kBuckets);
  base::UmaHistogramCustomMicrosecondsTimes(
      "Process.Sandbox.StartSandboxedWin.PostSpawnTargetDuration",
      process_resumed_ - process_spawned_, kLowBound, kHighBound, kBuckets);
  base::UmaHistogramCustomMicrosecondsTimes(
      "Process.Sandbox.StartSandboxedWin.TotalDuration", process_resumed_,
      kLowBound, kHighBound, kBuckets);
}

// static
ResultCode SandboxWin::SetJobLevel(Sandbox sandbox_type,
                                   JobLevel job_level,
                                   uint32_t ui_exceptions,
                                   TargetConfig* config) {
  DCHECK(!config->IsConfigured());

  ResultCode ret = config->SetJobLevel(job_level, ui_exceptions);
  if (ret != SBOX_ALL_OK)
    return ret;

  std::optional<size_t> memory_limit = GetJobMemoryLimit(sandbox_type);
  if (memory_limit) {
    config->SetJobMemoryLimit(*memory_limit);
  }
  return SBOX_ALL_OK;
}

// static
void SandboxWin::AddBaseHandleClosePolicy(TargetConfig* config) {
  DCHECK(!config->IsConfigured());

  if (base::FeatureList::IsEnabled(kEnableCsrssLockdownFeature)) {
    // Close all ALPC ports.
    config->SetDisconnectCsrss();
  }

  config->AddKernelObjectToClose(HandleToClose::kWindowsShellGlobalCounters);
}

// static
ResultCode SandboxWin::AddAppContainerPolicy(TargetConfig* config,
                                             const wchar_t* sid) {
  DCHECK(!config->IsConfigured());
  if (IsAppContainerEnabled()) {
    ResultCode result = config->SetLowBox(sid);
    if (result != SBOX_ALL_OK) {
      return result;
    }
    config->GetAppContainer()->AddImpersonationCapability(
        kLpacChromeInstallFiles);
  }
  return SBOX_ALL_OK;
}

// static
ResultCode SandboxWin::AddWin32kLockdownPolicy(TargetConfig* config) {
  DCHECK(!config->IsConfigured());
  MitigationFlags flags = config->GetProcessMitigations();
  // Check not enabling twice. Should not happen.
  DCHECK_EQ(0U, flags & MITIGATION_WIN32K_DISABLE);

  flags |= MITIGATION_WIN32K_DISABLE;
  ResultCode result = config->SetProcessMitigations(flags);
  if (result != SBOX_ALL_OK)
    return result;

  // winmm.dll, used by timeGetTime, depends on user32 and gdi32 until RS1.
  if (base::win::GetVersion() <= base::win::Version::WIN10_TH2 ||
      !base::FeatureList::IsEnabled(features::kWinSboxNoFakeGdiInit)) {
    return config->SetFakeGdiInit();
  }
  return SBOX_ALL_OK;
}

// static
ResultCode SandboxWin::AddAppContainerProfileToConfig(
    const base::CommandLine& command_line,
    Sandbox sandbox_type,
    const std::string& appcontainer_id,
    TargetConfig* config) {
  DCHECK(!config->IsConfigured());
  if (base::win::GetVersion() < base::win::Version::WIN10_RS1)
    return SBOX_ALL_OK;
  std::wstring profile_name =
      GetAppContainerProfileName(appcontainer_id, sandbox_type);

  ResultCode result = config->AddAppContainerProfile(profile_name.c_str());
  if (result != SBOX_ALL_OK)
    return result;

  result = SetupAppContainerProfile(config->GetAppContainer(), command_line,
                                    sandbox_type);
  if (result != SBOX_ALL_OK)
    return result;

  DWORD granted_access;
  BOOL granted_access_status;
  bool access_check =
      config->GetAppContainer()->AccessCheck(
          command_line.GetProgram().value().c_str(),
          base::win::SecurityObjectType::kFile, GENERIC_READ | GENERIC_EXECUTE,
          &granted_access, &granted_access_status) &&
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
  if (!sandbox::features::IsAppContainerSandboxSupported())
    return false;

  if (sandbox_type == Sandbox::kMediaFoundationCdm)
    return true;

  if (sandbox_type == Sandbox::kGpu)
    return base::FeatureList::IsEnabled(features::kGpuAppContainer);

  if (sandbox_type == Sandbox::kNetwork) {
    return true;
  }

  if (sandbox_type == Sandbox::kOnDeviceModelExecution) {
    return true;
  }

#if BUILDFLAG(ENABLE_PRINTING)
  if (sandbox_type == Sandbox::kPrintCompositor) {
    return base::FeatureList::IsEnabled(
        sandbox::policy::features::kPrintCompositorLPAC);
  }
#endif

  if (sandbox_type == Sandbox::kWindowsSystemProxyResolver)
    return true;

  return false;
}

class BrokerServicesDelegateImpl : public BrokerServicesDelegate {
 public:
  bool ParallelLaunchEnabled() override {
    return features::IsParallelLaunchEnabled();
  }

  void ParallelLaunchPostTaskAndReplyWithResult(
      const base::Location& from_here,
      base::OnceCallback<CreateTargetResult()> task,
      base::OnceCallback<void(CreateTargetResult)> reply) override {
    base::ThreadPool::PostTaskAndReplyWithResult(
        from_here,
        {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
         base::TaskShutdownBehavior::BLOCK_SHUTDOWN},
        std::move(task), std::move(reply));
  }

  void BeforeTargetProcessCreateOnCreationThread(
      const void* trace_id) override {
    int active_threads = ++creation_threads_in_use_;
    base::UmaHistogramCounts100("MPArch.ChildProcessLaunchActivelyInParallel",
                                active_threads);

    TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("startup", "TargetProcess::Create",
                                      trace_id);
  }

  void AfterTargetProcessCreateOnCreationThread(const void* trace_id,
                                                DWORD process_id) override {
    creation_threads_in_use_--;
    TRACE_EVENT_NESTABLE_ASYNC_END1("startup", "TargetProcess::Create",
                                    trace_id, "pid", process_id);
  }

 private:
  // When parallel launching is enabled, target creation will happen on the
  // thread pool. This is atomic to keep track of the number of threads that are
  // currently creating processes.
  std::atomic<int> creation_threads_in_use_ = 0;
};

// static
bool SandboxWin::InitBrokerServices(BrokerServices* broker_services) {
  // TODO(abarth): DCHECK(CalledOnValidThread());
  //               See <http://b/1287166>.
  DCHECK(broker_services);
  DCHECK(!g_broker_services);

  ResultCode init_result =
      broker_services->Init(std::make_unique<BrokerServicesDelegateImpl>());
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
    const base::HandlesToInheritVector& handles_to_inherit,
    SandboxDelegate* delegate,
    TargetPolicy* policy) {
  const base::CommandLine& launcher_process_command_line =
      *base::CommandLine::ForCurrentProcess();

  Sandbox sandbox_type = delegate->GetSandboxType();
  // --no-sandbox and kNoSandbox are launched without a policy.
  if (IsUnsandboxedProcess(sandbox_type, cmd_line,
                           launcher_process_command_line)) {
    return ResultCode::SBOX_ERROR_UNSANDBOXED_PROCESS;
  }

  // Add any handles to be inherited to the policy.
  for (HANDLE handle : handles_to_inherit)
    policy->AddHandleToShare(handle);

  if (!policy->GetConfig()->IsConfigured()) {
    ResultCode result = GenerateConfigForSandboxedProcess(cmd_line, delegate,
                                                          policy->GetConfig());
    if (result != SBOX_ALL_OK)
      return result;
  }

#if !defined(OFFICIAL_BUILD)
  // If stdout/stderr point to a Windows console, these calls will
  // have no effect. These calls can fail with SBOX_ERROR_BAD_PARAMS.
  policy->SetStdoutHandle(GetStdHandle(STD_OUTPUT_HANDLE));
  policy->SetStderrHandle(GetStdHandle(STD_ERROR_HANDLE));
#endif

  if (!delegate->PreSpawnTarget(policy))
    return SBOX_ERROR_DELEGATE_PRE_SPAWN;

  return SBOX_ALL_OK;
}

// static
ResultCode SandboxWin::StartSandboxedProcess(
    const base::CommandLine& cmd_line,
    const base::HandlesToInheritVector& handles_to_inherit,
    SandboxDelegate* delegate,
    StartSandboxedProcessCallback result_callback) {
  SandboxLaunchTimer timer;

  // Avoid making a policy if we won't use it.
  if (IsUnsandboxedProcess(delegate->GetSandboxType(), cmd_line,
                           *base::CommandLine::ForCurrentProcess())) {
    base::Process process;
    ResultCode result =
        LaunchWithoutSandbox(cmd_line, handles_to_inherit, delegate, &process);
    DWORD last_error = GetLastError();
    std::move(result_callback).Run(std::move(process), last_error, result);
    return SBOX_ALL_OK;
  }

  auto policy = g_broker_services->CreatePolicy(delegate->GetSandboxTag());
  timer.OnPolicyCreated();

  ResultCode result = GeneratePolicyForSandboxedProcess(
      cmd_line, handles_to_inherit, delegate, policy.get());
  if (SBOX_ALL_OK != result) {
    DWORD last_error = GetLastError();
    std::move(result_callback).Run(base::Process(), last_error, result);
    return result;
  }
  timer.OnPolicyGenerated();

  int64_t trace_event_id = timer.GetStartTimeInMicroseconds();
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0(
      "startup", "StartProcessWithAccess::LAUNCHPROCESS", trace_event_id);

  g_broker_services->SpawnTargetAsync(
      cmd_line.GetProgram().value().c_str(),
      cmd_line.GetCommandLineString().c_str(), std::move(policy),
      base::BindOnce(&SandboxWin::FinishStartSandboxedProcess, delegate,
                     std::move(timer), std::move(result_callback)));
  return SBOX_ALL_OK;
}

// static
void SandboxWin::FinishStartSandboxedProcess(
    SandboxDelegate* delegate,
    SandboxLaunchTimer timer,
    StartSandboxedProcessCallback result_callback,
    base::win::ScopedProcessInformation target,
    DWORD last_error,
    ResultCode result) {
  int64_t trace_event_id = timer.GetStartTimeInMicroseconds();
  TRACE_EVENT_NESTABLE_ASYNC_END0(
      "startup", "StartProcessWithAccess::LAUNCHPROCESS", trace_event_id);
  if (SBOX_ALL_OK != result) {
    base::UmaHistogramSparse("Process.Sandbox.Launch.Error", last_error);
    if (result == SBOX_ERROR_GENERIC) {
      DPLOG(ERROR) << "Failed to launch process";
    } else {
      DLOG(ERROR) << "Failed to launch process. Error: " << result;
    }
    std::move(result_callback).Run(base::Process(), last_error, result);
    return;
  }
  timer.OnProcessSpawned();

  delegate->PostSpawnTarget(target.process_handle());
  CHECK(ResumeThread(target.thread_handle()) != static_cast<DWORD>(-1));
  timer.OnProcessResumed();

  // Record timing histogram on sandboxed & launched success.
  if (SBOX_ALL_OK == result) {
    timer.RecordHistograms();
  }

  base::Process process(target.TakeProcessHandle());
  std::move(result_callback).Run(std::move(process), last_error, result);
}

// static
ResultCode SandboxWin::GetPolicyDiagnostics(
    base::OnceCallback<void(base::Value)> response) {
  CHECK(g_broker_services);
  CHECK(!response.is_null());
  auto receiver = std::make_unique<ServiceManagerDiagnosticsReceiver>(
      base::SequencedTaskRunner::GetCurrentDefault(), std::move(response));
  return g_broker_services->GetPolicyDiagnostics(std::move(receiver));
}

void BlocklistAddOneDllForTesting(const wchar_t* module_name,
                                  TargetConfig* config) {
  std::map<std::wstring, std::wstring> modules = GetShortNameModules();
  BlocklistAddOneDll(module_name, modules, config);
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
    case Sandbox::kNetwork:
      return "Network";
    case Sandbox::kOnDeviceModelExecution:
      return "On-Device Model Execution";
    case Sandbox::kCdm:
      return "CDM";
    case Sandbox::kPrintCompositor:
      return "Print Compositor";
#if BUILDFLAG(ENABLE_OOP_PRINTING)
    case Sandbox::kPrintBackend:
      return "Print Backend";
#endif
    case Sandbox::kAudio:
      return "Audio";
#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
    case Sandbox::kScreenAI:
      return "Screen AI";
#endif
    case Sandbox::kSpeechRecognition:
      return "Speech Recognition";
    case Sandbox::kPdfConversion:
      return "PDF Conversion";
    case Sandbox::kMediaFoundationCdm:
      return "Media Foundation CDM";
    case Sandbox::kService:
      return "Service";
    case Sandbox::kServiceWithJit:
      return "Service With Jit";
    case Sandbox::kIconReader:
      return "Icon Reader";
    case Sandbox::kWindowsSystemProxyResolver:
      return "Windows System Proxy Resolver";
  }
}

// static
std::string SandboxWin::GetSandboxTagForDelegate(
    std::string_view prefix,
    sandbox::mojom::Sandbox sandbox_type) {
  // sandbox.mojom.Sandbox has an operator << we can use for non-human values.
  std::ostringstream stream;
  stream << prefix << "!" << sandbox_type;
  return stream.str();
}

// static
std::optional<size_t> SandboxWin::GetJobMemoryLimit(Sandbox sandbox_type) {
#if defined(ARCH_CPU_64_BITS)
  size_t memory_limit = static_cast<size_t>(kDataSizeLimit);

  if (sandbox_type == Sandbox::kGpu || sandbox_type == Sandbox::kRenderer ||
      sandbox_type == Sandbox::kOnDeviceModelExecution) {
    constexpr uint64_t GB = 1024 * 1024 * 1024;
    // Allow the GPU/RENDERER process's sandbox to access more physical memory
    // if it's available on the system.
    //
    // Renderer processes are allowed to access 16 GB; the GPU process, up
    // to 64 GB.
    uint64_t physical_memory = base::SysInfo::AmountOfPhysicalMemory();
    if (sandbox_type == Sandbox::kGpu && physical_memory > 64 * GB) {
      memory_limit = 64 * GB;
    } else if (sandbox_type == Sandbox::kGpu && physical_memory > 32 * GB) {
      memory_limit = 32 * GB;
    } else if (physical_memory > 16 * GB) {
      memory_limit = 16 * GB;
    } else {
      memory_limit = 8 * GB;
    }

    if (sandbox_type == Sandbox::kRenderer) {
      // Set limit to 1Tb.
      memory_limit = 1024 * GB;
    }
  }
  return memory_limit;
#else
  return std::nullopt;
#endif
}

}  // namespace policy
}  // namespace sandbox
