// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/utility/webnn/webnn_sandbox_init.h"

#include <optional>

#include "base/command_line.h"
#include "base/debug/leak_annotations.h"
#include "base/logging.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include "services/webnn/ort/environment.h"
#include "services/webnn/ort/ort_data_type.h"
#include "services/webnn/public/cpp/ep_device_info.h"
#include "services/webnn/public/mojom/ep_package_info.mojom.h"
#include "services/webnn/webnn_switches.h"  // nogncheck
#endif

namespace webnn {

#if BUILDFLAG(IS_WIN)

bool PreSandboxInit() {
  // Stage 2 of the WebNN compiler sandbox: load and one-time-initialize
  // the third-party execution-provider preload helper. This runs after
  // the broker has placed the process inside its LPAC (with the
  // chromeInstallFiles impersonation capability, so chrome.exe's install
  // directory is reachable for DLL loads) and just before LowerToken()
  // in utility_main.cc drops the token to USER_LOCKDOWN. See
  // content/browser/service_host/utility_sandbox_delegate_win.cc for
  // the full sandbox configuration.
  //
  // Today the only execution-provider backend is the ONNX Runtime, loaded
  // via ort::Environment::InitializeForCompilerProcess(). As additional
  // backends (LiteRT, etc.) come online, this function is the extension point
  // for their preload work; the surrounding sandbox lifecycle does not change.
  // TODO(crbug.com/500769395): Drive execution-provider discovery / preload
  // through whichever consolidated helper the WebNN compiler service
  // ultimately exposes.

  // Parse the target EP library path and device info from the command line. The
  // browser process passes them as two separate switches:
  //   --webnn-compiler-ep-library=<library_path>
  //   --webnn-compiler-ep-device-info=<EpDeviceInfo::ToSwitchValue()>
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();

  base::FilePath library_path =
      command_line->GetSwitchValuePath(switches::kWebNNCompilerEpLibrary);
  CHECK(!library_path.empty());

  EpDeviceInfo device_info = EpDeviceInfo::FromSwitchValue(
      command_line->GetSwitchValueASCII(switches::kWebNNCompilerEpDeviceInfo));

  auto env_result =
      ort::Environment::InitializeForCompilerProcess(library_path, device_info);
  if (!env_result.has_value()) {
    LOG(ERROR) << "[WebNN] Failed to initialize ORT Environment before "
                  "sandbox lockdown: "
               << env_result.error();
    return false;
  }

  // The ORT environment must stay resident for the entire lifetime of the
  // process to keep the required libraries loaded. Other callers should use
  // Environment::GetInstance() to get the instance.
  [[maybe_unused]] auto* leaked_env = env_result->release();
  ANNOTATE_LEAKING_OBJECT_PTR(leaked_env);

  return true;
}

#endif  // BUILDFLAG(IS_WIN)

}  // namespace webnn
