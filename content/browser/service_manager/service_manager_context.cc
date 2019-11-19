// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_manager/service_manager_context.h"

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/deferred_sequenced_task_runner.h"
#include "base/feature_list.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_pump_type.h"
#include "base/no_destructor.h"
#include "base/optional.h"
#include "base/process/process_handle.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "build/build_config.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/builtin_service_manifests.h"
#include "content/browser/child_process_launcher.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/browser/system_connector_impl.h"
#include "content/browser/utility_process_host.h"
#include "content/browser/wake_lock/wake_lock_context_host.h"
#include "content/common/service_manager/service_manager_connection_impl.h"
#include "content/public/app/content_browser_manifest.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/gpu_service_registry.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/service_process_host.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/service_manager_connection.h"
#include "content/public/common/service_names.mojom.h"
#include "media/audio/audio_manager.h"
#include "media/media_buildflags.h"
#include "media/mojo/buildflags.h"
#include "media/mojo/mojom/constants.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/system/invitation.h"
#include "services/audio/public/mojom/constants.mojom.h"
#include "services/audio/service.h"
#include "services/audio/service_factory.h"
#include "services/device/device_service.h"
#include "services/device/public/mojom/constants.mojom.h"
#include "services/media_session/media_session_service.h"
#include "services/media_session/public/cpp/features.h"
#include "services/media_session/public/mojom/constants.mojom.h"
#include "services/metrics/metrics_mojo_service.h"
#include "services/metrics/public/mojom/constants.mojom.h"
#include "services/network/public/cpp/cross_thread_shared_url_loader_factory_info.h"
#include "services/network/public/mojom/network_service_test.mojom.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/constants.h"
#include "services/service_manager/public/cpp/manifest.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/mojom/service.mojom.h"
#include "services/service_manager/sandbox/sandbox_type.h"
#include "services/service_manager/service_manager.h"
#include "services/service_manager/service_process_host.h"
#include "services/service_manager/service_process_launcher.h"
#include "services/tracing/public/cpp/tracing_features.h"
#include "services/tracing/public/mojom/constants.mojom.h"
#include "services/tracing/tracing_service.h"
#include "third_party/blink/public/strings/grit/blink_strings.h"
#include "ui/base/buildflags.h"
#include "ui/base/ui_base_features.h"

#if defined(OS_ANDROID)
#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "content/public/android/content_jni_headers/ContentNfcDelegate_jni.h"
#endif

