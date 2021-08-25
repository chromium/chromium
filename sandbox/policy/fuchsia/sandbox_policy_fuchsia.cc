// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/policy/fuchsia/sandbox_policy_fuchsia.h"

#include <lib/fdio/spawn.h>
#include <stdio.h>
#include <zircon/processargs.h>
#include <zircon/syscalls/policy.h>

#include <fuchsia/camera3/cpp/fidl.h>
#include <fuchsia/fonts/cpp/fidl.h>
#include <fuchsia/intl/cpp/fidl.h>
#include <fuchsia/logger/cpp/fidl.h>
#include <fuchsia/media/cpp/fidl.h>
#include <fuchsia/mediacodec/cpp/fidl.h>
#include <fuchsia/memorypressure/cpp/fidl.h>
#include <fuchsia/net/cpp/fidl.h>
#include <fuchsia/net/interfaces/cpp/fidl.h>
#include <fuchsia/sysmem/cpp/fidl.h>
#include <fuchsia/ui/scenic/cpp/fidl.h>
#include <lib/sys/cpp/component_context.h>
#include <lib/sys/cpp/service_directory.h>

#include <memory>
#include <utility>

#include "base/base_paths_fuchsia.h"
#include "base/clang_profiling_buildflags.h"
#include "base/command_line.h"
#include "base/containers/span.h"
#include "base/files/file_util.h"
#include "base/fuchsia/default_job.h"
#include "base/fuchsia/filtered_service_directory.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/process_context.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/threading/thread_task_runner_handle.h"
#include "sandbox/policy/switches.h"

namespace sandbox {
namespace policy {
namespace {

enum SandboxFeature {
  // Clones the job. This is required to start new processes (to make it useful
  // the process will also need access to the fuchsia.process.Launcher service).
  kCloneJob = 1 << 0,

  // Provides access to resources required by Vulkan.
  kProvideVulkanResources = 1 << 1,

  // Read only access to /config/ssl, which contains root certs info.
  kProvideSslConfig = 1 << 2,

  // Allows the process to use the ambient mark-vmo-as-executable capability.
  kAmbientMarkVmoAsExecutable = 1 << 3,
};

struct SandboxConfig {
  base::span<const char* const> services;
  uint32_t features;
};

constexpr SandboxConfig kGpuConfig = {
    base::make_span((const char* const[]){
        // TODO(crbug.com/1224707): Use the fuchsia.scheduler API instead.
        fuchsia::media::ProfileProvider::Name_,
        fuchsia::sysmem::Allocator::Name_,
        "fuchsia.vulkan.loader.Loader",
        fuchsia::ui::scenic::Scenic::Name_,
    }),
    kProvideVulkanResources,
};

constexpr SandboxConfig kNetworkConfig = {
    base::make_span((const char* const[]){
        fuchsia::net::NameLookup::Name_,
        "fuchsia.net.name.Lookup",
        fuchsia::net::interfaces::State::Name_,
        "fuchsia.posix.socket.Provider",
    }),
    kProvideSslConfig,
};

constexpr SandboxConfig kRendererConfig = {
    base::make_span((const char* const[]){
        fuchsia::fonts::Provider::Name_,
        // TODO(crbug.com/1224707): Use the fuchsia.scheduler API instead.
        fuchsia::media::ProfileProvider::Name_,
        fuchsia::mediacodec::CodecFactory::Name_,
        fuchsia::memorypressure::Provider::Name_,
        fuchsia::sysmem::Allocator::Name_,
    }),
    kAmbientMarkVmoAsExecutable,
};

constexpr SandboxConfig kVideoCaptureConfig = {
    base::make_span((const char* const[]){
        fuchsia::camera3::DeviceWatcher::Name_,
        fuchsia::sysmem::Allocator::Name_,
    }),
    0,
};

// No-access-to-anything.
constexpr SandboxConfig kEmptySandboxConfig = {
    base::span<const char* const>(),
    0,
};

const SandboxConfig* GetConfigForSandboxType(SandboxType type) {
  switch (type) {
    case SandboxType::kNoSandbox:
      return nullptr;
    case SandboxType::kGpu:
      return &kGpuConfig;
    case SandboxType::kNetwork:
      return &kNetworkConfig;
    case SandboxType::kRenderer:
      return &kRendererConfig;
    case SandboxType::kVideoCapture:
      return &kVideoCaptureConfig;
    // Remaining types receive no-access-to-anything.
    case SandboxType::kAudio:
    case SandboxType::kCdm:
    case SandboxType::kPrintCompositor:
    case SandboxType::kService:
    case SandboxType::kSpeechRecognition:
    case SandboxType::kUtility:
      return &kEmptySandboxConfig;
  }
}

// Services that are passed to all processes.
constexpr auto kDefaultServices = base::make_span((const char* const[]) {
// DebugData service is needed only for profiling.
#if BUILDFLAG(CLANG_PROFILING)
  "fuchsia.debugdata.DebugData",
#endif
      fuchsia::intl::PropertyProvider::Name_, fuchsia::logger::LogSink::Name_
});

}  // namespace

SandboxPolicyFuchsia::SandboxPolicyFuchsia(SandboxType type) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kNoSandbox)) {
    type_ = SandboxType::kNoSandbox;
  } else {
    type_ = type;
  }

  // If we need to pass some services for the given sandbox type then create
  // |sandbox_directory_| and initialize it with the corresponding list of
  // services. FilteredServiceDirectory must be initialized on a thread that has
  // async_dispatcher.
  const SandboxConfig* config = GetConfigForSandboxType(type_);
  if (config) {
    service_directory_task_runner_ = base::ThreadTaskRunnerHandle::Get();
    service_directory_ = std::make_unique<base::FilteredServiceDirectory>(
        base::ComponentContextForProcess()->svc().get());
    for (const char* service_name : kDefaultServices) {
      zx_status_t status = service_directory_->AddService(service_name);
      ZX_CHECK(status == ZX_OK, status)
          << "AddService(" << service_name << ") failed";
    }
    for (const char* service_name : config->services) {
      zx_status_t status = service_directory_->AddService(service_name);
      ZX_CHECK(status == ZX_OK, status)
          << "AddService(" << service_name << ") failed";
    }
    // Bind the service directory and store the client channel for
    // UpdateLaunchOptionsForSandbox()'s use.
    zx_status_t status = service_directory_->ConnectClient(
        service_directory_client_.NewRequest());
    ZX_CHECK(status == ZX_OK, status) << "ConnectClient failed";
    CHECK(service_directory_client_);
  }
}

