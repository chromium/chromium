// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file exposes services from the browser to renderer processes.

// clang-format off
#include "content/browser/renderer_host/render_process_host_impl.h"
// clang-format on

#include "components/discardable_memory/public/mojom/discardable_shared_memory_manager.mojom.h"
#include "components/discardable_memory/service/discardable_shared_memory_manager.h"
#include "components/metrics/single_sample_metrics.h"
#include "components/viz/host/gpu_client.h"
#include "content/browser/blob_storage/blob_registry_wrapper.h"
#include "content/browser/compositor/surface_utils.h"
#include "content/browser/field_trial_recorder.h"
#include "content/browser/file_system/file_system_manager_impl.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "content/browser/media/media_internals.h"
#include "content/browser/mime_registry_impl.h"
#include "content/browser/push_messaging/push_messaging_manager.h"
#include "content/browser/renderer_host/embedded_frame_sink_provider_impl.h"
#include "content/browser/renderer_host/media/media_stream_track_metrics_host.h"
#include "content/browser/renderer_host/p2p/socket_dispatcher_host.h"
#include "content/browser/renderer_host/render_message_filter.h"
#include "content/browser/renderer_host/render_widget_helper.h"
#include "content/common/features.h"
#include "content/common/render_message_filter.mojom.h"
#include "content/public/browser/device_service.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "media/base/media_switches.h"
#include "media/mojo/mojom/interface_factory.mojom.h"
#include "media/mojo/mojom/video_encoder_metrics_provider.mojom.h"
#include "services/device/public/mojom/power_monitor.mojom.h"
#include "services/device/public/mojom/screen_orientation.mojom.h"
#include "services/device/public/mojom/time_zone_monitor.mojom.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/mojom/ukm_interface.mojom.h"
#include "services/metrics/ukm_recorder_factory_impl.h"
#include "services/resource_coordinator/public/mojom/memory_instrumentation/memory_instrumentation.mojom.h"
#include "third_party/blink/public/mojom/plugins/plugin_registry.mojom.h"
#include "third_party/blink/public/public_buildflags.h"

#if BUILDFLAG(IS_ANDROID)
#include "content/browser/android/java_interfaces_impl.h"
#include "content/browser/font_unique_name_lookup/font_unique_name_lookup_service.h"
#include "content/public/browser/android/java_interfaces.h"
#include "storage/browser/database/database_tracker.h"
#include "third_party/blink/public/mojom/android_font_lookup/android_font_lookup.mojom.h"
#include "third_party/blink/public/mojom/webdatabase/web_database.mojom.h"
#endif

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include "components/services/font/public/mojom/font_service.mojom.h"  // nogncheck
#include "content/browser/font_service.h"  // nogncheck
#include "content/browser/media/video_encode_accelerator_provider_launcher.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "content/browser/sandbox_support_mac_impl.h"
#include "content/common/sandbox_support_mac.mojom.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "components/services/font_data/font_data_service_impl.h"
#include "content/browser/renderer_host/dwrite_font_proxy_impl_win.h"
#include "content/public/common/font_cache_dispatcher_win.h"
#include "content/public/common/font_cache_win.mojom.h"
#endif

#if BUILDFLAG(ENABLE_PLUGINS)
#include "content/browser/renderer_host/plugin_registry_impl.h"
#endif

#if BUILDFLAG(USE_MINIKIN_HYPHENATION)
#include "content/browser/hyphenation/hyphenation_impl.h"
#endif

