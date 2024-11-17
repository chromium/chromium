// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/policy/fuchsia/sandbox_policy_fuchsia.h"

#include <fidl/fuchsia.scheduler/cpp/fidl.h>
#include <fuchsia/buildinfo/cpp/fidl.h>
#include <fuchsia/camera3/cpp/fidl.h>
#include <fuchsia/fonts/cpp/fidl.h>
#include <fuchsia/hwinfo/cpp/fidl.h>
#include <fuchsia/intl/cpp/fidl.h>
#include <fuchsia/kernel/cpp/fidl.h>
#include <fuchsia/logger/cpp/fidl.h>
#include <fuchsia/media/cpp/fidl.h>
#include <fuchsia/mediacodec/cpp/fidl.h>
#include <fuchsia/memorypressure/cpp/fidl.h>
#include <fuchsia/net/interfaces/cpp/fidl.h>
#include <fuchsia/sysmem/cpp/fidl.h>
#include <fuchsia/tracing/perfetto/cpp/fidl.h>
#include <fuchsia/tracing/provider/cpp/fidl.h>
#include <fuchsia/ui/composition/cpp/fidl.h>
#include <lib/fdio/spawn.h>
#include <lib/sys/cpp/component_context.h>
#include <lib/sys/cpp/service_directory.h>
#include <stdio.h>
#include <zircon/processargs.h>
#include <zircon/syscalls/policy.h>

#include <memory>
#include <utility>

#include "base/base_paths.h"
#include "base/clang_profiling_buildflags.h"
#include "base/command_line.h"
#include "base/containers/span.h"
#include "base/files/file_util.h"
#include "base/fuchsia/default_job.h"
#include "base/fuchsia/file_utils.h"
#include "base/fuchsia/filtered_service_directory.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/process_context.h"
#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/thread.h"
#include "printing/buildflags/buildflags.h"
#include "sandbox/policy/mojom/sandbox.mojom.h"
#include "sandbox/policy/switches.h"

namespace sandbox {
namespace policy {
namespace {

enum SandboxFeature {
  // Provides access to resources required by Vulkan.
  kProvideVulkanResources = 1 << 1,