SandboxPolicyFuchsia::~SandboxPolicyFuchsia() {
  if (service_directory_) {
    service_directory_task_runner_->DeleteSoon(FROM_HERE,
                                               std::move(service_directory_));
  }
}


void SandboxPolicyFuchsia::UpdateLaunchOptionsForSandbox(
    base::LaunchOptions* options) {
  // Always clone stderr to get logs output.
  options->fds_to_remap.push_back(std::make_pair(STDERR_FILENO, STDERR_FILENO));
  options->fds_to_remap.push_back(std::make_pair(STDOUT_FILENO, STDOUT_FILENO));

  if (type_ == SandboxType::kNoSandbox) {
    options->spawn_flags = FDIO_SPAWN_CLONE_NAMESPACE | FDIO_SPAWN_CLONE_JOB;
    options->clear_environment = false;
    return;
  }

  // Map /pkg (read-only files deployed from the package) into the child's
  // namespace.
  base::FilePath package_root;
  base::PathService::Get(base::DIR_ASSETS, &package_root);
  options->paths_to_clone.push_back(package_root);

  // If /config/data/tzdata/icu/ exists then it contains up-to-date timezone
  // data which should be provided to all sub-processes, for consistency.
  const auto kIcuTimezoneDataPath = base::FilePath("/config/data/tzdata/icu");
  static bool icu_timezone_data_exists = base::PathExists(kIcuTimezoneDataPath);
  if (icu_timezone_data_exists)
    options->paths_to_clone.push_back(kIcuTimezoneDataPath);

  // Clear environmental variables to better isolate the child from
  // this process.
  options->clear_environment = true;

  // Don't clone anything by default.
  options->spawn_flags = 0;

  // Must get a config here as --no-sandbox bails out earlier.
  const SandboxConfig* config = GetConfigForSandboxType(type_);
  CHECK(config);

  if (config->features & kCloneJob)
    options->spawn_flags |= FDIO_SPAWN_CLONE_JOB;

  if (config->features & kProvideSslConfig)
    options->paths_to_clone.push_back(base::FilePath("/config/ssl"));

  if (config->features & kProvideVulkanResources) {
    static const char* const kPathsToCloneForVulkan[] = {
        // Used configure and access the GPU.
        "/dev/class/gpu", "/config/vulkan/icd.d",
        // Used for Fuchsia Emulator.
        "/dev/class/goldfish-address-space", "/dev/class/goldfish-control",
        "/dev/class/goldfish-pipe", "/dev/class/goldfish-sync"};
    for (const char* path_str : kPathsToCloneForVulkan) {
      base::FilePath path(path_str);
      // Vulkan paths aren't needed with newer Fuchsia versions, so they may not
      // be available.
      if (base::PathExists(path)) {
        options->paths_to_clone.push_back(path);
      }
    }
  }

  // If the process needs access to any services then transfer the
  // |service_directory_client_| handle for it to mount at "/svc".
  if (service_directory_client_) {
    options->paths_to_transfer.push_back(base::PathToTransfer{
        base::FilePath("/svc"),
        service_directory_client_.TakeChannel().release()});
  }

  // Isolate the child process from the call by launching it in its own job.
  zx_status_t status = zx::job::create(*base::GetDefaultJob(), 0, &job_);
  ZX_CHECK(status == ZX_OK, status) << "zx_job_create";
  options->job_handle = job_.get();

  // Only allow ambient VMO mark-as-executable capability to be granted
  // to processes that which need to JIT (i.e. do not run V8/WASM).
  zx_policy_basic_v2_t ambient_mark_vmo_exec{
      ZX_POL_AMBIENT_MARK_VMO_EXEC,

      // Kill processes which attempt to execute writable VMOs but lack the
      // right to do so.
      (config->features & kAmbientMarkVmoAsExecutable) ? ZX_POL_ACTION_ALLOW
                                                       : ZX_POL_ACTION_KILL,

      // Only grant spawn-capable processes, such as the browser process, the
      // ability to override the execution policy.
      (config->features & kCloneJob) ? ZX_POL_OVERRIDE_ALLOW
                                     : ZX_POL_OVERRIDE_DENY};

  status = job_.set_policy(ZX_JOB_POL_ABSOLUTE, ZX_JOB_POL_BASIC_V2,
                           &ambient_mark_vmo_exec, 1);
  ZX_CHECK(status == ZX_OK, status) << "zx_job_set_policy";
}

}  // namespace policy
}  // namespace sandbox
