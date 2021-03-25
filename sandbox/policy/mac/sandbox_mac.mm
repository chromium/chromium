// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/policy/mac/sandbox_mac.h"

#include <fcntl.h>
#include <sys/param.h>

#include <string>

#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "sandbox/policy/mac/audio.sb.h"
#include "sandbox/policy/mac/cdm.sb.h"
#include "sandbox/policy/mac/common.sb.h"
#include "sandbox/policy/mac/gpu.sb.h"
#include "sandbox/policy/mac/nacl_loader.sb.h"
#include "sandbox/policy/mac/network.sb.h"
#include "sandbox/policy/mac/ppapi.sb.h"
#include "sandbox/policy/mac/print_backend.sb.h"
#include "sandbox/policy/mac/print_compositor.sb.h"
#include "sandbox/policy/mac/renderer.sb.h"
#include "sandbox/policy/mac/speech_recognition.sb.h"
#include "sandbox/policy/mac/utility.sb.h"

namespace sandbox {
namespace policy {

const char* SandboxMac::kSandboxBrowserPID = "BROWSER_PID";
const char* SandboxMac::kSandboxBundlePath = "BUNDLE_PATH";
const char* SandboxMac::kSandboxChromeBundleId = "BUNDLE_ID";
const char* SandboxMac::kSandboxSodaComponentPath = "SODA_COMPONENT_PATH";
const char* SandboxMac::kSandboxSodaLanguagePackPath =
    "SODA_LANGUAGE_PACK_PATH";
const char* SandboxMac::kSandboxComponentPath = "COMPONENT_PATH";
const char* SandboxMac::kSandboxDisableDenialLogging =
    "DISABLE_SANDBOX_DENIAL_LOGGING";
const char* SandboxMac::kSandboxEnableLogging = "ENABLE_LOGGING";
const char* SandboxMac::kSandboxHomedirAsLiteral = "USER_HOMEDIR_AS_LITERAL";
const char* SandboxMac::kSandboxLoggingPathAsLiteral = "LOG_FILE_PATH";
const char* SandboxMac::kSandboxOSVersion = "OS_VERSION";
const char* SandboxMac::kSandboxBundleVersionPath = "BUNDLE_VERSION_PATH";
const char* SandboxMac::kSandboxDisableMetalShaderCache =
    "DISABLE_METAL_SHADER_CACHE";

// static
base::FilePath SandboxMac::GetCanonicalPath(const base::FilePath& path) {
  base::ScopedFD fd(HANDLE_EINTR(open(path.value().c_str(), O_RDONLY)));
  if (!fd.is_valid()) {
    DPLOG(ERROR) << "GetCanonicalSandboxPath() failed for: " << path.value();
    return path;
  }

  base::FilePath::CharType canonical_path[MAXPATHLEN];
  if (HANDLE_EINTR(fcntl(fd.get(), F_GETPATH, canonical_path)) != 0) {
    DPLOG(ERROR) << "GetCanonicalSandboxPath() failed for: " << path.value();
    return path;
  }

  return base::FilePath(canonical_path);
}

// static
std::string SandboxMac::GetSandboxProfile(SandboxType sandbox_type) {
  std::string profile = std::string(kSeatbeltPolicyString_common);

  switch (sandbox_type) {
    case SandboxType::kAudio:
      profile += kSeatbeltPolicyString_audio;
      break;
    case SandboxType::kCdm:
      profile += kSeatbeltPolicyString_cdm;
      break;
    case SandboxType::kGpu:
      profile += kSeatbeltPolicyString_gpu;
      break;
    case SandboxType::kNaClLoader:
      profile += kSeatbeltPolicyString_nacl_loader;
      break;
    case SandboxType::kNetwork:
      profile += kSeatbeltPolicyString_network;
      break;
    case SandboxType::kPpapi:
      profile += kSeatbeltPolicyString_ppapi;
      break;
    case SandboxType::kPrintBackend:
      profile += kSeatbeltPolicyString_print_backend;
      break;
    case SandboxType::kPrintCompositor:
      profile += kSeatbeltPolicyString_print_compositor;
      break;
    case SandboxType::kSpeechRecognition:
      profile += kSeatbeltPolicyString_speech_recognition;
      break;
    case SandboxType::kUtility:
      profile += kSeatbeltPolicyString_utility;
      break;
    case SandboxType::kRenderer:
      profile += kSeatbeltPolicyString_renderer;
      break;
    case SandboxType::kNoSandbox:
    case SandboxType::kVideoCapture:
      CHECK(false);
      break;
  }
  return profile;
}

}  // namespace policy
}  // namespace sandbox