namespace content {

namespace {

base::LazyInstance<std::unique_ptr<service_manager::Connector>>::Leaky
    g_io_thread_connector = LAZY_INSTANCE_INITIALIZER;

base::LazyInstance<std::map<std::string, base::WeakPtr<UtilityProcessHost>>>::
    Leaky g_active_process_groups;

service_manager::Manifest GetContentSystemManifest() {
  // TODO(https://crbug.com/961869): This is a bit of a temporary hack so that
  // we can make the global service instance a singleton. For now we just mirror
  // the per-BrowserContext manifest (formerly also used for the global
  // singleton instance), sans packaged services, since those are only meant to
  // be tied to a BrowserContext. The per-BrowserContext service should go away
  // soon, and then this can be removed.
  service_manager::Manifest manifest = GetContentBrowserManifest();
  manifest.Amend(GetContentClient()
                     ->browser()
                     ->GetServiceManifestOverlay(mojom::kBrowserServiceName)
                     .value_or(service_manager::Manifest()));
  manifest.service_name = mojom::kSystemServiceName;
  manifest.packaged_services.clear();
  manifest.options.instance_sharing_policy =
      service_manager::Manifest::InstanceSharingPolicy::kSingleton;
  return manifest;
}

void DestroyConnectorOnIOThread() { g_io_thread_connector.Get().reset(); }

// A ServiceProcessHost implementation which delegates to Content-managed
// processes, either via a new UtilityProcessHost to launch new service
// processes, or the existing GpuProcessHost to run service instances in the GPU
// process.
class ContentChildServiceProcessHost
    : public service_manager::ServiceProcessHost {
 public:
  ContentChildServiceProcessHost(bool run_in_gpu_process,
                                 base::Optional<int> child_flags)
      : run_in_gpu_process_(run_in_gpu_process), child_flags_(child_flags) {}
  ~ContentChildServiceProcessHost() override = default;

  // service_manager::ServiceProcessHost:
  mojo::PendingRemote<service_manager::mojom::Service> Launch(
      const service_manager::Identity& identity,
      service_manager::SandboxType sandbox_type,
      const base::string16& display_name,
      LaunchCallback callback) override {
    mojo::PendingRemote<service_manager::mojom::Service> remote;
    auto receiver = remote.InitWithNewPipeAndPassReceiver();
    if (run_in_gpu_process_) {
      // TODO(https://crbug.com/781334): Services running in the GPU process
      // should be packaged into the content_gpu manifest. Then this would be
      // unnecessary.
      GpuProcessHost* process_host = GpuProcessHost::Get();
      if (!process_host) {
        DLOG(ERROR) << "GPU process host not available.";
        return mojo::NullRemote();
      }

      // TODO(xhwang): It's possible that |process_host| is non-null, but the
      // actual process is dead. In that case the receiver will be dropped. Make
      // sure we handle these cases correctly.
      process_host->gpu_host()->RunService(identity.name(),
                                           std::move(receiver));
      base::ProcessId process_id = process_host->process_id();
      std::move(callback).Run(process_id != base::kNullProcessId
                                  ? process_id
                                  : base::GetCurrentProcId());
      return remote;
    }

    // Start a new process for this service.
    UtilityProcessHost* process_host = new UtilityProcessHost();
    process_host->SetName(display_name);
    process_host->SetMetricsName(identity.name());
    process_host->SetServiceIdentity(identity);
    process_host->SetSandboxType(sandbox_type);
    if (child_flags_.has_value())
      process_host->set_child_flags(child_flags_.value());
    process_host->Start();
    process_host->RunService(
        identity.name(), std::move(receiver),
        base::BindOnce(
            [](LaunchCallback callback,
               const base::Optional<base::ProcessId> pid) {
              std::move(callback).Run(pid.value_or(base::kNullProcessId));
            },
            std::move(callback)));
    return remote;
  }

 private:
  const bool run_in_gpu_process_;
  const base::Optional<int> child_flags_;
  DISALLOW_COPY_AND_ASSIGN(ContentChildServiceProcessHost);
};

// SharedURLLoaderFactory for device service, backed by
// GetContentClient()->browser()->GetSystemSharedURLLoaderFactory().
class DeviceServiceURLLoaderFactory : public network::SharedURLLoaderFactory {
 public:
  DeviceServiceURLLoaderFactory() = default;

