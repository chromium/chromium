// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webnn/webnn_compiler_process_host.h"

#include <string_view>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/service_process_host.h"
#include "sandbox/policy/switches.h"
#include "services/webnn/host/execution_provider_initializer.h"
#include "services/webnn/public/cpp/compiler_disconnect_reason.h"
#include "services/webnn/public/cpp/context_properties.h"
#include "services/webnn/public/mojom/features.mojom-features.h"
#include "services/webnn/public/mojom/webnn_compiler_context.mojom.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom.h"
#include "services/webnn/public/mojom/webnn_model_loader.mojom.h"

namespace content {

namespace {

// Maximum number of unexpected Compiler-process disconnects allowed before
// relaunch is blocked.
constexpr int kMaxCompilerCrashCount = 3;

// Number of times each WebNN Compiler process has crashed, keyed by device.
// Stop relaunching after too many crashes to avoid an infinite loop.
// This is intentionally file-scoped to keep process-wide crash accounting
// across WebNNCompilerProcessHost re-creation within the browser process.
base::flat_map<webnn::EpDeviceInfo, int>& GetWebNNCompilerCrashCounts() {
  static base::NoDestructor<base::flat_map<webnn::EpDeviceInfo, int>> counts;
  return *counts;
}

std::string_view WebnnDeviceTypeToString(webnn::mojom::Device device_type) {
  switch (device_type) {
    case webnn::mojom::Device::kCpu:
      return "CPU";
    case webnn::mojom::Device::kGpu:
      return "GPU";
    case webnn::mojom::Device::kNpu:
      return "NPU";
  }
}

// Formats a device for logging as "[<ep_name>, <DEVICE_TYPE>]".
std::string EpDeviceToString(const webnn::EpDeviceInfo& device) {
  return base::StrCat({"[", device.ep_name, ", ",
                       WebnnDeviceTypeToString(device.device_type), "]"});
}

// Launches the WebNN Compiler utility process and returns its mojo remote.
// `device_type` is the target device type for this process and included in the
// process display name.
mojo::Remote<webnn::mojom::WebNNCompilerService> LaunchCompilerProcess(
    webnn::mojom::Device device_type) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  ServiceProcessHost::Options options;
  options.WithDisplayName(base::StrCat(
      {"WebNN Compiler (", WebnnDeviceTypeToString(device_type), ")"}));

  // Only bypass MITIGATION_FORCE_MS_SIGNED_BINS when the browser was launched
  // with --allow-third-party-modules (for testing with non-MS-signed DLLs).
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          sandbox::policy::switches::kAllowThirdPartyModules)) {
    options.WithExtraCommandLineSwitches(
        {sandbox::policy::switches::kAllowThirdPartyModules});
  }

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          sandbox::policy::switches::kDisableWebNNCompilerSandbox)) {
    LOG(WARNING) << "[WebNN] Compiler sandbox is disabled";
  }

  return ServiceProcessHost::Launch<webnn::mojom::WebNNCompilerService>(
      std::move(options));
}

}  // namespace

WebNNCompilerProcessHost::WebNNCompilerProcessHost() = default;

WebNNCompilerProcessHost::~WebNNCompilerProcessHost() = default;

void WebNNCompilerProcessHost::RequestCompilerContext(
    webnn::mojom::CreateContextOptionsPtr context_options,
    const webnn::ContextProperties& context_properties,
    const webnn::EpDeviceInfo& target_device,
    mojo::PendingReceiver<webnn::mojom::WebNNCompilerContext>
        compiler_context_receiver,
    mojo::PendingRemote<webnn::mojom::WebNNModelLoader> model_loader_remote) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  const auto crash_it = GetWebNNCompilerCrashCounts().find(target_device);
  if ((crash_it != GetWebNNCompilerCrashCounts().end() &&
       crash_it->second >= kMaxCompilerCrashCount) ||
      !base::FeatureList::IsEnabled(
          webnn::mojom::features::kWebNNCompilerProcess) ||
      !base::FeatureList::IsEnabled(
          webnn::mojom::features::kWebNNOnnxRuntime)) {
    // Drop the pipe endpoints — peer endpoints will observe a disconnect.
    LOG(ERROR) << "[WebNN] RequestCompilerContext() failed: "
                  "WebNN Compiler process is disabled or has crashed too many "
                  "times for device "
               << EpDeviceToString(target_device);
    return;
  }

  // Always call this function to update the EP info since EPs in `NotPresent`
  // state may be added asynchronously after initialization.
  webnn::EnsureExecutionProvidersReady(base::BindOnce(
      &WebNNCompilerProcessHost::OnEpsResolvedForCompilerContext,
      weak_ptr_factory_.GetWeakPtr(), std::move(context_options),
      context_properties, target_device, std::move(compiler_context_receiver),
      std::move(model_loader_remote)));
}