namespace content {

namespace {
RenderProcessHost::BindHostReceiverInterceptor&
GetBindHostReceiverInterceptor() {
  static base::NoDestructor<RenderProcessHost::BindHostReceiverInterceptor>
      interceptor;
  return *interceptor;
}
}  // namespace

// static
void RenderProcessHost::InterceptBindHostReceiverForTesting(
    BindHostReceiverInterceptor callback) {
  GetBindHostReceiverInterceptor() = std::move(callback);
}

void RenderProcessHostImpl::OnBindHostReceiver(
    mojo::GenericPendingReceiver receiver) {
#if BUILDFLAG(IS_ANDROID)
  // content::GetGlobalJavaInterfaces() works only on the UI Thread.
  if (auto r = receiver.As<blink::mojom::AndroidFontLookup>()) {
    content::GetGlobalJavaInterfaces()->GetInterface(std::move(r));
    return;
  }
#endif

  GetContentClient()->browser()->BindHostReceiverForRenderer(
      this, std::move(receiver));
}

void RenderProcessHostImpl::RegisterMojoInterfaces() {
  auto registry = std::make_unique<service_manager::BinderRegistry>();

  registry->AddInterface(base::BindRepeating(
      [](int rph_id, scoped_refptr<RenderWidgetHelper> helper,
         mojo::PendingReceiver<mojom::RenderMessageFilter> receiver) {
        // We should only ever see one instance of this created per
        // RenderProcessHost since it is maintained by the `RenderThreadImpl`.
        mojo::MakeSelfOwnedReceiver(
            std::make_unique<RenderMessageFilter>(rph_id, helper.get()),
            std::move(receiver));
      },
      GetID(), widget_helper_));

  AddUIThreadInterface(
      registry.get(),
      base::BindRepeating(
          [](mojo::PendingReceiver<device::mojom::TimeZoneMonitor> receiver) {
            GetDeviceService().BindTimeZoneMonitor(std::move(receiver));
          }));

  AddUIThreadInterface(
      registry.get(),
      base::BindRepeating(
          [](mojo::PendingReceiver<device::mojom::PowerMonitor> receiver) {
            GetDeviceService().BindPowerMonitor(std::move(receiver));
          }));

  AddUIThreadInterface(
      registry.get(),
      base::BindRepeating(
          [](mojo::PendingReceiver<device::mojom::ScreenOrientationListener>
                 receiver) {
            GetDeviceService().BindScreenOrientationListener(
                std::move(receiver));
          }));

  AddUIThreadInterface(
      registry.get(),
      base::BindRepeating(
          &RenderProcessHostImpl::CreateEmbeddedFrameSinkProvider,
          instance_weak_factory_.GetWeakPtr()));

  AddUIThreadInterface(
      registry.get(),
      base::BindRepeating(&RenderProcessHostImpl::BindCompositingModeReporter,
                          instance_weak_factory_.GetWeakPtr()));

  AddUIThreadInterface(
      registry.get(),
      base::BindRepeating(&RenderProcessHostImpl::CreateDomStorageProvider,
                          instance_weak_factory_.GetWeakPtr()));

#if BUILDFLAG(IS_ANDROID)
  // TODO(crbug.com/333756088): WebSQL is disabled everywhere except Android
  // WebView.
  AddUIThreadInterface(
      registry.get(),
      base::BindRepeating(&RenderProcessHostImpl::BindWebDatabaseHostImpl,
                          instance_weak_factory_.GetWeakPtr()));
#endif  // BUILDFLAG(IS_ANDROID)

  AddUIThreadInterface(
      registry.get(),
      base::BindRepeating(
          [](base::WeakPtr<RenderProcessHostImpl> host,
             mojo::PendingReceiver<
                 memory_instrumentation::mojom::CoordinatorConnector>
                 receiver) {
            if (!host) {
              return;
            }
            host->coordinator_connector_receiver_.reset();
            host->coordinator_connector_receiver_.Bind(std::move(receiver));
            if (!host->GetProcess().IsValid()) {
              // We only want to accept messages from this interface once we
              // have a known PID.
              host->coordinator_connector_receiver_.Pause();
            }
          },
          instance_weak_factory_.GetWeakPtr()));

  registry->AddInterface(
      base::BindRepeating(&MimeRegistryImpl::Create),
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN,
           base::TaskPriority::USER_BLOCKING}));
#if BUILDFLAG(USE_MINIKIN_HYPHENATION)
#if !BUILDFLAG(IS_ANDROID)
  hyphenation::HyphenationImpl::RegisterGetDictionary();
#endif
  registry->AddInterface(
      base::BindRepeating(&hyphenation::HyphenationImpl::Create),
      hyphenation::HyphenationImpl::GetTaskRunner());
#endif
#if BUILDFLAG(IS_ANDROID)
  if (base::FeatureList::IsEnabled(features::kFontSrcLocalMatching)) {
    registry->AddInterface(
        base::BindRepeating(&FontUniqueNameLookupService::Create),
        FontUniqueNameLookupService::GetTaskRunner());
  }
#endif