  // mojom::URLLoaderFactory implementation:
  void CreateLoaderAndStart(
      mojo::PendingReceiver<network::mojom::URLLoader> receiver,
      int32_t routing_id,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& url_request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
      override {
    GetContentClient()
        ->browser()
        ->GetSystemSharedURLLoaderFactory()
        ->CreateLoaderAndStart(std::move(receiver), routing_id, request_id,
                               options, url_request, std::move(client),
                               traffic_annotation);
  }

  // SharedURLLoaderFactory implementation:
  void Clone(mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver)
      override {
    GetContentClient()->browser()->GetSystemSharedURLLoaderFactory()->Clone(
        std::move(receiver));
  }

  std::unique_ptr<network::SharedURLLoaderFactoryInfo> Clone() override {
    return std::make_unique<network::CrossThreadSharedURLLoaderFactoryInfo>(
        this);
  }

 private:
  friend class base::RefCounted<DeviceServiceURLLoaderFactory>;
  ~DeviceServiceURLLoaderFactory() override = default;

  DISALLOW_COPY_AND_ASSIGN(DeviceServiceURLLoaderFactory);
};

bool AudioServiceOutOfProcess() {
  // Returns true iff kAudioServiceOutOfProcess feature is enabled and if the
  // embedder does not provide its own in-process AudioManager.
  return base::FeatureList::IsEnabled(features::kAudioServiceOutOfProcess) &&
         !GetContentClient()->browser()->OverridesAudioManager();
}

using InProcessServiceFactory =
    base::RepeatingCallback<std::unique_ptr<service_manager::Service>(
        service_manager::mojom::ServiceRequest request)>;

void LaunchInProcessServiceOnSequence(
    const InProcessServiceFactory& factory,
    service_manager::mojom::ServiceRequest request) {
  service_manager::Service::RunAsyncUntilTermination(
      factory.Run(std::move(request)));
}

void LaunchInProcessService(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    const InProcessServiceFactory& factory,
    service_manager::mojom::ServiceRequest request) {
  task_runner->PostTask(
      FROM_HERE, base::BindOnce(&LaunchInProcessServiceOnSequence, factory,
                                std::move(request)));
}

// Temporary helper to reduce churn when moving away from Content packaged
// services.
using InProcessServiceMap = std::map<
    std::string,
    base::RepeatingCallback<void(service_manager::mojom::ServiceRequest)>>;
InProcessServiceMap& GetInProcessServiceMap() {
  static base::NoDestructor<InProcessServiceMap> services;
  return *services;
}

void RegisterInProcessService(
    const std::string& service_name,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    const InProcessServiceFactory& factory) {
  GetInProcessServiceMap()[service_name] = base::BindRepeating(
      &LaunchInProcessService, std::move(task_runner), factory);
}

void CreateInProcessAudioService(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    service_manager::mojom::ServiceRequest request) {
  // TODO(https://crbug.com/853254): Remove BrowserMainLoop::GetAudioManager().
  task_runner->PostTask(
      FROM_HERE, base::BindOnce(
                     [](media::AudioManager* audio_manager,
                        service_manager::mojom::ServiceRequest request) {
                       service_manager::Service::RunAsyncUntilTermination(
                           audio::CreateEmbeddedService(audio_manager,
                                                        std::move(request)));
                     },
                     BrowserMainLoop::GetAudioManager(), std::move(request)));
}

std::unique_ptr<service_manager::Service> CreateTracingService(
    service_manager::mojom::ServiceRequest request) {
  return std::make_unique<tracing::TracingService>(std::move(request));
}

std::unique_ptr<service_manager::Service> CreateMediaSessionService(
    service_manager::mojom::ServiceRequest request) {
  return std::make_unique<media_session::MediaSessionService>(
      std::move(request));
}

void RunServiceInstanceOnIOThread(
    const service_manager::Identity& identity,
    mojo::PendingReceiver<service_manager::mojom::Service>* receiver) {
  if (!AudioServiceOutOfProcess() &&
      identity.name() == audio::mojom::kServiceName) {
    CreateInProcessAudioService(audio::Service::GetInProcessTaskRunner(),
                                std::move(*receiver));
    return;
  }
}

// A ServiceProcessHost implementation which uses the Service Manager's builtin
// service executable launcher. Not yet intended for use in production Chrome,
// hence availability is gated behind a flag.
class ServiceExecutableProcessHost
    : public service_manager::ServiceProcessHost {
 public:
  explicit ServiceExecutableProcessHost(const base::FilePath& executable_path)
      : launcher_(nullptr, executable_path) {}
  ~ServiceExecutableProcessHost() override = default;

  // service_manager::ServiceProcessHost:
  mojo::PendingRemote<service_manager::mojom::Service> Launch(
      const service_manager::Identity& identity,
      service_manager::SandboxType sandbox_type,
      const base::string16& display_name,
      LaunchCallback callback) override {
    // TODO(https://crbug.com/781334): Support sandboxing.
    return launcher_
        .Start(identity, service_manager::SANDBOX_TYPE_NO_SANDBOX,
               std::move(callback))
        .PassInterface();
  }

 private:
  service_manager::ServiceProcessLauncher launcher_;

  DISALLOW_COPY_AND_ASSIGN(ServiceExecutableProcessHost);
};

using ServiceRequestHandler = base::RepeatingCallback<void(
    const service_manager::Identity& identity,
    mojo::PendingReceiver<service_manager::mojom::Service> receiver)>;

// Implements in- and out-of-process service instance launching for services
// built into the Content embedder's binary.
//
// All methods on this object (except the constructor) are called on the Service
// Manager's thread, which is effectively the browser's IO thread.
class BrowserServiceManagerDelegate
    : public service_manager::ServiceManager::Delegate {
 public:
  BrowserServiceManagerDelegate(
      const scoped_refptr<base::SequencedTaskRunner>& main_thread_task_runner,
      ServiceRequestHandler main_thread_request_handler)
      : main_thread_task_runner_(main_thread_task_runner),
        main_thread_request_handler_(std::move(main_thread_request_handler)) {}
  ~BrowserServiceManagerDelegate() override = default;

  // service_manager::ServiceManager::Delegate:
  bool RunBuiltinServiceInstanceInCurrentProcess(
      const service_manager::Identity& identity,
      mojo::PendingReceiver<service_manager::mojom::Service> receiver)
      override {
    const auto& service_map = GetInProcessServiceMap();
    auto it = service_map.find(identity.name());
    if (it != service_map.end()) {
      it->second.Run(std::move(receiver));
      return true;
    }

    RunServiceInstanceOnIOThread(identity, &receiver);
    if (!receiver)
      return true;

    main_thread_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(main_thread_request_handler_, identity,
                                  std::move(receiver)));
    return true;
  }