  // Read only access to /config/ssl, which contains root certs info.
  kProvideSslConfig = 1 << 2,
};

struct SandboxConfig {
  base::span<const char* const> services;
  uint32_t features;
};

// Services that are passed to all processes.
// Prevent incorrect indentation due to the preprocessor lines within `({...})`:
// clang-format off
constexpr auto kMinimalServices = base::make_span((const char* const[]){
    // TODO(crbug.com/40815933): Remove this and/or intl below if an alternative
    // solution does not require access to the service in all processes. For now
    // these services are made available everywhere because they are required by
    // base::SysInfo.
    fuchsia::buildinfo::Provider::Name_,
    fuchsia::hwinfo::Product::Name_,

// DebugData service is needed only for profiling.
#if BUILDFLAG(CLANG_PROFILING)
    "fuchsia.debugdata.Publisher",
#endif

    fuchsia::intl::PropertyProvider::Name_,
    fuchsia::logger::LogSink::Name_,
    fuchsia::tracing::perfetto::ProducerConnector::Name_,
});
// clang-format on

// For processes that only get kMinimalServices and no other capabilities.
constexpr SandboxConfig kMinimalConfig = {
    base::span<const char* const>(),
    0,
};

constexpr SandboxConfig kGpuConfig = {
    base::make_span((const char* const[]){
        // TODO(crbug.com/42050308): Use the fuchsia.scheduler API instead.
        fuchsia::media::ProfileProvider::Name_,
        fuchsia::mediacodec::CodecFactory::Name_,
        fidl::DiscoverableProtocolName<fuchsia_scheduler::RoleManager>,
        fuchsia::sysmem::Allocator::Name_,
        fuchsia::sysmem2::Allocator::Name_,
        "fuchsia.vulkan.loader.Loader",
        fuchsia::tracing::provider::Registry::Name_,
        fuchsia::ui::composition::Allocator::Name_,
        fuchsia::ui::composition::Flatland::Name_,
    }),
    kProvideVulkanResources,
};

constexpr SandboxConfig kNetworkConfig = {
    base::make_span((const char* const[]){
        "fuchsia.device.NameProvider",
        "fuchsia.net.name.Lookup",
        fuchsia::net::interfaces::State::Name_,
        "fuchsia.posix.socket.Provider",
    }),
    kProvideSslConfig,
};

constexpr SandboxConfig kRendererConfig = {
    base::make_span((const char* const[]){
        fuchsia::fonts::Provider::Name_,
        fuchsia::kernel::VmexResource::Name_,
        // TODO(crbug.com/42050308): Use the fuchsia.scheduler API instead.
        fuchsia::media::ProfileProvider::Name_,
        fuchsia::memorypressure::Provider::Name_,
        fidl::DiscoverableProtocolName<fuchsia_scheduler::RoleManager>,
        fuchsia::sysmem::Allocator::Name_,
        fuchsia::sysmem2::Allocator::Name_,
        fuchsia::ui::composition::Allocator::Name_,
    }),
    0,
};

constexpr SandboxConfig kVideoCaptureConfig = {
    base::make_span((const char* const[]){
        fuchsia::camera3::DeviceWatcher::Name_,
        fuchsia::sysmem::Allocator::Name_,
        fuchsia::sysmem2::Allocator::Name_,
    }),
    0,
};

constexpr SandboxConfig kServiceWithJitConfig = {
    base::make_span(
        (const char* const[]){fuchsia::kernel::VmexResource::Name_}),
    0,
};

const SandboxConfig* GetConfigForSandboxType(sandbox::mojom::Sandbox type) {
  switch (type) {
    case sandbox::mojom::Sandbox::kNoSandbox:
      return nullptr;
    case sandbox::mojom::Sandbox::kGpu:
      return &kGpuConfig;
    case sandbox::mojom::Sandbox::kNetwork:
      return &kNetworkConfig;
    case sandbox::mojom::Sandbox::kRenderer:
      return &kRendererConfig;
    case sandbox::mojom::Sandbox::kVideoCapture:
      return &kVideoCaptureConfig;
    case sandbox::mojom::Sandbox::kServiceWithJit:
      return &kServiceWithJitConfig;
    // Remaining types receive no-access-to-anything.
    case sandbox::mojom::Sandbox::kAudio:
    case sandbox::mojom::Sandbox::kCdm:
    case sandbox::mojom::Sandbox::kOnDeviceModelExecution:
#if BUILDFLAG(ENABLE_OOP_PRINTING)
    case sandbox::mojom::Sandbox::kPrintBackend:
#endif
    case sandbox::mojom::Sandbox::kPrintCompositor:
    case sandbox::mojom::Sandbox::kService:
    case sandbox::mojom::Sandbox::kSpeechRecognition:
    case sandbox::mojom::Sandbox::kUtility:
      return &kMinimalConfig;
  }
}

scoped_refptr<base::SequencedTaskRunner> GetServiceDirectoryTaskRunner() {
  static base::NoDestructor<base::Thread> service_directory_thread(
      "svc_directory");
  if (!service_directory_thread->IsRunning()) {
    base::Thread::Options options;
    options.message_pump_type = base::MessagePumpType::IO;
    CHECK(service_directory_thread->StartWithOptions(std::move(options)));
  }
  return service_directory_thread->task_runner();
}

void AddServiceCallback(const char* service_name, zx_status_t status) {
  ZX_CHECK(status == ZX_OK, status)
      << "AddService(" << service_name << ") failed";
}

void ConnectClientCallback(zx_status_t status) {
  ZX_CHECK(status == ZX_OK, status) << "ConnectClient failed";
}

}  // namespace

SandboxPolicyFuchsia::SandboxPolicyFuchsia(sandbox::mojom::Sandbox type) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kNoSandbox)) {
    type_ = sandbox::mojom::Sandbox::kNoSandbox;
  } else {
    type_ = type;
  }

  // If we need to pass some services for the given sandbox type then create
  // |sandbox_directory_| and initialize it with the corresponding list of
  // services. FilteredServiceDirectory must be initialized on a thread that has
  // an async_dispatcher.
  const SandboxConfig* config = GetConfigForSandboxType(type_);
  if (config) {
    filtered_service_directory_ =
        base::SequenceBound<base::FilteredServiceDirectory>(
            GetServiceDirectoryTaskRunner(),
            base::ComponentContextForProcess()->svc());
    for (const char* service_name : kMinimalServices) {
      // |service_name_|  points to a compile-time constant in
      // |kMinimalServices|. It will remain valid for the duration of the task.
      filtered_service_directory_
          .AsyncCall(&base::FilteredServiceDirectory::AddService)
          .WithArgs(service_name)
          .Then(base::BindOnce(&AddServiceCallback, service_name));
    }
    for (const char* service_name : config->services) {
      // |service_name_| comes from |config|, which points to a compile-time
      // constant. It will remain valid for the duration of the task.
      filtered_service_directory_
          .AsyncCall(&base::FilteredServiceDirectory::AddService)
          .WithArgs(service_name)
          .Then(base::BindOnce(&AddServiceCallback, service_name));
    }
    filtered_service_directory_
        .AsyncCall(&base::FilteredServiceDirectory::ConnectClient)
        .WithArgs(service_directory_client_.NewRequest())
        .Then(base::BindOnce(&ConnectClientCallback));
  }
}

SandboxPolicyFuchsia::~SandboxPolicyFuchsia() = default;

void SandboxPolicyFuchsia::UpdateLaunchOptionsForSandbox(
    base::LaunchOptions* options) {
  // Always clone stderr to get logs output.
  options->fds_to_remap.push_back(std::make_pair(STDERR_FILENO, STDERR_FILENO));
  options->fds_to_remap.push_back(std::make_pair(STDOUT_FILENO, STDOUT_FILENO));

  if (type_ == sandbox::mojom::Sandbox::kNoSandbox) {
    options->spawn_flags = FDIO_SPAWN_CLONE_NAMESPACE | FDIO_SPAWN_CLONE_JOB;
    options->clear_environment = false;
    return;
  }

  // Map /pkg (read-only files deployed from the package) into the child's
  // namespace.
  options->paths_to_clone.push_back(
      base::FilePath(base::kPackageRootDirectoryPath));

  // If /config/tzdata/icu/ exists then it contains up-to-date timezone
  // data which should be provided to all sub-processes, for consistency.
  // LINT.IfChange(icu_time_zone_data_path)
  const auto kIcuTimezoneDataPath = base::FilePath("/config/tzdata/icu");
  // LINT.ThenChange(//base/i18n/icu_util.cc:icu_time_zone_data_path)
  static bool icu_timezone_data_exists = base::PathExists(kIcuTimezoneDataPath);
  if (icu_timezone_data_exists) {
    options->paths_to_clone.push_back(kIcuTimezoneDataPath);
  }

  // Clear environmental variables to better isolate the child from
  // this process.
  options->clear_environment = true;

  // Don't clone anything by default.
  options->spawn_flags = 0;

  // Must get a config here as --no-sandbox bails out earlier.
  const SandboxConfig* config = GetConfigForSandboxType(type_);
  CHECK(config);

  if (config->features & kProvideSslConfig) {
    options->paths_to_clone.push_back(base::FilePath("/config/ssl"));
  }

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
}

}  // namespace policy
}  // namespace sandbox
