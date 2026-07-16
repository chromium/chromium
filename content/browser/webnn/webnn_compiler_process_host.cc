// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webnn/webnn_compiler_process_host.h"

#include <algorithm>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/service_process_host.h"
#include "content/public/browser/service_process_host_passkeys.h"
#include "sandbox/policy/switches.h"
#include "services/webnn/host/execution_provider_initializer.h"
#include "services/webnn/public/cpp/compiler_disconnect_reason.h"
#include "services/webnn/public/cpp/context_properties.h"
#include "services/webnn/public/cpp/execution_providers_info.h"
#include "services/webnn/public/cpp/webnn_device_util.h"
#include "services/webnn/public/mojom/features.mojom-features.h"
#include "services/webnn/public/mojom/webnn_compiler_context.mojom.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom.h"
#include "services/webnn/public/mojom/webnn_model_loader.mojom.h"
#include "services/webnn/webnn_switches.h"

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

// Returns the paths of the workaround libraries that must be preloaded for
// `target_device` before sandbox lockdown. The libraries live alongside the EP
// library, whose directory is `ep_library_dir`.
std::vector<base::FilePath> GetPreloadLibraryWorkaroundPaths(
    const webnn::EpDeviceInfo& target_device,
    const base::FilePath& ep_library_dir) {
  auto ep_it = webnn::kKnownEPs.find(target_device.ep_name);
  if (ep_it == webnn::kKnownEPs.end()) {
    return {};
  }
  const auto& offline_support = ep_it->second.offline_compilation_support;
  auto support_it =
      std::ranges::find(offline_support, target_device.device_type,
                        &webnn::OfflineCompilationSupport::device_type);
  if (support_it == offline_support.end()) {
    return {};
  }

  std::vector<base::FilePath> preload_library_paths;
  preload_library_paths.reserve(
      support_it->preload_libraries_workaround.size());
  for (std::string_view preload_library :
       support_it->preload_libraries_workaround) {
    preload_library_paths.push_back(
        ep_library_dir.AppendASCII(preload_library));
  }
  return preload_library_paths;
}

