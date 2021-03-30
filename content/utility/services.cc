// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/utility/services.h"

#include <utility>

#include "base/command_line.h"
#include "base/no_destructor.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/services/storage/public/mojom/storage_service.mojom.h"
#include "components/services/storage/storage_service_impl.h"
#include "content/child/child_process.h"
#include "content/public/common/content_switches.h"
#include "content/public/utility/content_utility_client.h"
#include "content/public/utility/utility_thread.h"
#include "device/vr/buildflags/buildflags.h"
#include "media/media_buildflags.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/bindings/service_factory.h"
#include "services/audio/service_factory.h"
#include "services/data_decoder/data_decoder_service.h"
#include "services/network/network_service.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/tracing/public/mojom/tracing_service.mojom.h"
#include "services/tracing/tracing_service.h"
#include "services/video_capture/public/mojom/video_capture_service.mojom.h"
#include "services/video_capture/video_capture_service_impl.h"

#if defined(OS_MAC)
#include "base/mac/mach_logging.h"
#include "sandbox/mac/system_services.h"
#include "sandbox/policy/sandbox.h"
#endif

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
#include "media/cdm/cdm_adapter_factory.h"          // nogncheck
#include "media/mojo/services/cdm_service.h"        // nogncheck
#include "media/mojo/services/mojo_cdm_helper.h"    // nogncheck
#include "media/mojo/services/mojo_media_client.h"  // nogncheck
#if BUILDFLAG(ENABLE_CDM_HOST_VERIFICATION)
#include "media/cdm/cdm_host_file.h"
#endif  // BUILDFLAG(ENABLE_CDM_HOST_VERIFICATION)

#if BUILDFLAG(ENABLE_VR) && !defined(OS_ANDROID)
#include "content/services/isolated_xr_device/xr_device_service.h"  // nogncheck
#include "device/vr/public/mojom/isolated_xr_service.mojom.h"       // nogncheck
#endif

#if defined(OS_WIN)
#include "base/win/scoped_com_initializer.h"
#include "sandbox/win/src/sandbox.h"

extern sandbox::TargetServices* g_utility_target_services;
#endif  // defined(OS_WIN)
#endif  // BUILDFLAG(ENABLE_LIBRARY_CDMS)

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
#include "sandbox/linux/services/libc_interceptor.h"
#include "sandbox/policy/sandbox_type.h"
#endif  // defined(OS_LINUX) || defined(OS_CHROMEOS)

#if BUILDFLAG(GOOGLE_CHROME_BRANDING) && BUILDFLAG(IS_CHROMEOS_ASH)
#include "services/shape_detection/public/mojom/shape_detection_service.mojom.h"  // nogncheck
#include "services/shape_detection/shape_detection_service.h"  // nogncheck
#endif

namespace content {

namespace {

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)

std::unique_ptr<media::CdmAuxiliaryHelper> CreateCdmHelper(
    media::mojom::FrameInterfaceFactory* interface_provider) {
  return std::make_unique<media::MojoCdmHelper>(interface_provider);
}

class ContentCdmServiceClient final : public media::CdmService::Client {
 public:
  ContentCdmServiceClient() = default;
  ~ContentCdmServiceClient() override = default;

  void EnsureSandboxed() override {
#if defined(OS_WIN)
    // |g_utility_target_services| can be null if --no-sandbox is specified.
    if (g_utility_target_services)
      g_utility_target_services->LowerToken();
#endif
  }