  std::unique_ptr<service_manager::ServiceProcessHost>
  CreateProcessHostForBuiltinServiceInstance(
      const service_manager::Identity& identity) override {
    // TODO(crbug.com/895615): Package these services in content_gpu instead
    // of using this hack.
    bool run_in_gpu_process = false;
    base::Optional<int> child_flags;
#if BUILDFLAG(ENABLE_MOJO_MEDIA_IN_GPU_PROCESS)
    if (identity.name() == media::mojom::kMediaServiceName)
      run_in_gpu_process = true;
#endif
    return std::make_unique<ContentChildServiceProcessHost>(run_in_gpu_process,
                                                            child_flags);
  }

  std::unique_ptr<service_manager::ServiceProcessHost>
  CreateProcessHostForServiceExecutable(
      const base::FilePath& executable_path) override {
    if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kEnableServiceBinaryLauncher)) {
      return nullptr;
    }

    return std::make_unique<ServiceExecutableProcessHost>(executable_path);
  }

 private:
  const scoped_refptr<base::SequencedTaskRunner> main_thread_task_runner_;
  const ServiceRequestHandler main_thread_request_handler_;

  DISALLOW_COPY_AND_ASSIGN(BrowserServiceManagerDelegate);
};

}  // namespace

// State which lives on the IO thread and drives the ServiceManager.
class ServiceManagerContext::InProcessServiceManagerContext
    : public base::RefCountedThreadSafe<InProcessServiceManagerContext> {
 public:
  InProcessServiceManagerContext(scoped_refptr<base::SingleThreadTaskRunner>
                                     service_manager_thread_task_runner)
      : service_manager_thread_task_runner_(
            service_manager_thread_task_runner) {}

  void Start(std::vector<service_manager::Manifest> manifests,
             mojo::PendingRemote<service_manager::mojom::Service> system_remote,
             ServiceRequestHandler request_handler) {
    service_manager_thread_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &InProcessServiceManagerContext::StartOnServiceManagerThread, this,
            std::move(manifests), base::ThreadTaskRunnerHandle::Get(),
            std::move(system_remote), std::move(request_handler)));
  }

  void ShutDown() {
    service_manager_thread_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &InProcessServiceManagerContext::ShutDownOnServiceManagerThread,
            this));
  }

  void StartServices(std::vector<std::string> service_names) {
    service_manager_thread_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&InProcessServiceManagerContext ::
                                      StartServicesOnServiceManagerThread,
                                  this, std::move(service_names)));
  }

 private:
  friend class base::RefCountedThreadSafe<InProcessServiceManagerContext>;

  ~InProcessServiceManagerContext() = default;

  void StartOnServiceManagerThread(
      std::vector<service_manager::Manifest> manifests,
      scoped_refptr<base::SequencedTaskRunner> ui_thread_task_runner,
      mojo::PendingRemote<service_manager::mojom::Service> system_remote,
      ServiceRequestHandler request_handler) {
    service_manager_ = std::make_unique<service_manager::ServiceManager>(
        std::move(manifests),
        std::make_unique<BrowserServiceManagerDelegate>(
            ui_thread_task_runner, std::move(request_handler)));

    mojo::Remote<service_manager::mojom::ProcessMetadata> metadata;
    service_manager_->RegisterService(
        service_manager::Identity(mojom::kSystemServiceName,
                                  service_manager::kSystemInstanceGroup,
                                  base::Token{}, base::Token::CreateRandom()),
        std::move(system_remote), metadata.BindNewPipeAndPassReceiver());
    metadata->SetPID(base::GetCurrentProcId());

    service_manager_->SetInstanceQuitCallback(
        base::Bind(&OnInstanceQuitOnServiceManagerThread,
                   std::move(ui_thread_task_runner)));
  }

  static void OnInstanceQuitOnServiceManagerThread(
      scoped_refptr<base::SequencedTaskRunner> ui_thread_task_runner,
      const service_manager::Identity& id) {
    ui_thread_task_runner->PostTask(FROM_HERE,
                                    base::BindOnce(&OnInstanceQuit, id));
  }

  static void OnInstanceQuit(const service_manager::Identity& id) {
    if (GetContentClient()->browser()->ShouldTerminateOnServiceQuit(id)) {
      // Don't LOG(FATAL) because we don't want a browser crash report.
      LOG(ERROR) << "Terminating because service '" << id.name()
                 << "' quit unexpectedly.";
      // Skip shutdown to reduce the risk that other code in the browser will
      // respond to the service pipe closing.
      exit(1);
    }
  }

  void ShutDownOnServiceManagerThread() {
    service_manager_.reset();
    GetInProcessServiceMap().clear();
  }

  void StartServicesOnServiceManagerThread(
      std::vector<std::string> service_names) {
    if (!service_manager_)
      return;

    for (const auto& service_name : service_names)
      service_manager_->StartService(service_name);
  }

  const scoped_refptr<base::SingleThreadTaskRunner>
      service_manager_thread_task_runner_;
  std::unique_ptr<service_manager::ServiceManager> service_manager_;

  DISALLOW_COPY_AND_ASSIGN(InProcessServiceManagerContext);
};

