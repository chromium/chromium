// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/policy/mac/sandbox_mac.h"

#include <fcntl.h>
#include <sys/param.h>

#include <string>

#include "base/feature_list.h"
#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "printing/buildflags/buildflags.h"
#include "sandbox/policy/features.h"
#include "sandbox/policy/mac/audio.sb.h"
#include "sandbox/policy/mac/cdm.sb.h"
#include "sandbox/policy/mac/common.sb.h"
#include "sandbox/policy/mac/gpu.sb.h"
#include "sandbox/policy/mac/mirroring.sb.h"
#include "sandbox/policy/mac/network.sb.h"
#include "sandbox/policy/mac/on_device_model_execution.sb.h"
#include "services/screen_ai/buildflags/buildflags.h"
#if BUILDFLAG(ENABLE_OOP_PRINTING)
#include "sandbox/policy/mac/print_backend.sb.h"
#endif
#include "sandbox/policy/mac/print_compositor.sb.h"
#include "sandbox/policy/mac/renderer.sb.h"
#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
#include "sandbox/policy/mac/screen_ai.sb.h"
#endif
#include "sandbox/policy/mac/on_device_translation.sb.h"
#include "sandbox/policy/mac/speech_recognition.sb.h"
#include "sandbox/policy/mac/utility.sb.h"
#include "sandbox/policy/mojom/sandbox.mojom.h"

namespace sandbox::policy {

base::FilePath GetCanonicalPath(const base::FilePath& path) {
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

std::string GetSandboxProfile(sandbox::mojom::Sandbox sandbox_type) {
  std::string profile = std::string(kSeatbeltPolicyString_common);

  switch (sandbox_type) {
    case sandbox::mojom::Sandbox::kAudio:
      profile += kSeatbeltPolicyString_audio;
      break;
    case sandbox::mojom::Sandbox::kCdm:
      profile += kSeatbeltPolicyString_cdm;
      break;
    case sandbox::mojom::Sandbox::kGpu:
      profile += kSeatbeltPolicyString_gpu;
      break;
    case sandbox::mojom::Sandbox::kMirroring:
      profile += kSeatbeltPolicyString_mirroring;
      break;
    case sandbox::mojom::Sandbox::kNetwork:
      profile += kSeatbeltPolicyString_network;
      break;
#if BUILDFLAG(ENABLE_OOP_PRINTING)
    case sandbox::mojom::Sandbox::kPrintBackend:
      profile += kSeatbeltPolicyString_print_backend;
      break;
#endif
    case sandbox::mojom::Sandbox::kPrintCompositor:
      profile += kSeatbeltPolicyString_print_compositor;
      break;
#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
    case sandbox::mojom::Sandbox::kScreenAI:
      profile += kSeatbeltPolicyString_screen_ai;
      break;
#endif
    case sandbox::mojom::Sandbox::kSpeechRecognition:
      profile += kSeatbeltPolicyString_speech_recognition;
      break;
    case sandbox::mojom::Sandbox::kOnDeviceModelExecution:
      profile += kSeatbeltPolicyString_on_device_model_execution;
      break;
    case sandbox::mojom::Sandbox::kOnDeviceTranslation:
      profile += kSeatbeltPolicyString_on_device_translation;
      break;
    // kService and kUtility are the same on OS_MAC, so fallthrough.
    case sandbox::mojom::Sandbox::kService:
    case sandbox::mojom::Sandbox::kServiceWithJit:
    case sandbox::mojom::Sandbox::kUtility:
      profile += kSeatbeltPolicyString_utility;
      break;
    case sandbox::mojom::Sandbox::kRenderer:
      profile += kSeatbeltPolicyString_renderer;
      break;
    case sandbox::mojom::Sandbox::kNoSandbox:
      CHECK(false);
      break;
  }
  return profile;
}

bool CanCacheSandboxPolicy(sandbox::mojom::Sandbox sandbox_type) {
  static const bool feature_enabled =
      base::FeatureList::IsEnabled(features::kCacheMacSandboxProfiles);
  if (!feature_enabled)
    return false;

  switch (sandbox_type) {
    case sandbox::mojom::Sandbox::kRenderer:
    case sandbox::mojom::Sandbox::kService:
    case sandbox::mojom::Sandbox::kServiceWithJit:
    case sandbox::mojom::Sandbox::kUtility:
      return true;
    default:
      return false;
  }
}

}  // namespace sandbox::policy