#if BUILDFLAG(IS_WIN)
  registry->AddInterface(
      base::BindRepeating(&DWriteFontProxyImpl::Create),
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::TaskPriority::USER_BLOCKING, base::MayBlock()}));
#endif

  file_system_manager_impl_.reset(new FileSystemManagerImpl(
      GetID(), storage_partition_impl_->GetFileSystemContext(),
      ChromeBlobStorageContext::GetFor(GetBrowserContext())));

  AddUIThreadInterface(
      registry.get(), base::BindRepeating(&viz::GpuClient::Add,
                                          base::Unretained(gpu_client_.get())));

  registry->AddInterface(
      base::BindRepeating(&GpuDataManagerImpl::BindReceiver));

  // Note, the base::Unretained() is safe because the target object has an IO
  // thread deleter and the callback is also targeting the IO thread.  When
  // the RPHI is destroyed it also triggers the destruction of the registry
  // on the IO thread.
  media_stream_track_metrics_host_.reset(new MediaStreamTrackMetricsHost());
  registry->AddInterface(base::BindRepeating(
      &MediaStreamTrackMetricsHost::BindReceiver,
      base::Unretained(media_stream_track_metrics_host_.get())));

  registry->AddInterface(
      base::BindRepeating(&metrics::CreateSingleSampleMetricsProvider));

  AddUIThreadInterface(
      registry.get(),
      base::BindRepeating(&RenderProcessHostImpl::CreateMediaLogRecordHost,
                          instance_weak_factory_.GetWeakPtr()));

  AddUIThreadInterface(registry.get(),
                       base::BindRepeating(&FieldTrialRecorder::Create));

  associated_interfaces_ =
      std::make_unique<blink::AssociatedInterfaceRegistry>();
  blink::AssociatedInterfaceRegistry* associated_registry =
      associated_interfaces_.get();

  // This base::Unretained() usage is safe since the associated_registry is
  // owned by this RPHI.
  associated_registry->AddInterface<mojom::RendererHost>(base::BindRepeating(
      &RenderProcessHostImpl::CreateRendererHost, base::Unretained(this)));

  registry->AddInterface(
      base::BindRepeating(&BlobRegistryWrapper::Bind,
                          storage_partition_impl_->GetBlobRegistry(), GetID()));

#if BUILDFLAG(ENABLE_PLUGINS)
  // Initialization can happen more than once (in the case of a child process
  // crash), but we don't want to lose the plugin registry in this case.
  if (!plugin_registry_) {
    plugin_registry_ = std::make_unique<PluginRegistryImpl>(GetID());
  }
  AddUIThreadInterface(
      registry.get(),
      base::BindRepeating(&RenderProcessHostImpl::BindPluginRegistry,
                          instance_weak_factory_.GetWeakPtr()));
#else
  // On platforms where plugins are disabled, the PluginRegistry interface is
  // never bound. This still results in posting a task on the UI thread to
  // look for the interface which can be slow. Instead, drop the interface
  // immediately on the IO thread by binding en empty interface handler.
  registry->AddInterface(base::BindRepeating(
      [](mojo::PendingReceiver<blink::mojom::PluginRegistry> receiver) {}));
#endif

  AddUIThreadInterface(
      registry.get(),
      base::BindRepeating(&RenderProcessHostImpl::BindMediaInterfaceProxy,
                          instance_weak_factory_.GetWeakPtr()));

  AddUIThreadInterface(
      registry.get(),
      base::BindRepeating(
          &RenderProcessHostImpl::BindVideoEncoderMetricsProvider,
          instance_weak_factory_.GetWeakPtr()));

  AddUIThreadInterface(
      registry.get(),
      base::BindRepeating(&RenderProcessHostImpl::BindAecDumpManager,
                          instance_weak_factory_.GetWeakPtr()));

#if BUILDFLAG(IS_FUCHSIA)
  AddUIThreadInterface(
      registry.get(),
      base::BindRepeating(&RenderProcessHostImpl::BindMediaCodecProvider,
                          instance_weak_factory_.GetWeakPtr()));