void WebNNCompilerProcessHost::OnEpsResolvedForCompilerContext(
    webnn::mojom::CreateContextOptionsPtr context_options,
    const webnn::ContextProperties& context_properties,
    const webnn::EpDeviceInfo& target_device,
    mojo::PendingReceiver<webnn::mojom::WebNNCompilerContext>
        compiler_context_receiver,
    mojo::PendingRemote<webnn::mojom::WebNNModelLoader> model_loader_remote,
    base::flat_map<std::string, webnn::mojom::EpPackageInfoPtr>
        ep_package_info_map) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  const auto ep_it = ep_package_info_map.find(target_device.ep_name);
  if (ep_it == ep_package_info_map.end()) {
    // Drop the pipe endpoints — peer endpoints will observe a disconnect.
    LOG(ERROR) << "[WebNN] RequestCompilerContext() failed: "
                  "EP package info not found for device "
               << EpDeviceToString(target_device);
    return;
  }
  const base::FilePath& ep_library_path = ep_it->second->library_path;

  // Each EP device gets its own compiler process.
  auto& compiler_remote = webnn_compiler_remotes_[target_device];

  // Launch a new Compiler process for this device if not already running.
  if (!compiler_remote.is_bound()) {
    compiler_remote = LaunchCompilerProcess(target_device.device_type);
    // Compiler process could not be launched — peer endpoints will observe a
    // disconnect.
    if (!compiler_remote.is_bound()) {
      LOG(ERROR) << "[WebNN] RequestCompilerContext() failed: "
                    "WebNN Compiler process could not be launched for device "
                 << EpDeviceToString(target_device);
      return;
    }

    compiler_remote.set_disconnect_with_reason_handler(
        base::BindOnce(&WebNNCompilerProcessHost::OnDisconnected,
                       base::Unretained(this), target_device));
  }

  // Tell the Compiler process to create a per-context compiler state.
  // EP library path and target device information are forwarded via mojom so
  // the Compiler process can initialize its ORT Environment with the correct EP
  // device.
  // The CompilerContext receiver and ModelLoader remote are forwarded to the
  // Compiler process, completing the pipe connections.
  compiler_remote->CreateCompilerContext(
      std::move(context_options), context_properties, ep_library_path,
      target_device, std::move(model_loader_remote),
      std::move(compiler_context_receiver));
}

void WebNNCompilerProcessHost::OnDisconnected(
    const webnn::EpDeviceInfo& device_info,
    uint32_t reason,
    const std::string& description) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  webnn_compiler_remotes_.erase(device_info);

  // `reason` comes from a less-trusted child process. Verify it matches a
  // known disconnect reason before acting on it; treat any unrecognized
  // value as an unexpected crash.
  switch (reason) {
    case static_cast<uint32_t>(webnn::CompilerDisconnectReason::kIdleShutdown):
      // The compiler process shut down gracefully after all compiler
      // contexts disconnected and the idle timeout elapsed. Not a crash.
      DVLOG(1) << "[WebNN] Compiler process idle shutdown for: "
               << EpDeviceToString(device_info) << " (" << description << ").";
      return;
    default:
      break;
  }

  // Any other disconnect is unexpected and treated as a crash.
  int crash_count = ++GetWebNNCompilerCrashCounts()[device_info];
  base::UmaHistogramExactLinear(
      base::StrCat({"WebNN.CompilerProcess.CrashCount.", device_info.ep_name,
                    ".", WebnnDeviceTypeToString(device_info.device_type)}),
      crash_count, kMaxCompilerCrashCount + 1);

  LOG(ERROR) << "[WebNN] Compiler process disconnected unexpectedly for "
             << EpDeviceToString(device_info) << " (count: " << crash_count
             << ").";
}

}  // namespace content
