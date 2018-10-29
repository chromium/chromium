// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/gpu/gpu_child_thread.h"

#include <stddef.h>
#include <utility>

#include "base/allocator/allocator_extension.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/thread_checker.h"
#include "build/build_config.h"
#include "components/viz/common/features.h"
#include "content/child/child_process.h"
#include "content/gpu/gpu_service_factory.h"
#include "content/public/common/connection_filter.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/service_manager_connection.h"
#include "content/public/common/service_names.mojom.h"
#include "content/public/gpu/content_gpu_client.h"
#include "gpu/command_buffer/common/activity_flags.h"
#include "gpu/ipc/service/gpu_channel_manager.h"
#include "gpu/ipc/service/gpu_init.h"
#include "gpu/ipc/service/gpu_watchdog_thread.h"
#include "ipc/ipc_sync_message_filter.h"
#include "media/gpu/ipc/service/media_gpu_channel_manager.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/viz/privileged/interfaces/gl/gpu_service.mojom.h"
#include "third_party/skia/include/core/SkGraphics.h"

#if defined(USE_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#endif

#if defined(OS_ANDROID)
#include "media/base/android/media_drm_bridge_client.h"
#include "media/mojo/clients/mojo_android_overlay.h"
#endif

namespace content {
namespace {

ChildThreadImpl::Options GetOptions() {
  ChildThreadImpl::Options::Builder builder;

#if defined(USE_OZONE)
  IPC::MessageFilter* message_filter =
      ui::OzonePlatform::GetInstance()->GetGpuMessageFilter();
  if (message_filter)
    builder.AddStartupFilter(message_filter);
#endif

  builder.AutoStartServiceManagerConnection(false);
  builder.ConnectToBrowser(true);

  return builder.Build();
}

// This ConnectionFilter queues all incoming bind interface requests until
// Release() is called.
class QueueingConnectionFilter : public ConnectionFilter {
 public:
  QueueingConnectionFilter(
      scoped_refptr<base::SequencedTaskRunner> io_task_runner,
      std::unique_ptr<service_manager::BinderRegistry> registry)
      : io_task_runner_(io_task_runner),
        registry_(std::move(registry)),
        weak_factory_(this) {
    // This will be reattached by any of the IO thread functions on first call.
    io_thread_checker_.DetachFromThread();
  }
  ~QueueingConnectionFilter() override {
    DCHECK(io_thread_checker_.CalledOnValidThread());
  }

  base::Closure GetReleaseCallback() {
    return base::Bind(base::IgnoreResult(&base::TaskRunner::PostTask),
                      io_task_runner_, FROM_HERE,
                      base::Bind(&QueueingConnectionFilter::Release,
                                 weak_factory_.GetWeakPtr()));
  }

  void AddInterfaces() {
#if defined(USE_OZONE)
    ui::OzonePlatform::GetInstance()->AddInterfaces(registry_.get());
#endif
  }

 private:
  struct PendingRequest {
    std::string interface_name;
    mojo::ScopedMessagePipeHandle interface_pipe;
  };

  // ConnectionFilter:
  void OnBindInterface(const service_manager::BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle* interface_pipe,
                       service_manager::Connector* connector) override {
    DCHECK(io_thread_checker_.CalledOnValidThread());

    if (registry_->CanBindInterface(interface_name)) {
      if (released_) {
        registry_->BindInterface(interface_name, std::move(*interface_pipe));
      } else {
        std::unique_ptr<PendingRequest> request =
            std::make_unique<PendingRequest>();
        request->interface_name = interface_name;
        request->interface_pipe = std::move(*interface_pipe);
        pending_requests_.push_back(std::move(request));
      }
    }
  }

  void Release() {
    DCHECK(io_thread_checker_.CalledOnValidThread());
    released_ = true;
    for (auto& request : pending_requests_) {
      registry_->BindInterface(request->interface_name,
                               std::move(request->interface_pipe));
    }
  }

  base::ThreadChecker io_thread_checker_;
  scoped_refptr<base::SequencedTaskRunner> io_task_runner_;
  bool released_ = false;
  std::vector<std::unique_ptr<PendingRequest>> pending_requests_;
  std::unique_ptr<service_manager::BinderRegistry> registry_;

