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
#include "base/optional.h"
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
#include "sandbox/policy/mac/sandbox_mac.h"
#include "sandbox/policy/sandbox_type.h"
#include "sandbox/policy/switches.h"

#if BUILDFLAG(ENABLE_PLUGINS)
#include "content/public/common/pepper_plugin_info.h"
#endif

namespace content {

namespace {

// Set by SetNetworkTestCertsDirectoryForTesting().
base::NoDestructor<base::Optional<base::FilePath>> g_network_test_certs_dir;

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
      "DARWIN_USER_CACHE_DIR",
      sandbox::policy::SandboxMac::GetCanonicalPath(base::FilePath(dir_path))
          .value()));

  rv = confstr(_CS_DARWIN_USER_DIR, dir_path, sizeof(dir_path));
  PCHECK(rv != 0);
  CHECK(client->SetParameter(
      "DARWIN_USER_DIR",
      sandbox::policy::SandboxMac::GetCanonicalPath(base::FilePath(dir_path))
          .value()));

  rv = confstr(_CS_DARWIN_USER_TEMP_DIR, dir_path, sizeof(dir_path));
  PCHECK(rv != 0);
  CHECK(client->SetParameter(
      "DARWIN_USER_TEMP_DIR",
      sandbox::policy::SandboxMac::GetCanonicalPath(base::FilePath(dir_path))
          .value()));
}

// All of the below functions populate the |client| with the parameters that the
// sandbox needs to resolve information that cannot be known at build time, such
// as the user's home directory.
void SetupCommonSandboxParameters(sandbox::SeatbeltExecClient* client) {
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  bool enable_logging =
      command_line->HasSwitch(sandbox::policy::switches::kEnableSandboxLogging);

  CHECK(client->SetBooleanParameter(
      sandbox::policy::SandboxMac::kSandboxEnableLogging, enable_logging));
  CHECK(client->SetBooleanParameter(
      sandbox::policy::SandboxMac::kSandboxDisableDenialLogging,
      !enable_logging));

  std::string bundle_path =
      sandbox::policy::SandboxMac::GetCanonicalPath(base::mac::MainBundlePath())
          .value();
  CHECK(client->SetParameter(sandbox::policy::SandboxMac::kSandboxBundlePath,
                             bundle_path));

  std::string bundle_id = base::mac::BaseBundleID();
  DCHECK(!bundle_id.empty()) << "base::mac::OuterBundle is unset";
  CHECK(client->SetParameter(
      sandbox::policy::SandboxMac::kSandboxChromeBundleId, bundle_id));

  CHECK(client->SetParameter(sandbox::policy::SandboxMac::kSandboxBrowserPID,
                             std::to_string(getpid())));

  std::string logging_path =
      GetContentClient()->browser()->GetLoggingFileName(*command_line).value();
  CHECK(client->SetParameter(
      sandbox::policy::SandboxMac::kSandboxLoggingPathAsLiteral, logging_path));

#if defined(COMPONENT_BUILD)
  // For component builds, allow access to one directory level higher, where
  // the dylibs live.
  base::FilePath component_path = base::mac::MainBundlePath().Append("..");
  std::string component_path_canonical =
      sandbox::policy::SandboxMac::GetCanonicalPath(component_path).value();
  CHECK(client->SetParameter(sandbox::policy::SandboxMac::kSandboxComponentPath,
                             component_path_canonical));
#endif

  CHECK(client->SetParameter(sandbox::policy::SandboxMac::kSandboxOSVersion,
                             GetOSVersion()));

  std::string homedir =
      sandbox::policy::SandboxMac::GetCanonicalPath(base::GetHomeDir()).value();
  CHECK(client->SetParameter(
      sandbox::policy::SandboxMac::kSandboxHomedirAsLiteral, homedir));

  CHECK(client->SetBooleanParameter(
      "FILTER_SYSCALLS",
      base::FeatureList::IsEnabled(features::kMacSyscallSandbox)));

  CHECK(client->SetBooleanParameter("FILTER_SYSCALLS_DEBUG", false));
}

void SetupNetworkSandboxParameters(sandbox::SeatbeltExecClient* client) {
  SetupCommonSandboxParameters(client);

  std::vector<base::FilePath> storage_paths =
      GetContentClient()->browser()->GetNetworkContextsParentDirectory();

  AddDarwinDirs(client);

  CHECK(client->SetParameter("NETWORK_SERVICE_STORAGE_PATHS_COUNT",
                             base::NumberToString(storage_paths.size())));
  for (size_t i = 0; i < storage_paths.size(); ++i) {
    base::FilePath path =
        sandbox::policy::SandboxMac::GetCanonicalPath(storage_paths[i]);
    std::string param_name =
        base::StringPrintf("NETWORK_SERVICE_STORAGE_PATH_%zu", i);
    CHECK(client->SetParameter(param_name, path.value())) << param_name;
  }

  if (g_network_test_certs_dir->has_value()) {
    CHECK(client->SetParameter("NETWORK_SERVICE_TEST_CERTS_DIR",
                               sandbox::policy::SandboxMac::GetCanonicalPath(
                                   **g_network_test_certs_dir)
                                   .value()));
  }
}

#if BUILDFLAG(ENABLE_PLUGINS)
void SetupPPAPISandboxParameters(sandbox::SeatbeltExecClient* client) {
  SetupCommonSandboxParameters(client);

  std::vector<content::WebPluginInfo> plugins;
  PluginService::GetInstance()->GetInternalPlugins(&plugins);

  base::FilePath bundle_path = sandbox::policy::SandboxMac::GetCanonicalPath(
      base::mac::MainBundlePath());

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

void SetupCDMSandboxParameters(sandbox::SeatbeltExecClient* client) {
  SetupCommonSandboxParameters(client);

  base::FilePath bundle_path = sandbox::policy::SandboxMac::GetCanonicalPath(
      base::mac::FrameworkBundlePath().DirName());
  CHECK(!bundle_path.empty());

  CHECK(client->SetParameter(
      sandbox::policy::SandboxMac::kSandboxBundleVersionPath,
      bundle_path.value()));
}

void SetupUtilitySandboxParameters(sandbox::SeatbeltExecClient* client,
                                   const base::CommandLine& command_line) {
  SetupCommonSandboxParameters(client);
}

}  // namespace

void SetupSandboxParameters(sandbox::policy::SandboxType sandbox_type,
                            const base::CommandLine& command_line,
                            sandbox::SeatbeltExecClient* client) {
  switch (sandbox_type) {
    case sandbox::policy::SandboxType::kAudio:
    case sandbox::policy::SandboxType::kNaClLoader:
    case sandbox::policy::SandboxType::kPrintCompositor:
    case sandbox::policy::SandboxType::kRenderer:
      SetupCommonSandboxParameters(client);
      break;
    case sandbox::policy::SandboxType::kGpu:
      SetupCommonSandboxParameters(client);
      AddDarwinDirs(client);
      break;
    case sandbox::policy::SandboxType::kCdm:
      SetupCDMSandboxParameters(client);
      break;
    case sandbox::policy::SandboxType::kNetwork:
      SetupNetworkSandboxParameters(client);
      break;
    case sandbox::policy::SandboxType::kPpapi:
#if BUILDFLAG(ENABLE_PLUGINS)
      SetupPPAPISandboxParameters(client);
#endif
      break;
    case sandbox::policy::SandboxType::kUtility:
      SetupUtilitySandboxParameters(client, command_line);
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
  g_network_test_certs_dir->emplace(path);
}

}  // namespace content
