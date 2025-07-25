// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/policy/mac/sandbox_mac.h"

#include <fcntl.h>
#include <sys/param.h>

#include <string>

#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/posix/eintr_wrapper.h"
#include "sandbox/policy/mac/audio.sb.h"
#include "sandbox/policy/mac/cdm.sb.h"
#include "sandbox/policy/mac/common.sb.h"
#include "sandbox/policy/mac/gpu.sb.h"
#include "sandbox/policy/mac/mirroring.sb.h"
#include "sandbox/policy/mac/network.sb.h"
#include "sandbox/policy/mac/on_device_model_execution.sb.h"
#include "sandbox/policy/mac/on_device_translation.sb.h"
#include "sandbox/policy/mac/print_backend.sb.h"
#include "sandbox/policy/mac/print_compositor.sb.h"
#include "sandbox/policy/mac/renderer.sb.h"
#include "sandbox/policy/mac/screen_ai.sb.h"
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
  std::string profile_suffix = [](sandbox::mojom::Sandbox sandbox_type) {
    switch (sandbox_type) {
      case sandbox::mojom::Sandbox::kAudio:
        return kSeatbeltPolicyString_audio;
      case sandbox::mojom::Sandbox::kCdm:
        return kSeatbeltPolicyString_cdm;
      case sandbox::mojom::Sandbox::kGpu:
        return kSeatbeltPolicyString_gpu;
      case sandbox::mojom::Sandbox::kMirroring:
        return kSeatbeltPolicyString_mirroring;
      case sandbox::mojom::Sandbox::kNetwork:
        return kSeatbeltPolicyString_network;
      case sandbox::mojom::Sandbox::kPrintBackend:
        return kSeatbeltPolicyString_print_backend;
      case sandbox::mojom::Sandbox::kPrintCompositor:
        return kSeatbeltPolicyString_print_compositor;
      case sandbox::mojom::Sandbox::kScreenAI:
        return kSeatbeltPolicyString_screen_ai;
      case sandbox::mojom::Sandbox::kSpeechRecognition:
        return kSeatbeltPolicyString_speech_recognition;
      case sandbox::mojom::Sandbox::kOnDeviceModelExecution:
        return kSeatbeltPolicyString_on_device_model_execution;
      case sandbox::mojom::Sandbox::kOnDeviceTranslation:
        return kSeatbeltPolicyString_on_device_translation;
      // `kService` and `kUtility` are the same on OS_MAC, so fallthrough.
      case sandbox::mojom::Sandbox::kService:
      case sandbox::mojom::Sandbox::kServiceWithJit:
      case sandbox::mojom::Sandbox::kUtility:
        return kSeatbeltPolicyString_utility;
      case sandbox::mojom::Sandbox::kRenderer:
        return kSeatbeltPolicyString_renderer;
      case sandbox::mojom::Sandbox::kNoSandbox:
        NOTREACHED();
    }
    NOTREACHED();
  }(sandbox_type);

  return kSeatbeltPolicyString_common + profile_suffix;
}

bool CanCacheSandboxPolicy(sandbox::mojom::Sandbox sandbox_type) {
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