#endif

  // ---- Please do not register interfaces below this line ------
  //
  // This call should be done after registering all interfaces above, so that
  // embedder can override any interfaces. The fact that registry calls
  // the last registration for the name allows us to easily override interfaces.
  GetContentClient()->browser()->ExposeInterfacesToRenderer(
      registry.get(), associated_interfaces_.get(), this);

  DCHECK(child_host_pending_receiver_);
  io_thread_host_impl_.emplace(
      GetIOThreadTaskRunner({}), GetID(), instance_weak_factory_.GetWeakPtr(),
      std::move(registry), std::move(child_host_pending_receiver_));
}

void RenderProcessHostImpl::IOThreadHostImpl::BindHostReceiver(
    mojo::GenericPendingReceiver receiver) {
  const auto& interceptor = GetBindHostReceiverInterceptor();
  if (interceptor) {
    interceptor.Run(render_process_id_, &receiver);
    if (!receiver) {
      return;
    }
  }

#if BUILDFLAG(IS_WIN)
  if (base::FeatureList::IsEnabled(features::kSkiaFontService)) {
    if (auto font_data_receiver =
            receiver.As<font_data_service::mojom::FontDataService>()) {
      font_data_service::FontDataServiceImpl::ConnectToFontService(
          std::move(font_data_receiver));
      return;
    }
  }
#endif

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  if (auto font_receiver = receiver.As<font_service::mojom::FontService>()) {
    ConnectToFontService(std::move(font_receiver));
    return;
  }

  if (base::FeatureList::IsEnabled(media::kUseOutOfProcessVideoEncoding)) {
    if (auto r = receiver.As<media::mojom::VideoEncodeAcceleratorProvider>()) {
      if (!video_encode_accelerator_factory_remote_.is_bound()) {
        LaunchVideoEncodeAcceleratorProviderFactory(
            video_encode_accelerator_factory_remote_
                .BindNewPipeAndPassReceiver());
        video_encode_accelerator_factory_remote_.reset_on_disconnect();
      }

      if (!video_encode_accelerator_factory_remote_.is_bound()) {
        return;
      }

      video_encode_accelerator_factory_remote_
          ->CreateVideoEncodeAcceleratorProvider(std::move(r));
      return;
    }
  }

  if (auto r = receiver.As<mojom::ThreadTypeSwitcher>()) {
    child_thread_type_switcher_.Bind(std::move(r));
    return;
  }
#endif  // (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS))

#if BUILDFLAG(IS_WIN)
  if (auto r = receiver.As<mojom::FontCacheWin>()) {
    FontCacheDispatcher::Create(std::move(r));
    return;
  }
#endif

#if BUILDFLAG(IS_MAC)
  if (auto r = receiver.As<mojom::SandboxSupportMac>()) {
    static base::NoDestructor<SandboxSupportMacImpl> sandbox_support;
    sandbox_support->BindReceiver(std::move(r));
    return;
  }
#endif

  if (auto r = receiver.As<
               discardable_memory::mojom::DiscardableSharedMemoryManager>()) {
    discardable_memory::DiscardableSharedMemoryManager::Get()->Bind(
        std::move(r));
    return;
  }

  if (auto r = receiver.As<ukm::mojom::UkmRecorderFactory>()) {
    metrics::UkmRecorderFactoryImpl::Create(ukm::UkmRecorder::Get(),
                                            std::move(r));
    return;
  }

#if BUILDFLAG(IS_ANDROID)
  // Bind the font lookup on the IO thread as an optimization to avoid
  // running navigation critical path tasks on the UI thread.
  if (auto r = receiver.As<blink::mojom::AndroidFontLookup>()) {
    GetGlobalJavaInterfacesOnIOThread()->GetInterface(std::move(r));
    return;
  }
#endif

  std::string interface_name = *receiver.interface_name();
  mojo::ScopedMessagePipeHandle pipe = receiver.PassPipe();
  if (binders_->TryBindInterface(interface_name, &pipe)) {
    return;
  }

  receiver = mojo::GenericPendingReceiver(interface_name, std::move(pipe));
  if (!receiver) {
    return;
  }

  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&IOThreadHostImpl::BindHostReceiverOnUIThread,
                                weak_host_, std::move(receiver)));
}

// static
void RenderProcessHostImpl::IOThreadHostImpl::BindHostReceiverOnUIThread(
    base::WeakPtr<RenderProcessHostImpl> weak_host,
    mojo::GenericPendingReceiver receiver) {
  if (weak_host) {
    weak_host->OnBindHostReceiver(std::move(receiver));
  }
}

}  // namespace content