  std::unique_ptr<media::CdmFactory> CreateCdmFactory(
      media::mojom::FrameInterfaceFactory* frame_interfaces) override {
    return std::make_unique<media::CdmAdapterFactory>(
        base::BindRepeating(&CreateCdmHelper, frame_interfaces));
  }

#if BUILDFLAG(ENABLE_CDM_HOST_VERIFICATION)
  void AddCdmHostFilePaths(
      std::vector<media::CdmHostFilePath>* cdm_host_file_paths) override {
    GetContentClient()->AddContentDecryptionModules(nullptr,
                                                    cdm_host_file_paths);
  }
#endif  // BUILDFLAG(ENABLE_CDM_HOST_VERIFICATION)
};
#endif  // BUILDFLAG(ENABLE_LIBRARY_CDMS)

class UtilityThreadVideoCaptureServiceImpl final
    : public video_capture::VideoCaptureServiceImpl {
 public:
  explicit UtilityThreadVideoCaptureServiceImpl(
      mojo::PendingReceiver<video_capture::mojom::VideoCaptureService> receiver,
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner)
      : VideoCaptureServiceImpl(std::move(receiver),
                                std::move(ui_task_runner)) {}

 private:
#if defined(OS_WIN)
  base::win::ScopedCOMInitializer com_initializer_{
      base::win::ScopedCOMInitializer::kMTA};
#endif
};

auto RunNetworkService(
    mojo::PendingReceiver<network::mojom::NetworkService> receiver) {
  auto binders = std::make_unique<service_manager::BinderRegistry>();
  GetContentClient()->utility()->RegisterNetworkBinders(binders.get());
  return std::make_unique<network::NetworkService>(
      std::move(binders), std::move(receiver),
      /*delay_initialization_until_set_client=*/true);
}

auto RunAudio(mojo::PendingReceiver<audio::mojom::AudioService> receiver) {
#if defined(OS_MAC)
  // Don't connect to launch services when running sandboxed
  // (https://crbug.com/874785).
  if (sandbox::policy::Sandbox::IsProcessSandboxed()) {
    sandbox::DisableLaunchServices();
  }

  // Set the audio process to run with similar scheduling parameters as the
  // browser process.
  task_category_policy category;
  category.role = TASK_FOREGROUND_APPLICATION;
  kern_return_t result = task_policy_set(
      mach_task_self(), TASK_CATEGORY_POLICY,
      reinterpret_cast<task_policy_t>(&category), TASK_CATEGORY_POLICY_COUNT);

  MACH_LOG_IF(ERROR, result != KERN_SUCCESS, result)
      << "task_policy_set TASK_CATEGORY_POLICY";

  task_qos_policy qos;
  qos.task_latency_qos_tier = LATENCY_QOS_TIER_0;
  qos.task_throughput_qos_tier = THROUGHPUT_QOS_TIER_0;
  result = task_policy_set(mach_task_self(), TASK_BASE_QOS_POLICY,
                           reinterpret_cast<task_policy_t>(&qos),
                           TASK_QOS_POLICY_COUNT);

  MACH_LOG_IF(ERROR, result != KERN_SUCCESS, result)
      << "task_policy_set TASK_QOS_POLICY";
#endif

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
  auto* command_line = base::CommandLine::ForCurrentProcess();
  if (sandbox::policy::SandboxTypeFromCommandLine(*command_line) ==
      sandbox::policy::SandboxType::kNoSandbox) {
    // This is necessary to avoid crashes in certain environments.
    // See https://crbug.com/1109346
    sandbox::InitLibcLocaltimeFunctions();
  }
#endif

#if defined(OS_WIN)
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAudioProcessHighPriority)) {
    auto success =
        ::SetPriorityClass(::GetCurrentProcess(), ABOVE_NORMAL_PRIORITY_CLASS);
    DCHECK(success);
  }
#endif
  return audio::CreateStandaloneService(std::move(receiver));
}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING) && BUILDFLAG(IS_CHROMEOS_ASH)
auto RunShapeDetectionService(
    mojo::PendingReceiver<shape_detection::mojom::ShapeDetectionService>
        receiver) {
  return std::make_unique<shape_detection::ShapeDetectionService>(
      std::move(receiver));
}
#endif

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
auto RunCdmService(mojo::PendingReceiver<media::mojom::CdmService> receiver) {
  return std::make_unique<media::CdmService>(
      std::make_unique<ContentCdmServiceClient>(), std::move(receiver));
}
#endif

auto RunDataDecoder(
    mojo::PendingReceiver<data_decoder::mojom::DataDecoderService> receiver) {
  UtilityThread::Get()->EnsureBlinkInitialized();
  return std::make_unique<data_decoder::DataDecoderService>(
      std::move(receiver));
}

auto RunStorageService(
    mojo::PendingReceiver<storage::mojom::StorageService> receiver) {
  return std::make_unique<storage::StorageServiceImpl>(
      std::move(receiver), ChildProcess::current()->io_task_runner());
}

auto RunTracing(
    mojo::PendingReceiver<tracing::mojom::TracingService> receiver) {
  return std::make_unique<tracing::TracingService>(std::move(receiver));
}

auto RunVideoCapture(
    mojo::PendingReceiver<video_capture::mojom::VideoCaptureService> receiver) {
  return std::make_unique<UtilityThreadVideoCaptureServiceImpl>(
      std::move(receiver), base::ThreadTaskRunnerHandle::Get());
}

#if BUILDFLAG(ENABLE_VR) && !defined(OS_ANDROID)
auto RunXrDeviceService(
    mojo::PendingReceiver<device::mojom::XRDeviceService> receiver) {
  return std::make_unique<device::XrDeviceService>(
      std::move(receiver), content::ChildProcess::current()->io_task_runner());
}
#endif

}  // namespace

void RegisterIOThreadServices(mojo::ServiceFactory& services) {
  // The network service runs on the IO thread because it needs a message
  // loop of type IO that can get notified when pipes have data.
  services.Add(RunNetworkService);

  // Add new IO-thread services above this line.
  GetContentClient()->utility()->RegisterIOThreadServices(services);
}

void RegisterMainThreadServices(mojo::ServiceFactory& services) {
  services.Add(RunAudio);

  services.Add(RunDataDecoder);
  services.Add(RunStorageService);
  services.Add(RunTracing);
  services.Add(RunVideoCapture);

#if BUILDFLAG(GOOGLE_CHROME_BRANDING) && BUILDFLAG(IS_CHROMEOS_ASH)
  services.Add(RunShapeDetectionService);
#endif

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
  services.Add(RunCdmService);
#endif

#if BUILDFLAG(ENABLE_VR) && !defined(OS_ANDROID)
  services.Add(RunXrDeviceService);
#endif

  // Add new main-thread services above this line.
  GetContentClient()->utility()->RegisterMainThreadServices(services);
}

}  // namespace content
