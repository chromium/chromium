// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/sandbox_parameters_mac.h"

#include <unistd.h>

#include <optional>

#include "base/apple/bundle_locations.h"
#include "base/apple/foundation_util.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/mac/mac_util.h"
#include "base/no_destructor.h"
#include "base/numerics/checked_math.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/system/sys_info.h"
#include "content/browser/mac_helpers.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "printing/buildflags/buildflags.h"
#include "sandbox/mac/sandbox_compiler.h"
#include "sandbox/policy/mac/params.h"
#include "sandbox/policy/mac/sandbox_mac.h"
#include "sandbox/policy/mojom/sandbox.mojom.h"
#include "sandbox/policy/switches.h"
#include "services/screen_ai/buildflags/buildflags.h"

namespace content {

namespace {

std::optional<base::FilePath>& GetNetworkTestCertsDirectory() {
  // Set by SetNetworkTestCertsDirectoryForTesting().
  static base::NoDestructor<std::optional<base::FilePath>>
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
  return base::NumberToString(final_os_version);
}

// Retrieves the users shared darwin dirs and adds it to the profile.
void AddDarwinDirs(sandbox::SandboxCompiler* compiler) {
  char dir_path[PATH_MAX + 1];

  size_t rv = confstr(_CS_DARWIN_USER_CACHE_DIR, dir_path, sizeof(dir_path));
  PCHECK(rv != 0);
  CHECK(compiler->SetParameter(
      sandbox::policy::kParamDarwinUserCacheDir,
      sandbox::policy::GetCanonicalPath(base::FilePath(dir_path)).value()));

  rv = confstr(_CS_DARWIN_USER_DIR, dir_path, sizeof(dir_path));
  PCHECK(rv != 0);
  CHECK(compiler->SetParameter(
      sandbox::policy::kParamDarwinUserDir,
      sandbox::policy::GetCanonicalPath(base::FilePath(dir_path)).value()));

  rv = confstr(_CS_DARWIN_USER_TEMP_DIR, dir_path, sizeof(dir_path));
  PCHECK(rv != 0);
  CHECK(compiler->SetParameter(
      sandbox::policy::kParamDarwinUserTempDir,
      sandbox::policy::GetCanonicalPath(base::FilePath(dir_path)).value()));
}

// All of the below functions populate the `compiler` with the parameters that
// the sandbox needs to resolve information that cannot be known at build time,
// such as the user's home directory.
void SetupCommonSandboxParameters(
    sandbox::SandboxCompiler* compiler,
    const base::CommandLine& target_command_line) {
  const base::CommandLine* browser_command_line =
      base::CommandLine::ForCurrentProcess();
  bool enable_logging = browser_command_line->HasSwitch(
      sandbox::policy::switches::kEnableSandboxLogging);

  CHECK(compiler->SetParameter(
      sandbox::policy::kParamExecutablePath,
      sandbox::policy::GetCanonicalPath(target_command_line.GetProgram())
          .value()));

  CHECK(compiler->SetBooleanParameter(sandbox::policy::kParamEnableLogging,
                                      enable_logging));
  CHECK(compiler->SetBooleanParameter(
      sandbox::policy::kParamDisableSandboxDenialLogging, !enable_logging));

  std::string bundle_path =
      sandbox::policy::GetCanonicalPath(base::apple::MainBundlePath()).value();
  CHECK(compiler->SetParameter(sandbox::policy::kParamBundlePath, bundle_path));

  std::string bundle_id = base::apple::BaseBundleID();
  DCHECK(!bundle_id.empty()) << "base::apple::OuterBundle is unset";
  CHECK(compiler->SetParameter(sandbox::policy::kParamBundleId, bundle_id));

  CHECK(compiler->SetParameter(sandbox::policy::kParamBrowserPid,
                               base::NumberToString(getpid())));

  std::string logging_path = GetContentClient()
                                 ->browser()
                                 ->GetLoggingFileName(*browser_command_line)
                                 .value();
  CHECK(
      compiler->SetParameter(sandbox::policy::kParamLogFilePath, logging_path));

#if defined(COMPONENT_BUILD)
  // For component builds, allow access to one directory level higher, where
  // the dylibs live.
  base::FilePath component_path = base::apple::MainBundlePath().Append("..");
  std::string component_path_canonical =
      sandbox::policy::GetCanonicalPath(component_path).value();
  CHECK(compiler->SetParameter(sandbox::policy::kParamComponentPath,
                               component_path_canonical));
#endif

  CHECK(
      compiler->SetParameter(sandbox::policy::kParamOsVersion, GetOSVersion()));

  std::string homedir =
      sandbox::policy::GetCanonicalPath(base::GetHomeDir()).value();
  CHECK(
      compiler->SetParameter(sandbox::policy::kParamHomedirAsLiteral, homedir));

  CHECK(compiler->SetBooleanParameter(
      sandbox::policy::kParamFilterSyscalls,
      base::FeatureList::IsEnabled(features::kMacSyscallSandbox)));

  CHECK(compiler->SetBooleanParameter(
      sandbox::policy::kParamFilterSyscallsDebug, false));
}

void SetupNetworkSandboxParameters(sandbox::SandboxCompiler* compiler,
                                   const base::CommandLine& command_line) {
  SetupCommonSandboxParameters(compiler, command_line);

  std::vector<base::FilePath> storage_paths =
      GetContentClient()->browser()->GetNetworkContextsParentDirectory();

  AddDarwinDirs(compiler);

  CHECK(compiler->SetParameter(
      sandbox::policy::kParamNetworkServiceStoragePathsCount,
      base::NumberToString(storage_paths.size())));
  for (size_t i = 0; i < storage_paths.size(); ++i) {
    base::FilePath path = sandbox::policy::GetCanonicalPath(storage_paths[i]);
    std::string param_name = base::StringPrintf(
        "%s%zu", sandbox::policy::kParamNetworkServiceStoragePathN, i);
    CHECK(compiler->SetParameter(param_name, path.value())) << param_name;
  }

  if (GetNetworkTestCertsDirectory().has_value()) {
    CHECK(compiler->SetParameter(
        sandbox::policy::kParamNetworkServiceTestCertsDir,
        sandbox::policy::GetCanonicalPath(*GetNetworkTestCertsDirectory())
            .value()));
  }
}

bool SetupGpuSandboxParameters(sandbox::SandboxCompiler* compiler,
                               const base::CommandLine& command_line) {
  SetupCommonSandboxParameters(compiler, command_line);
  AddDarwinDirs(compiler);
  CHECK(compiler->SetBooleanParameter(
      sandbox::policy::kParamDisableMetalShaderCache,
      command_line.HasSwitch(
          sandbox::policy::switches::kDisableMetalShaderCache)));

  base::FilePath helper_bundle_path =
      base::apple::GetInnermostAppBundlePath(command_line.GetProgram());

  // The helper may not be contained in an app bundle for unit tests.
  // In that case `kParamHelperBundleId` will remain unset.
  if (!helper_bundle_path.empty()) {
    @autoreleasepool {
      NSBundle* helper_bundle = [NSBundle
          bundleWithPath:base::SysUTF8ToNSString(helper_bundle_path.value())];
      if (!helper_bundle) {
        return false;
      }

      return compiler->SetParameter(
          sandbox::policy::kParamHelperBundleId,
          base::SysNSStringToUTF8(helper_bundle.bundleIdentifier));
    }
  }

  return true;
}

}  // namespace

bool SetupSandboxParameters(sandbox::mojom::Sandbox sandbox_type,
                            const base::CommandLine& command_line,
                            sandbox::SandboxCompiler* compiler) {
  switch (sandbox_type) {
    case sandbox::mojom::Sandbox::kAudio:
    case sandbox::mojom::Sandbox::kCdm:
    case sandbox::mojom::Sandbox::kMirroring:
#if BUILDFLAG(ENABLE_OOP_PRINTING)
    case sandbox::mojom::Sandbox::kPrintBackend:
#endif
    case sandbox::mojom::Sandbox::kPrintCompositor:
    case sandbox::mojom::Sandbox::kRenderer:
    case sandbox::mojom::Sandbox::kService:
    case sandbox::mojom::Sandbox::kServiceWithJit:
    case sandbox::mojom::Sandbox::kUtility:
      SetupCommonSandboxParameters(compiler, command_line);
      break;
    case sandbox::mojom::Sandbox::kOnDeviceModelExecution:
    case sandbox::mojom::Sandbox::kGpu:
      return SetupGpuSandboxParameters(compiler, command_line);
    case sandbox::mojom::Sandbox::kNetwork:
      SetupNetworkSandboxParameters(compiler, command_line);
      break;
    case sandbox::mojom::Sandbox::kNoSandbox:
      CHECK(false) << "Unhandled parameters for sandbox_type "
                   << static_cast<int>(sandbox_type);
      break;
    // Setup parameters for sandbox types handled by embedders below.
#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
    case sandbox::mojom::Sandbox::kScreenAI:
#endif
    case sandbox::mojom::Sandbox::kSpeechRecognition:
    case sandbox::mojom::Sandbox::kOnDeviceTranslation:
      SetupCommonSandboxParameters(compiler, command_line);
      CHECK(GetContentClient()->browser()->SetupEmbedderSandboxParameters(
          sandbox_type, compiler));
      break;
  }
  return true;
}

void SetNetworkTestCertsDirectoryForTesting(const base::FilePath& path) {
  GetNetworkTestCertsDirectory().emplace(path);
}

}  // namespace content