ServiceManagerContext::ServiceManagerContext(
    scoped_refptr<base::SingleThreadTaskRunner>
        service_manager_thread_task_runner)
    : service_manager_thread_task_runner_(
          std::move(service_manager_thread_task_runner)) {
  // The |service_manager_thread_task_runner_| must have been created before
  // starting the ServiceManager.
  DCHECK(service_manager_thread_task_runner_);
  std::vector<service_manager::Manifest> manifests =
      GetBuiltinServiceManifests();
  manifests.push_back(GetContentSystemManifest());
  for (auto& manifest : manifests) {
    base::Optional<service_manager::Manifest> overlay =
        GetContentClient()->browser()->GetServiceManifestOverlay(
            manifest.service_name);
    if (overlay)
      manifest.Amend(*overlay);
    if (!manifest.preloaded_files.empty()) {
      std::map<std::string, base::FilePath> preloaded_files_map;
      for (const auto& info : manifest.preloaded_files)
        preloaded_files_map.emplace(info.key, info.path);
      ChildProcessLauncher::SetRegisteredFilesForService(
          manifest.service_name, std::move(preloaded_files_map));
    }
  }
  for (auto& extra_manifest :
       GetContentClient()->browser()->GetExtraServiceManifests()) {
    manifests.emplace_back(std::move(extra_manifest));
  }
  in_process_context_ =
      new InProcessServiceManagerContext(service_manager_thread_task_runner_);

  mojo::PendingRemote<service_manager::mojom::Service> system_remote;
  ServiceManagerConnection::SetForProcess(ServiceManagerConnection::Create(
      system_remote.InitWithNewPipeAndPassReceiver(),
      service_manager_thread_task_runner_));
  auto* system_connection = ServiceManagerConnection::GetForProcess();
  SetSystemConnector(system_connection->GetConnector()->Clone());

  RegisterInProcessService(metrics::mojom::kMetricsServiceName,
                           service_manager_thread_task_runner_,
                           base::BindRepeating(&metrics::CreateMetricsService));

  if (base::FeatureList::IsEnabled(
          media_session::features::kMediaSessionService)) {
    RegisterInProcessService(media_session::mojom::kServiceName,
                             base::SequencedTaskRunnerHandle::Get(),
                             base::BindRepeating(&CreateMediaSessionService));
  }

  // This is safe to assign directly from any thread, because
  // ServiceManagerContext must be constructed before anyone can call
  // GetConnectorForIOThread().
  g_io_thread_connector.Get() = system_connection->GetConnector()->Clone();

  GetContentClient()->browser()->WillStartServiceManager();

  if (base::FeatureList::IsEnabled(features::kTracingServiceInProcess)) {
    RegisterInProcessService(tracing::mojom::kServiceName,
                             base::CreateSequencedTaskRunner(
                                 {base::ThreadPool(), base::MayBlock(),
                                  base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN,
                                  base::WithBaseSyncPrimitives(),
                                  base::TaskPriority::USER_BLOCKING}),
                             base::BindRepeating(&CreateTracingService));
  }

  in_process_context_->Start(
      manifests, std::move(system_remote),
      base::BindRepeating(&ServiceManagerContext::RunServiceInstance,
                          weak_ptr_factory_.GetWeakPtr()));
  in_process_context_->StartServices(
      GetContentClient()->browser()->GetStartupServices());
}

