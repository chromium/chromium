// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/sandbox_parameters_mac.h"

#include <unistd.h>

#include "base/check.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/mac/bundle_locations.h"
#include "base/mac/foundation_util.h"
#include "base/mac/mac_util.h"
#include "base/no_destructor.h"
#include "base/numerics/checked_math.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/system/sys_info.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "ppapi/buildflags/buildflags.h"
#include "sandbox/mac/seatbelt_exec.h"
#include "sandbox/policy/mac/params.h"
#include "sandbox/policy/mac/sandbox_mac.h"
#include "sandbox/policy/sandbox_type.h"
#include "sandbox/policy/switches.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#if BUILDFLAG(ENABLE_PLUGINS)
#include "content/public/common/pepper_plugin_info.h"
#endif

namespace content {

namespace {

absl::optional<base::FilePath>& GetNetworkTestCertsDirectory() {
  // Set by SetNetworkTestCertsDirectoryForTesting().
  static base::NoDestructor<absl::optional<base::FilePath>>
      network_test_certs_dir;
  return *network_test_certs_dir;
}

// Produce the OS version as an integer "1010", etc. and pass that to the
// profile. The profile converts the string back to a number and can do
// comparison operations on OS version.
std::string GetOSVersion() {
  int32_t major_version, minor_version, bugfix_version;
  base::SysInfo::OperatingSystemVersionNumbers(&major_version, &minor_version,
                                               &bugfix_version);
  base::CheckedNumeric<int32_t> os_version(major_version);
  os_version *= 100;
  os_version += minor_version;

  int32_t final_os_version = os_version.ValueOrDie();
  return std::to_string(final_os_version);
}

// Retrieves the users shared darwin dirs and adds it to the profile.
void AddDarwinDirs(sandbox::SeatbeltExecClient* client) {
  char dir_path[PATH_MAX + 1];

  size_t rv = confstr(_CS_DARWIN_USER_CACHE_DIR, dir_path, sizeof(dir_path));
  PCHECK(rv != 0);
  CHECK(client->SetParameter(
      sandbox::policy::kParamDarwinUserCacheDir,
      sandbox::policy::GetCanonicalPath(base::FilePath(dir_path)).value()));

  rv = confstr(_CS_DARWIN_USER_DIR, dir_path, sizeof(dir_path));
  PCHECK(rv != 0);
  CHECK(client->SetParameter(
      sandbox::policy::kParamDarwinUserDir,
      sandbox::policy::GetCanonicalPath(base::FilePath(dir_path)).value()));

  rv = confstr(_CS_DARWIN_USER_TEMP_DIR, dir_path, sizeof(dir_path));
  PCHECK(rv != 0);
  CHECK(client->SetParameter(
      sandbox::policy::kParamDarwinUserTempDir,
      sandbox::policy::GetCanonicalPath(base::FilePath(dir_path)).value()));
}

// All of the below functions populate the |client| with the parameters that the
// sandbox needs to resolve information that cannot be known at build time, such
// as the user's home directory.
void SetupCommonSandboxParameters(sandbox::SeatbeltExecClient* client) {
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  bool enable_logging =
      command_line->HasSwitch(sandbox::policy::switches::kEnableSandboxLogging);

  CHECK(client->SetBooleanParameter(sandbox::policy::kParamEnableLogging,
                                    enable_logging));
  CHECK(client->SetBooleanParameter(
      sandbox::policy::kParamDisableSandboxDenialLogging, !enable_logging));

  std::string bundle_path =
      sandbox::policy::GetCanonicalPath(base::mac::MainBundlePath()).value();
  CHECK(client->SetParameter(sandbox::policy::kParamBundlePath, bundle_path));

  std::string bundle_id = base::mac::BaseBundleID();
  DCHECK(!bundle_id.empty()) << "base::mac::OuterBundle is unset";
  CHECK(client->SetParameter(sandbox::policy::kParamBundleId, bundle_id));

  CHECK(client->SetParameter(sandbox::policy::kParamBrowserPid,
                             std::to_string(getpid())));

  std::string logging_path =
      GetContentClient()->browser()->GetLoggingFileName(*command_line).value();
  CHECK(client->SetParameter(sandbox::policy::kParamLogFilePath, logging_path));

#if defined(COMPONENT_BUILD)
  // For component builds, allow access to one directory level higher, where
  // the dylibs live.
  base::FilePath component_path = base::mac::MainBundlePath().Append("..");
  std::string component_path_canonical =
      sandbox::policy::GetCanonicalPath(component_path).value();
  CHECK(client->SetParameter(sandbox::policy::kParamComponentPath,
                             component_path_canonical));
#endif

  CHECK(client->SetParameter(sandbox::policy::kParamOsVersion, GetOSVersion()));

  std::string homedir =
      sandbox::policy::GetCanonicalPath(base::GetHomeDir()).value();
  CHECK(client->SetParameter(sandbox::policy::kParamHomedirAsLiteral, homedir));

  CHECK(client->SetBooleanParameter(
      sandbox::policy::kParamFilterSyscalls,
      base::FeatureList::IsEnabled(features::kMacSyscallSandbox)));

  CHECK(client->SetBooleanParameter(sandbox::policy::kParamFilterSyscallsDebug,
                                    false));
}

void SetupNetworkSandboxParameters(sandbox::SeatbeltExecClient* client) {
  SetupCommonSandboxParameters(client);

  std::vector<base::FilePath> storage_paths =
      GetContentClient()->browser()->GetNetworkContextsParentDirectory();

  AddDarwinDirs(client);

  CHECK(client->SetParameter(
      sandbox::policy::kParamNetworkServiceStoragePathsCount,
      base::NumberToString(storage_paths.size())));
  for (size_t i = 0; i < storage_paths.size(); ++i) {
    base::FilePath path = sandbox::policy::GetCanonicalPath(storage_paths[i]);
    std::string param_name = base::StringPrintf(
        "%s%zu", sandbox::policy::kParamNetworkServiceStoragePathN, i);
    CHECK(client->SetParameter(param_name, path.value())) << param_name;
  }

  if (GetNetworkTestCertsDirectory().has_value()) {
    CHECK(client->SetParameter(
        sandbox::policy::kParamNetworkServiceTestCertsDir,
        sandbox::policy::GetCanonicalPath(*GetNetworkTestCertsDirectory())
            .value()));
  }
}

#if BUILDFLAG(ENABLE_PLUGINS)
void SetupPPAPISandboxParameters(sandbox::SeatbeltExecClient* client) {
  SetupCommonSandboxParameters(client);

  std::vector<content::WebPluginInfo> plugins;
  PluginService::GetInstance()->GetInternalPlugins(&plugins);

  base::FilePath bundle_path =
      sandbox::policy::GetCanonicalPath(base::mac::MainBundlePath());

  const std::string param_base_name = "PPAPI_PATH_";
  int index = 0;
  for (const auto& plugin : plugins) {
    // Only add plugins which are external to Chrome's bundle to the profile.
    if (!bundle_path.IsParent(plugin.path) && plugin.path.IsAbsolute()) {
      std::string param_name =
          param_base_name + base::StringPrintf("%d", index++);
      CHECK(client->SetParameter(param_name, plugin.path.value()));
    }
  }

  // The profile does not support more than 4 PPAPI plugins, but it will be set
  // to n+1 more than the plugins added.
  CHECK(index <= 5);
}
#endif

void SetupGpuSandboxParameters(sandbox::SeatbeltExecClient* client,
                               const base::CommandLine& command_line) {
  SetupCommonSandboxParameters(client);
  AddDarwinDirs(client);
  CHECK(client->SetBooleanParameter(
      sandbox::policy::kParamDisableMetalShaderCache,
      command_line.HasSwitch(
          sandbox::policy::switches::kDisableMetalShaderCache)));
}

}  // namespace

void SetupSandboxParameters(sandbox::policy::SandboxType sandbox_type,
                            const base::CommandLine& command_line,
                            sandbox::SeatbeltExecClient* client) {
  switch (sandbox_type) {
    case sandbox::policy::SandboxType::kAudio:
    case sandbox::policy::SandboxType::kCdm:
    case sandbox::policy::SandboxType::kMirroring:
    case sandbox::policy::SandboxType::kNaClLoader:
    case sandbox::policy::SandboxType::kPrintBackend:
    case sandbox::policy::SandboxType::kPrintCompositor:
    case sandbox::policy::SandboxType::kRenderer:
    case sandbox::policy::SandboxType::kUtility:
      SetupCommonSandboxParameters(client);
      break;
    case sandbox::policy::SandboxType::kGpu: {
      SetupGpuSandboxParameters(client, command_line);
      break;
    }
    case sandbox::policy::SandboxType::kNetwork:
      SetupNetworkSandboxParameters(client);
      break;
    case sandbox::policy::SandboxType::kPpapi:
#if BUILDFLAG(ENABLE_PLUGINS)
      SetupPPAPISandboxParameters(client);
#endif
      break;
    case sandbox::policy::SandboxType::kNoSandbox:
    case sandbox::policy::SandboxType::kVideoCapture:
      CHECK(false) << "Unhandled parameters for sandbox_type "
                   << static_cast<int>(sandbox_type);
      break;
    // Setup parameters for sandbox types handled by embedders below.
    case sandbox::policy::SandboxType::kSpeechRecognition:
      SetupCommonSandboxParameters(client);
      CHECK(GetContentClient()->browser()->SetupEmbedderSandboxParameters(
          sandbox_type, client));
  }
}

void SetNetworkTestCertsDirectoryForTesting(const base::FilePath& path) {
  GetNetworkTestCertsDirectory().emplace(path);
}

}  // namespace content