// Launches the WebNN Compiler utility process and returns its mojo remote.
// `ep_library_path` is the path to the EP library that will be loaded.
// `target_device` is the EP device that the Compiler process will work on.
mojo::Remote<webnn::mojom::WebNNCompilerService> LaunchCompilerProcess(
    const base::FilePath& ep_library_path,
    const webnn::EpDeviceInfo& target_device,
    ServiceProcessHostPreloadLibraries::PassKey pass_key) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  ServiceProcessHost::Options options;
  options.WithDisplayName(base::StrCat(
      {"WebNN Compiler (", webnn::DeviceTypeToString(target_device.device_type),
       ")"}));

  // Pass the target EP library path and device info via the command line so the
  // Compiler process can load the EP libraries and register the target EP in
  // PreSandboxInit().
  std::vector<std::pair<std::string, std::string>> extra_switch_key_values;
  extra_switch_key_values.reserve(2);
  extra_switch_key_values.emplace_back(
      switches::kWebNNCompilerEpLibrary,
      base::WideToUTF8(ep_library_path.value()));
  extra_switch_key_values.emplace_back(switches::kWebNNCompilerEpDeviceInfo,
                                       target_device.ToSwitchValue());
  options.WithExtraCommandLineSwitchKeyValues(
      std::move(extra_switch_key_values));

  // Only bypass MITIGATION_FORCE_MS_SIGNED_BINS when the browser was launched
  // with --allow-third-party-modules (for testing with non-MS-signed DLLs).
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          sandbox::policy::switches::kAllowThirdPartyModules)) {
    options.WithExtraCommandLineSwitches(
        {sandbox::policy::switches::kAllowThirdPartyModules});
  }

  // Preload the EP workaround libraries into the Compiler process before
  // sandbox lockdown, which is required because some EPs load these libraries
  // from a worker thread that runs under the lockdown token and would otherwise
  // fail. See services/webnn/public/cpp/execution_providers_info.h for details.
  // TODO(crbug.com/529544314): Remove once the EPs are fixed to load these
  // libraries from the main thread before sandbox lockdown.
  std::vector<base::FilePath> preload_libraries =
      GetPreloadLibraryWorkaroundPaths(target_device,
                                       ep_library_path.DirName());
  if (!preload_libraries.empty()) {
    options.WithPreloadedLibraries(std::move(preload_libraries), pass_key);
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

  auto it = GetWebNNCompilerCrashCounts().find(target_device);
  const int crash_count =
      it != GetWebNNCompilerCrashCounts().end() ? it->second : 0;
  if (crash_count >= kMaxCompilerCrashCount ||
      !base::FeatureList::IsEnabled(
          webnn::mojom::features::kWebNNCompilerProcess) ||
      !base::FeatureList::IsEnabled(
          webnn::mojom::features::kWebNNOnnxRuntime)) {
    // Drop the pipe endpoints — peer endpoints will observe a disconnect.
    LOG(ERROR) << "[WebNN] RequestCompilerContext() failed: "
                  "WebNN Compiler process is disabled or has crashed too many "
                  "times for ["
               << target_device.ToSwitchValue() << "].";
    return;
  }

  // Create the context directly if the Compiler process for this device is
  // already running.
  auto& compiler_remote = webnn_compiler_remotes_[target_device];
  if (compiler_remote.is_bound()) {
    compiler_remote->CreateCompilerContext(
        std::move(context_options), context_properties,
        std::move(model_loader_remote), std::move(compiler_context_receiver));
    return;
  }

  // Need to launch a new Compiler process. Resolve EPs first to get the
  // library path. Always call this function to update the EP package info since
  // EPs in `NotPresent` state may be added asynchronously after initialization.
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
                  "EP package info not found for ["
               << target_device.ToSwitchValue() << "].";
    return;
  }
  const base::FilePath& ep_library_path = ep_it->second->library_path;

  auto& compiler_remote = webnn_compiler_remotes_[target_device];
  if (!compiler_remote.is_bound()) {
    compiler_remote =
        LaunchCompilerProcess(ep_library_path, target_device,
                              ServiceProcessHostPreloadLibraries::GetPassKey());
    // Compiler process could not be launched — peer endpoints will observe a
    // disconnect.
    if (!compiler_remote.is_bound()) {
      LOG(ERROR) << "[WebNN] RequestCompilerContext() failed: "
                    "WebNN Compiler process could not be launched for ["
                 << target_device.ToSwitchValue() << "].";
      return;
    }

    compiler_remote.set_disconnect_with_reason_handler(
        base::BindOnce(&WebNNCompilerProcessHost::OnDisconnected,
                       base::Unretained(this), target_device));
  }

  // Tell the Compiler process to create a per-context compiler state.
  // The CompilerContext receiver and ModelLoader remote are forwarded to the
  // Compiler process, completing the pipe connections.
  compiler_remote->CreateCompilerContext(
      std::move(context_options), context_properties,
      std::move(model_loader_remote), std::move(compiler_context_receiver));
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
      // The Compiler process shut down gracefully after all compiler
      // contexts disconnected and the idle timeout elapsed. Not a crash.
      DVLOG(1) << "[WebNN] Compiler process idle shutdown for ["
               << device_info.ToSwitchValue() << "] (" << description << ").";
      return;
    default:
      break;
  }

  // Any other disconnect is unexpected and treated as a crash.
  int crash_count = ++GetWebNNCompilerCrashCounts()[device_info];
  base::UmaHistogramExactLinear(
      base::StrCat({"WebNN.CompilerProcess.CrashCount.", device_info.ep_name,
                    ".", webnn::DeviceTypeToString(device_info.device_type)}),
      crash_count, kMaxCompilerCrashCount + 1);

  LOG(ERROR) << "[WebNN] Compiler process disconnected unexpectedly for ["
             << device_info.ToSwitchValue() << "] (count: " << crash_count
             << ").";
}

}  // namespace content