ServiceManagerContext::~ServiceManagerContext() {
  ShutDown();
}

void ServiceManagerContext::ShutDown() {
  // NOTE: The in-process ServiceManager MUST be destroyed before the browser
  // process-wide ServiceManagerConnection. Otherwise it's possible for the
  // ServiceManager to receive connection requests for service:content_browser
  // which it may attempt to service by launching a new instance of the browser.
  if (in_process_context_)
    in_process_context_->ShutDown();
  if (ServiceManagerConnection::GetForProcess())
    ServiceManagerConnection::DestroyForProcess();
  service_manager_thread_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&DestroyConnectorOnIOThread));
}

// static
service_manager::Connector* ServiceManagerContext::GetConnectorForIOThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  return g_io_thread_connector.Get().get();
}

// static
bool ServiceManagerContext::HasValidProcessForProcessGroup(
    const std::string& process_group_name) {
  auto iter = g_active_process_groups.Get().find(process_group_name);
  if (iter == g_active_process_groups.Get().end() || !iter->second)
    return false;
  return iter->second->GetData().GetProcess().IsValid();
}

void ServiceManagerContext::RunServiceInstance(
    const service_manager::Identity& identity,
    mojo::PendingReceiver<service_manager::mojom::Service> receiver) {
  if (identity.name() == device::mojom::kServiceName) {
    // This task runner may be used by some device service implementation bits
    // to interface with dbus client code, which in turn imposes some subtle
    // thread affinity on the clients. We therefore require a single-thread
    // runner.
    scoped_refptr<base::SingleThreadTaskRunner> device_blocking_task_runner =
        base::CreateSingleThreadTaskRunner({base::ThreadPool(),
                                            base::MayBlock(),
                                            base::TaskPriority::BEST_EFFORT});
#if defined(OS_ANDROID)
    JNIEnv* env = base::android::AttachCurrentThread();
    base::android::ScopedJavaGlobalRef<jobject> java_nfc_delegate;
    java_nfc_delegate.Reset(Java_ContentNfcDelegate_create(env));
    DCHECK(!java_nfc_delegate.is_null());

    // See the comments on wake_lock_context_host.h, content_browser_client.h
    // and ContentNfcDelegate.java respectively for comments on those
    // parameters.
    auto service = device::CreateDeviceService(
        device_blocking_task_runner, service_manager_thread_task_runner_,
        base::MakeRefCounted<DeviceServiceURLLoaderFactory>(),
        content::GetNetworkConnectionTracker(),
        GetContentClient()->browser()->GetGeolocationApiKey(),
        GetContentClient()->browser()->ShouldUseGmsCoreGeolocationProvider(),
        base::BindRepeating(&WakeLockContextHost::GetNativeViewForContext),
        base::BindRepeating(
            &ContentBrowserClient::OverrideSystemLocationProvider,
            base::Unretained(GetContentClient()->browser())),
        std::move(java_nfc_delegate), std::move(receiver));
#else
    auto service = device::CreateDeviceService(
        device_blocking_task_runner, service_manager_thread_task_runner_,
        base::MakeRefCounted<DeviceServiceURLLoaderFactory>(),
        content::GetNetworkConnectionTracker(),
        GetContentClient()->browser()->GetGeolocationApiKey(),
        base::BindRepeating(
            &ContentBrowserClient::OverrideSystemLocationProvider,
            base::Unretained(GetContentClient()->browser())),
        std::move(receiver));
#endif
    service_manager::Service::RunAsyncUntilTermination(std::move(service));
    return;
  }

  GetContentClient()->browser()->RunServiceInstance(identity, &receiver);

  DLOG_IF(ERROR, receiver) << "Unhandled service request for \""
                           << identity.name() << "\"";
}

}  // namespace content
