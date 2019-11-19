// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/utility/utility_service_factory.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/debug/crash_logging.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/trace_event/trace_log.h"
#include "build/build_config.h"
#include "content/child/child_process.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/service_names.mojom.h"
#include "content/public/utility/content_utility_client.h"
#include "content/public/utility/utility_thread.h"
#include "content/utility/utility_thread_impl.h"
#include "media/media_buildflags.h"
#include "services/audio/public/mojom/constants.mojom.h"
#include "services/audio/service_factory.h"
#include "services/network/network_service.h"
#include "services/service_manager/public/mojom/service.mojom.h"
#include "services/tracing/public/cpp/tracing_features.h"
#include "services/tracing/public/mojom/constants.mojom.h"
#include "services/tracing/tracing_service.h"

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
#include "media/cdm/cdm_adapter_factory.h"           // nogncheck
#include "media/mojo/mojom/constants.mojom.h"   // nogncheck
#include "media/mojo/services/cdm_service.h"         // nogncheck
#include "media/mojo/services/mojo_cdm_helper.h"     // nogncheck
#include "media/mojo/services/mojo_media_client.h"   // nogncheck
#if BUILDFLAG(ENABLE_CDM_HOST_VERIFICATION)
#include "media/cdm/cdm_host_file.h"
#endif  // BUILDFLAG(ENABLE_CDM_HOST_VERIFICATION)
#endif

#if defined(OS_MACOSX)
#include "base/mac/mach_logging.h"
#include "sandbox/mac/system_services.h"
#include "services/service_manager/sandbox/features.h"
#include "services/service_manager/sandbox/sandbox_type.h"
#endif

#if defined(OS_WIN)
#include "sandbox/win/src/sandbox.h"

extern sandbox::TargetServices* g_utility_target_services;
#endif

namespace content {

namespace {

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)

std::unique_ptr<media::CdmAuxiliaryHelper> CreateCdmHelper(
    service_manager::mojom::InterfaceProvider* interface_provider) {
  return std::make_unique<media::MojoCdmHelper>(interface_provider);
}

class ContentCdmServiceClient final : public media::CdmService::Client {
 public:
  ContentCdmServiceClient() {}
  ~ContentCdmServiceClient() override {}

  void EnsureSandboxed() override {
#if defined(OS_WIN)
    // |g_utility_target_services| can be null if --no-sandbox is specified.
    if (g_utility_target_services)
      g_utility_target_services->LowerToken();
#endif
  }

  std::unique_ptr<media::CdmFactory> CreateCdmFactory(
      service_manager::mojom::InterfaceProvider* host_interfaces) override {
    return std::make_unique<media::CdmAdapterFactory>(
        base::Bind(&CreateCdmHelper, host_interfaces));
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

}  // namespace

UtilityServiceFactory::UtilityServiceFactory()
    : network_registry_(std::make_unique<service_manager::BinderRegistry>()),
      audio_binders_(std::make_unique<service_manager::BinderMap>()) {
  GetContentClient()->utility()->RegisterAudioBinders(audio_binders_.get());
}

UtilityServiceFactory::~UtilityServiceFactory() {}

void UtilityServiceFactory::RunService(
    const std::string& service_name,
    mojo::PendingReceiver<service_manager::mojom::Service> receiver) {
  auto request = service_manager::mojom::ServiceRequest(std::move(receiver));
  auto* trace_log = base::trace_event::TraceLog::GetInstance();
  if (trace_log->IsProcessNameEmpty())
    trace_log->set_process_name("Service: " + service_name);

  static auto* service_name_crash_key = base::debug::AllocateCrashKeyString(
      "service-name", base::debug::CrashKeySize::Size32);
  base::debug::SetCrashKeyString(service_name_crash_key, service_name);

  std::unique_ptr<service_manager::Service> service;
  if (service_name == audio::mojom::kServiceName) {
    service = CreateAudioService(std::move(request));
  } else if (service_name == tracing::mojom::kServiceName &&
             !base::FeatureList::IsEnabled(
                 features::kTracingServiceInProcess)) {
    service = std::make_unique<tracing::TracingService>(std::move(request));
  }
#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
  else if (service_name == media::mojom::kCdmServiceName) {
    service = std::make_unique<media::CdmService>(
        std::make_unique<ContentCdmServiceClient>(), std::move(request));
  }
#endif  // BUILDFLAG(ENABLE_LIBRARY_CDMS)

  if (service) {
    service_manager::Service::RunAsyncUntilTermination(
        std::move(service),
        base::BindOnce(&UtilityThread::ReleaseProcess,
                       base::Unretained(UtilityThread::Get())));
    return;
  }

  if (GetContentClient()->utility()->HandleServiceRequest(service_name,
                                                          std::move(request))) {
    return;
  }

  // Nothing knew how to handle this request. Complain loudly and die.
  LOG(ERROR) << "Ignoring request to start unknown service: " << service_name;
  UtilityThreadImpl* utility_thread =
      static_cast<UtilityThreadImpl*>(UtilityThread::Get());
  utility_thread->Shutdown();
  utility_thread->ReleaseProcess();
}

std::unique_ptr<service_manager::Service>
UtilityServiceFactory::CreateAudioService(
    mojo::PendingReceiver<service_manager::mojom::Service> receiver) {
#if defined(OS_MACOSX)
  // Don't connect to launch services when running sandboxed
  // (https://crbug.com/874785).
  if (service_manager::IsAudioSandboxEnabled()) {
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

  return audio::CreateStandaloneService(std::move(audio_binders_),
                                        std::move(receiver));
}

}  // namespace content