  base::WeakPtrFactory<QueueingConnectionFilter> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(QueueingConnectionFilter);
};

viz::VizMainImpl::ExternalDependencies CreateVizMainDependencies(
    service_manager::Connector* connector) {
  viz::VizMainImpl::ExternalDependencies deps;
  deps.create_display_compositor =
      base::FeatureList::IsEnabled(features::kVizDisplayCompositor);
  if (GetContentClient()->gpu())
    deps.sync_point_manager = GetContentClient()->gpu()->GetSyncPointManager();
  auto* process = ChildProcess::current();
  deps.shutdown_event = process->GetShutDownEvent();
  deps.io_thread_task_runner = process->io_task_runner();
  deps.connector = connector;
  return deps;
}

}  // namespace

GpuChildThread::GpuChildThread(base::RepeatingClosure quit_closure,
                               std::unique_ptr<gpu::GpuInit> gpu_init,
                               viz::VizMainImpl::LogMessages log_messages)
    : GpuChildThread(std::move(quit_closure),
                     GetOptions(),
                     std::move(gpu_init)) {
  viz_main_.SetLogMessagesForHost(std::move(log_messages));
}

GpuChildThread::GpuChildThread(const InProcessChildThreadParams& params,
                               std::unique_ptr<gpu::GpuInit> gpu_init)
    : GpuChildThread(base::DoNothing(),
                     ChildThreadImpl::Options::Builder()
                         .InBrowserProcess(params)
                         .AutoStartServiceManagerConnection(false)
                         .ConnectToBrowser(true)
                         .Build(),
                     std::move(gpu_init)) {}

GpuChildThread::GpuChildThread(base::RepeatingClosure quit_closure,
                               const ChildThreadImpl::Options& options,
                               std::unique_ptr<gpu::GpuInit> gpu_init)
    : ChildThreadImpl(std::move(quit_closure), options),
      viz_main_(this,
                CreateVizMainDependencies(GetConnector()),
                std::move(gpu_init)),
      weak_factory_(this) {
  if (in_process_gpu()) {
    DCHECK(base::CommandLine::ForCurrentProcess()->HasSwitch(
               switches::kSingleProcess) ||
           base::CommandLine::ForCurrentProcess()->HasSwitch(
               switches::kInProcessGPU));
  }
}

GpuChildThread::~GpuChildThread() {
}

void GpuChildThread::Init(const base::Time& process_start_time) {
  viz_main_.gpu_service()->set_start_time(process_start_time);

  // When running in in-process mode, this has been set in the browser at
  // ChromeBrowserMainPartsAndroid::PreMainMessageLoopRun().
#if defined(OS_ANDROID)
  if (!in_process_gpu()) {
    media::SetMediaDrmBridgeClient(
        GetContentClient()->GetMediaDrmBridgeClient());
  }
#endif

  blink::AssociatedInterfaceRegistry* associated_registry =
      &associated_interfaces_;
  associated_registry->AddInterface(base::Bind(
      &GpuChildThread::CreateVizMainService, base::Unretained(this)));

  auto registry = std::make_unique<service_manager::BinderRegistry>();
  registry->AddInterface(base::Bind(&GpuChildThread::BindServiceFactoryRequest,
                                    weak_factory_.GetWeakPtr()),
                         base::ThreadTaskRunnerHandle::Get());
  if (GetContentClient()->gpu())  // nullptr in tests.
    GetContentClient()->gpu()->InitializeRegistry(registry.get());

  std::unique_ptr<QueueingConnectionFilter> filter =
      std::make_unique<QueueingConnectionFilter>(GetIOTaskRunner(),
                                                 std::move(registry));
  release_pending_requests_closure_ = filter->GetReleaseCallback();

  filter->AddInterfaces();
  GetServiceManagerConnection()->AddConnectionFilter(std::move(filter));

  StartServiceManagerConnection();

  memory_pressure_listener_ =
      std::make_unique<base::MemoryPressureListener>(base::BindRepeating(
          &GpuChildThread::OnMemoryPressure, base::Unretained(this)));
}

void GpuChildThread::CreateVizMainService(
    viz::mojom::VizMainAssociatedRequest request) {
  viz_main_.BindAssociated(std::move(request));
}

bool GpuChildThread::in_process_gpu() const {
  return viz_main_.gpu_service()->gpu_info().in_process_gpu;
}

bool GpuChildThread::Send(IPC::Message* msg) {
  // The GPU process must never send a synchronous IPC message to the browser
  // process. This could result in deadlock.
  DCHECK(!msg->is_sync());

  return ChildThreadImpl::Send(msg);
}

void GpuChildThread::OnAssociatedInterfaceRequest(
    const std::string& name,
    mojo::ScopedInterfaceEndpointHandle handle) {
  if (!associated_interfaces_.TryBindInterface(name, &handle))
    ChildThreadImpl::OnAssociatedInterfaceRequest(name, std::move(handle));
}

void GpuChildThread::OnInitializationFailed() {
  OnChannelError();
}

void GpuChildThread::OnGpuServiceConnection(viz::GpuServiceImpl* gpu_service) {
  media::AndroidOverlayMojoFactoryCB overlay_factory_cb;
#if defined(OS_ANDROID)
  overlay_factory_cb = base::Bind(&GpuChildThread::CreateAndroidOverlay,
                                  base::ThreadTaskRunnerHandle::Get());
  gpu_service->media_gpu_channel_manager()->SetOverlayFactory(
      overlay_factory_cb);
#endif

  // Only set once per process instance.
  service_factory_.reset(new GpuServiceFactory(
      gpu_service->gpu_preferences(),
      gpu_service->gpu_channel_manager()->gpu_driver_bug_workarounds(),
      gpu_service->gpu_feature_info(),
      gpu_service->media_gpu_channel_manager()->AsWeakPtr(),
      std::move(overlay_factory_cb)));

  if (GetContentClient()->gpu()) {  // NULL in tests.
    GetContentClient()->gpu()->GpuServiceInitialized(
        gpu_service->gpu_preferences());
  }

  release_pending_requests_closure_.Run();
}

void GpuChildThread::PostCompositorThreadCreated(
    base::SingleThreadTaskRunner* task_runner) {
  auto* gpu_client = GetContentClient()->gpu();
  if (gpu_client)
    gpu_client->PostCompositorThreadCreated(task_runner);
}

void GpuChildThread::QuitMainMessageLoop() {
  ProcessShutdown();
}

void GpuChildThread::BindServiceFactoryRequest(
    service_manager::mojom::ServiceFactoryRequest request) {
  DVLOG(1) << "GPU: Binding service_manager::mojom::ServiceFactoryRequest";
  DCHECK(service_factory_);
  service_factory_bindings_.AddBinding(service_factory_.get(),
                                       std::move(request));
}

void GpuChildThread::OnMemoryPressure(
    base::MemoryPressureListener::MemoryPressureLevel level) {
  if (level != base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL)
    return;

  base::allocator::ReleaseFreeMemory();
  if (viz_main_.discardable_shared_memory_manager())
    viz_main_.discardable_shared_memory_manager()->ReleaseFreeMemory();
  SkGraphics::PurgeAllCaches();
}

#if defined(OS_ANDROID)
// static
std::unique_ptr<media::AndroidOverlay> GpuChildThread::CreateAndroidOverlay(
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    const base::UnguessableToken& routing_token,
    media::AndroidOverlayConfig config) {
  media::mojom::AndroidOverlayProviderPtr overlay_provider;
  if (main_task_runner->RunsTasksInCurrentSequence()) {
    ChildThread::Get()->GetConnector()->BindInterface(
        content::mojom::kBrowserServiceName, &overlay_provider);
  } else {
    // Create a connector on this sequence and bind it on the main thread.
    service_manager::mojom::ConnectorRequest request;
    auto connector = service_manager::Connector::Create(&request);
    connector->BindInterface(content::mojom::kBrowserServiceName,
                             &overlay_provider);
    auto bind_connector_request =
        [](service_manager::mojom::ConnectorRequest request) {
          ChildThread::Get()->GetConnector()->BindConnectorRequest(
              std::move(request));
        };
    main_task_runner->PostTask(
        FROM_HERE, base::BindOnce(bind_connector_request, std::move(request)));
  }

  return std::make_unique<media::MojoAndroidOverlay>(
      std::move(overlay_provider), std::move(config), routing_token);
}
#endif

}  // namespace content
