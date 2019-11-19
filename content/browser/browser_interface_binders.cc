// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browser_interface_binders.h"

#include "base/callback.h"
#include "base/feature_list.h"
#include "build/build_config.h"
#include "content/browser/background_fetch/background_fetch_service_impl.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/content_index/content_index_service_impl.h"
#include "content/browser/cookie_store/cookie_store_context.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/browser/image_capture/image_capture_impl.h"
#include "content/browser/keyboard_lock/keyboard_lock_service_impl.h"
#include "content/browser/media/session/media_session_service_impl.h"
#include "content/browser/picture_in_picture/picture_in_picture_service_impl.h"
#include "content/browser/process_internals/process_internals.mojom.h"
#include "content/browser/process_internals/process_internals_ui.h"
#include "content/browser/renderer_host/media/media_devices_dispatcher_host.h"
#include "content/browser/renderer_host/media/media_stream_dispatcher_host.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/screen_enumeration/screen_enumeration_impl.h"
#include "content/browser/service_worker/service_worker_provider_host.h"
#include "content/browser/speech/speech_recognition_dispatcher_host.h"
#include "content/browser/wake_lock/wake_lock_service_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/worker_host/dedicated_worker_host.h"
#include "content/browser/worker_host/shared_worker_host.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/shared_worker_instance.h"
#include "content/public/browser/webvr_service_provider.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/url_constants.h"
#include "device/gamepad/gamepad_monitor.h"
#include "device/gamepad/public/mojom/gamepad.mojom.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "media/capture/mojom/image_capture.mojom.h"
#include "media/mojo/mojom/video_decode_perf_history.mojom.h"
#include "media/mojo/services/video_decode_perf_history.h"
#include "services/device/public/mojom/constants.mojom.h"
#include "services/device/public/mojom/sensor_provider.mojom.h"
#include "services/device/public/mojom/vibration_manager.mojom.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/shape_detection/public/mojom/barcodedetection_provider.mojom.h"
#include "services/shape_detection/public/mojom/facedetection_provider.mojom.h"
#include "services/shape_detection/public/mojom/shape_detection_service.mojom.h"
#include "services/shape_detection/public/mojom/textdetection.mojom.h"
#include "third_party/blink/public/mojom/appcache/appcache.mojom.h"
#include "third_party/blink/public/mojom/background_fetch/background_fetch.mojom.h"
#include "third_party/blink/public/mojom/bluetooth/web_bluetooth.mojom.h"
#include "third_party/blink/public/mojom/choosers/color_chooser.mojom.h"
#include "third_party/blink/public/mojom/content_index/content_index.mojom.h"
#include "third_party/blink/public/mojom/cookie_store/cookie_store.mojom.h"
#include "third_party/blink/public/mojom/credentialmanager/credential_manager.mojom.h"
#include "third_party/blink/public/mojom/filesystem/file_system.mojom.h"
#include "third_party/blink/public/mojom/idle/idle_manager.mojom.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom.h"
#include "third_party/blink/public/mojom/keyboard_lock/keyboard_lock.mojom.h"
#include "third_party/blink/public/mojom/locks/lock_manager.mojom.h"
#include "third_party/blink/public/mojom/mediasession/media_session.mojom.h"
#include "third_party/blink/public/mojom/mediastream/media_devices.mojom.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"
#include "third_party/blink/public/mojom/payments/payment_app.mojom.h"
#include "third_party/blink/public/mojom/permissions/permission.mojom.h"
#include "third_party/blink/public/mojom/picture_in_picture/picture_in_picture.mojom.h"
#include "third_party/blink/public/mojom/presentation/presentation.mojom.h"
#include "third_party/blink/public/mojom/sms/sms_receiver.mojom.h"
#include "third_party/blink/public/mojom/speech/speech_recognizer.mojom.h"
#include "third_party/blink/public/mojom/speech/speech_synthesis.mojom.h"
#include "third_party/blink/public/mojom/usb/web_usb_service.mojom.h"
#include "third_party/blink/public/mojom/wake_lock/wake_lock.mojom.h"
#include "third_party/blink/public/mojom/webaudio/audio_context_manager.mojom.h"
#include "third_party/blink/public/mojom/webauthn/authenticator.mojom.h"
#include "third_party/blink/public/mojom/webauthn/virtual_authenticator.mojom.h"
#include "third_party/blink/public/mojom/webtransport/quic_transport_connector.mojom.h"

#if !defined(OS_ANDROID)
#include "base/command_line.h"
#include "content/browser/installedapp/installed_app_provider_impl_default.h"
#include "content/public/common/content_switches.h"
#include "third_party/blink/public/mojom/hid/hid.mojom.h"
#include "third_party/blink/public/mojom/installedapp/installed_app_provider.mojom.h"
#include "third_party/blink/public/mojom/serial/serial.mojom.h"
#endif

#if defined(OS_ANDROID)
#include "content/browser/android/date_time_chooser_android.h"
#include "content/browser/android/text_suggestion_host_mojo_impl_android.h"
#include "content/browser/renderer_host/render_widget_host_view_android.h"
#include "services/device/public/mojom/nfc.mojom.h"
#include "third_party/blink/public/mojom/hid/hid.mojom.h"
#include "third_party/blink/public/mojom/input/input_host.mojom.h"
#endif

namespace content {
namespace internal {

namespace {

// Forwards service receivers to Service Manager since the renderer cannot
// launch out-of-process services on is own.
template <typename Interface>
void ForwardServiceReceiver(const char* service_name,
                            RenderFrameHostImpl* host,
                            mojo::PendingReceiver<Interface> receiver) {
  auto* connector =
      BrowserContext::GetConnectorFor(host->GetProcess()->GetBrowserContext());
  connector->Connect(service_name, std::move(receiver));
}

void BindShapeDetectionServiceOnIOThread(
    mojo::PendingReceiver<shape_detection::mojom::ShapeDetectionService>
        receiver) {
  auto* gpu = GpuProcessHost::Get();
  if (gpu)
    gpu->RunService(std::move(receiver));
}

shape_detection::mojom::ShapeDetectionService* GetShapeDetectionService() {
  static base::NoDestructor<
      mojo::Remote<shape_detection::mojom::ShapeDetectionService>>
      remote;
  if (!*remote) {
    base::PostTask(FROM_HERE, {BrowserThread::IO},
                   base::BindOnce(&BindShapeDetectionServiceOnIOThread,
                                  remote->BindNewPipeAndPassReceiver()));
    remote->reset_on_disconnect();
  }

  return remote->get();
}

void BindBarcodeDetectionProvider(
    mojo::PendingReceiver<shape_detection::mojom::BarcodeDetectionProvider>
        receiver) {
  GetShapeDetectionService()->BindBarcodeDetectionProvider(std::move(receiver));
}

void BindFaceDetectionProvider(
    mojo::PendingReceiver<shape_detection::mojom::FaceDetectionProvider>
        receiver) {
  GetShapeDetectionService()->BindFaceDetectionProvider(std::move(receiver));
}

void BindTextDetection(
    mojo::PendingReceiver<shape_detection::mojom::TextDetection> receiver) {
  GetShapeDetectionService()->BindTextDetection(std::move(receiver));
}

void BindColorChooserFactoryForFrame(
    RenderFrameHost* host,
    mojo::PendingReceiver<blink::mojom::ColorChooserFactory> receiver) {
  auto* web_contents =
      static_cast<WebContentsImpl*>(WebContents::FromRenderFrameHost(host));
  web_contents->OnColorChooserFactoryReceiver(std::move(receiver));
}

void BindProcessInternalsHandler(
    content::RenderFrameHost* host,
    mojo::PendingReceiver<::mojom::ProcessInternalsHandler> receiver) {
  // This should not be requested by subframes, so terminate the renderer if
  // it issues such a request.
  if (host->GetParent()) {
    host->GetProcess()->ShutdownForBadMessage(
        content::RenderProcessHost::CrashReportMode::GENERATE_CRASH_DUMP);
    return;
  }

  auto* contents = WebContents::FromRenderFrameHost(host);
  DCHECK_EQ(contents->GetLastCommittedURL().host_piece(),
            kChromeUIProcessInternalsHost);
  static_cast<ProcessInternalsUI*>(contents->GetWebUI()->GetController())
      ->BindProcessInternalsHandler(std::move(receiver), host);
}

#if defined(OS_ANDROID)
void BindDateTimeChooserForFrame(
    content::RenderFrameHost* host,
    mojo::PendingReceiver<blink::mojom::DateTimeChooser> receiver) {
  auto* date_time_chooser = DateTimeChooserAndroid::FromWebContents(
      WebContents::FromRenderFrameHost(host));
  date_time_chooser->OnDateTimeChooserReceiver(std::move(receiver));
}

void BindTextSuggestionHostForFrame(
    content::RenderFrameHost* host,
    mojo::PendingReceiver<blink::mojom::TextSuggestionHost> receiver) {
  auto* view = static_cast<RenderWidgetHostViewAndroid*>(host->GetView());
  DCHECK(view);
  TextSuggestionHostMojoImplAndroid::Create(view->text_suggestion_host(),
                                            std::move(receiver));
}
#endif

template <typename Interface>
base::RepeatingCallback<void(mojo::PendingReceiver<Interface>)>
BindDedicatedWorkerReceiver(
    void (RenderProcessHost::*method)(mojo::PendingReceiver<Interface>),
    DedicatedWorkerHost* host) {
  return base::BindRepeating(
      [](DedicatedWorkerHost* host,
         void (RenderProcessHost::*method)(mojo::PendingReceiver<Interface>),
         mojo::PendingReceiver<Interface> receiver) {
        RenderProcessHost* process_host = host->GetProcessHost();
        if (process_host)
          (process_host->*method)(std::move(receiver));
      },
      base::Unretained(host), method);
}

template <typename Interface>
base::RepeatingCallback<void(const url::Origin& origin,
                             mojo::PendingReceiver<Interface>)>
BindDedicatedWorkerReceiverForOrigin(
    void (RenderProcessHost::*method)(const url::Origin& origin,
                                      mojo::PendingReceiver<Interface>),
    DedicatedWorkerHost* host) {
  return base::BindRepeating(
      [](DedicatedWorkerHost* host,
         void (RenderProcessHost::*method)(const url::Origin& origin,
                                           mojo::PendingReceiver<Interface>),
         const url::Origin& origin, mojo::PendingReceiver<Interface> receiver) {
        RenderProcessHost* process_host = host->GetProcessHost();
        if (process_host)
          (process_host->*method)(origin, std::move(receiver));
      },
      base::Unretained(host), method);
}

template <typename Interface>
base::RepeatingCallback<void(mojo::PendingReceiver<Interface>)>
BindSharedWorkerReceiver(
    void (RenderProcessHost::*method)(mojo::PendingReceiver<Interface>),
    SharedWorkerHost* host) {
  return base::BindRepeating(
      [](SharedWorkerHost* host,
         void (RenderProcessHost::*method)(mojo::PendingReceiver<Interface>),
         mojo::PendingReceiver<Interface> receiver) {
        RenderProcessHost* process_host = host->GetProcessHost();
        if (process_host)
          (process_host->*method)(std::move(receiver));
      },
      base::Unretained(host), method);
}

template <typename Interface>
base::RepeatingCallback<void(const url::Origin& origin,
                             mojo::PendingReceiver<Interface>)>
BindSharedWorkerReceiverForOrigin(
    void (RenderProcessHost::*method)(const url::Origin& origin,
                                      mojo::PendingReceiver<Interface>),
    SharedWorkerHost* host) {
  return base::BindRepeating(
      [](SharedWorkerHost* host,
         void (RenderProcessHost::*method)(const url::Origin& origin,
                                           mojo::PendingReceiver<Interface>),
         const url::Origin& origin, mojo::PendingReceiver<Interface> receiver) {
        RenderProcessHost* process_host = host->GetProcessHost();
        if (process_host)
          (process_host->*method)(origin, std::move(receiver));
      },
      base::Unretained(host), method);
}

template <typename Interface>
base::RepeatingCallback<void(mojo::PendingReceiver<Interface>)>
BindServiceWorkerReceiver(
    void (RenderProcessHost::*method)(mojo::PendingReceiver<Interface>),
    ServiceWorkerProviderHost* host) {
  return base::BindRepeating(
      [](ServiceWorkerProviderHost* host,
         void (RenderProcessHost::*method)(mojo::PendingReceiver<Interface>),
         mojo::PendingReceiver<Interface> receiver) {
        DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
        DCHECK(host->IsProviderForServiceWorker());

        content::RunOrPostTaskOnThread(
            FROM_HERE, BrowserThread::UI,
            base::BindOnce(
                [](ServiceWorkerProviderHost* host,
                   void (RenderProcessHost::*method)(
                       mojo::PendingReceiver<Interface>),
                   mojo::PendingReceiver<Interface> receiver) {
                  RenderProcessHost* process_host = host->GetProcessHost();
                  if (process_host)
                    (process_host->*method)(std::move(receiver));
                },
                base::Unretained(host), method, std::move(receiver)));
      },
      base::Unretained(host), method);
}

template <typename Interface>
base::RepeatingCallback<void(const ServiceWorkerVersionInfo&,
                             mojo::PendingReceiver<Interface>)>
BindServiceWorkerReceiverForOrigin(
    void (RenderProcessHost::*method)(const url::Origin& origin,
                                      mojo::PendingReceiver<Interface>),
    ServiceWorkerProviderHost* host) {
  return base::BindRepeating(
      [](ServiceWorkerProviderHost* host,
         void (RenderProcessHost::*method)(const url::Origin& origin,
                                           mojo::PendingReceiver<Interface>),
         const ServiceWorkerVersionInfo& info,
         mojo::PendingReceiver<Interface> receiver) {
        DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
        DCHECK(host->IsProviderForServiceWorker());

        auto origin = info.script_origin;

        content::RunOrPostTaskOnThread(
            FROM_HERE, BrowserThread::UI,
            base::BindOnce(
                [](ServiceWorkerProviderHost* host,
                   void (RenderProcessHost::*method)(
                       const url::Origin& origin,
                       mojo::PendingReceiver<Interface>),
                   mojo::PendingReceiver<Interface> receiver,
                   const url::Origin& origin) {
                  RenderProcessHost* process_host = host->GetProcessHost();
                  if (process_host)
                    (process_host->*method)(origin, std::move(receiver));
                },
                base::Unretained(host), method, std::move(receiver), origin));
      },
      base::Unretained(host), method);
}

}  // namespace

// Documents/frames
void PopulateFrameBinders(RenderFrameHostImpl* host,
                          service_manager::BinderMap* map) {
  map->Add<blink::mojom::AppCacheBackend>(base::BindRepeating(
      &RenderFrameHostImpl::CreateAppCacheBackend, base::Unretained(host)));

  map->Add<blink::mojom::AudioContextManager>(base::BindRepeating(
      &RenderFrameHostImpl::GetAudioContextManager, base::Unretained(host)));

  map->Add<blink::mojom::ContactsManager>(base::BindRepeating(
      &RenderFrameHostImpl::GetContactsManager, base::Unretained(host)));

  map->Add<blink::mojom::FileSystemManager>(base::BindRepeating(
      &RenderFrameHostImpl::GetFileSystemManager, base::Unretained(host)));

  map->Add<blink::mojom::IdleManager>(base::BindRepeating(
      &RenderFrameHostImpl::GetIdleManager, base::Unretained(host)));

  map->Add<blink::mojom::PermissionService>(base::BindRepeating(
      &RenderFrameHostImpl::CreatePermissionService, base::Unretained(host)));

  map->Add<blink::mojom::PresentationService>(base::BindRepeating(
      &RenderFrameHostImpl::GetPresentationService, base::Unretained(host)));

  map->Add<blink::mojom::SpeechRecognizer>(
      base::BindRepeating(&SpeechRecognitionDispatcherHost::Create,
                          host->GetProcess()->GetID(), host->GetRoutingID()),
      base::CreateSingleThreadTaskRunner({BrowserThread::IO}));

  map->Add<blink::mojom::SpeechSynthesis>(base::BindRepeating(
      &RenderFrameHostImpl::GetSpeechSynthesis, base::Unretained(host)));

  map->Add<blink::mojom::ScreenEnumeration>(
      base::BindRepeating(&ScreenEnumerationImpl::Create));

  if (base::FeatureList::IsEnabled(features::kSmsReceiver)) {
    map->Add<blink::mojom::SmsReceiver>(base::BindRepeating(
        &RenderFrameHostImpl::BindSmsReceiverReceiver, base::Unretained(host)));
  }

  map->Add<blink::mojom::WebUsbService>(base::BindRepeating(
      &RenderFrameHostImpl::CreateWebUsbService, base::Unretained(host)));

  map->Add<blink::mojom::LockManager>(base::BindRepeating(
      &RenderFrameHostImpl::CreateLockManager, base::Unretained(host)));

  map->Add<blink::mojom::IDBFactory>(base::BindRepeating(
      &RenderFrameHostImpl::CreateIDBFactory, base::Unretained(host)));

  map->Add<blink::mojom::FileChooser>(base::BindRepeating(
      &RenderFrameHostImpl::GetFileChooser, base::Unretained(host)));

  map->Add<device::mojom::GamepadMonitor>(
      base::BindRepeating(&device::GamepadMonitor::Create));

  map->Add<device::mojom::SensorProvider>(base::BindRepeating(
      &RenderFrameHostImpl::GetSensorProvider, base::Unretained(host)));

  map->Add<device::mojom::VibrationManager>(base::BindRepeating(
      &ForwardServiceReceiver<device::mojom::VibrationManager>,
      device::mojom::kServiceName, base::Unretained(host)));

  map->Add<payments::mojom::PaymentManager>(base::BindRepeating(
      &RenderFrameHostImpl::CreatePaymentManager, base::Unretained(host)));

  map->Add<blink::mojom::WebBluetoothService>(base::BindRepeating(
      &RenderFrameHostImpl::CreateWebBluetoothService, base::Unretained(host)));

  map->Add<blink::mojom::PushMessaging>(base::BindRepeating(
      &RenderFrameHostImpl::GetPushMessaging, base::Unretained(host)));

  map->Add<blink::mojom::CredentialManager>(base::BindRepeating(
      &RenderFrameHostImpl::GetCredentialManager, base::Unretained(host)));

  map->Add<blink::mojom::Authenticator>(base::BindRepeating(
      &RenderFrameHostImpl::GetAuthenticator, base::Unretained(host)));

  map->Add<blink::mojom::QuicTransportConnector>(
      base::BindRepeating(&RenderFrameHostImpl::CreateQuicTransportConnector,
                          base::Unretained(host)));

  map->Add<blink::test::mojom::VirtualAuthenticatorManager>(
      base::BindRepeating(&RenderFrameHostImpl::GetVirtualAuthenticatorManager,
                          base::Unretained(host)));

  // BrowserMainLoop::GetInstance() may be null on unit tests.
  if (BrowserMainLoop::GetInstance()) {
    // BrowserMainLoop, which owns MediaStreamManager, is alive for the lifetime
    // of Mojo communication (see BrowserMainLoop::ShutdownThreadsAndCleanUp(),
    // which shuts down Mojo). Hence, passing that MediaStreamManager instance
    // as a raw pointer here is safe.
    MediaStreamManager* media_stream_manager =
        BrowserMainLoop::GetInstance()->media_stream_manager();

    map->Add<blink::mojom::MediaDevicesDispatcherHost>(
        base::BindRepeating(&MediaDevicesDispatcherHost::Create,
                            host->GetProcess()->GetID(), host->GetRoutingID(),
                            base::Unretained(media_stream_manager)),
        base::CreateSingleThreadTaskRunner(BrowserThread::IO));

    map->Add<blink::mojom::MediaStreamDispatcherHost>(
        base::BindRepeating(&MediaStreamDispatcherHost::Create,
                            host->GetProcess()->GetID(), host->GetRoutingID(),
                            base::Unretained(media_stream_manager)),
        base::CreateSingleThreadTaskRunner(BrowserThread::IO));
  }

  map->Add<media::mojom::ImageCapture>(
      base::BindRepeating(&ImageCaptureImpl::Create));

  map->Add<media::mojom::VideoDecodePerfHistory>(
      base::BindRepeating(&RenderProcessHost::BindVideoDecodePerfHistory,
                          base::Unretained(host->GetProcess())));

  map->Add<shape_detection::mojom::BarcodeDetectionProvider>(
      base::BindRepeating(&BindBarcodeDetectionProvider));

  map->Add<shape_detection::mojom::FaceDetectionProvider>(
      base::BindRepeating(&BindFaceDetectionProvider));

  map->Add<shape_detection::mojom::TextDetection>(
      base::BindRepeating(&BindTextDetection));

#if defined(OS_ANDROID)
  if (base::FeatureList::IsEnabled(features::kWebNfc)) {
    map->Add<device::mojom::NFC>(base::BindRepeating(
        &RenderFrameHostImpl::BindNFCReceiver, base::Unretained(host)));
  }
#else
  map->Add<blink::mojom::HidService>(base::BindRepeating(
      &RenderFrameHostImpl::GetHidService, base::Unretained(host)));

  // The default (no-op) implementation of InstalledAppProvider. On Android, the
  // real implementation is provided in Java.
  map->Add<blink::mojom::InstalledAppProvider>(
      base::BindRepeating(&InstalledAppProviderImplDefault::Create));

  map->Add<blink::mojom::SerialService>(base::BindRepeating(
      &RenderFrameHostImpl::BindSerialService, base::Unretained(host)));
#endif  // !defined(OS_ANDROID)
}

void PopulateBinderMapWithContext(
    RenderFrameHostImpl* host,
    service_manager::BinderMapWithContext<RenderFrameHost*>* map) {
  map->Add<blink::mojom::BackgroundFetchService>(
      base::BindRepeating(&BackgroundFetchServiceImpl::CreateForFrame));
  map->Add<blink::mojom::ColorChooserFactory>(
      base::BindRepeating(&BindColorChooserFactoryForFrame));
  map->Add<blink::mojom::CookieStore>(
      base::BindRepeating(&CookieStoreContext::CreateServiceForFrame));
  map->Add<blink::mojom::ContentIndexService>(
      base::BindRepeating(&ContentIndexServiceImpl::CreateForFrame));
  map->Add<blink::mojom::KeyboardLockService>(
      base::BindRepeating(&KeyboardLockServiceImpl::CreateMojoService));
  map->Add<blink::mojom::MediaSessionService>(
      base::BindRepeating(&MediaSessionServiceImpl::Create));
  map->Add<blink::mojom::PictureInPictureService>(
      base::BindRepeating(&PictureInPictureServiceImpl::Create));
  map->Add<blink::mojom::WakeLockService>(
      base::BindRepeating(&WakeLockServiceImpl::Create));
  map->Add<device::mojom::VRService>(
      base::BindRepeating(&WebvrServiceProvider::BindWebvrService));
  map->Add<::mojom::ProcessInternalsHandler>(
      base::BindRepeating(&BindProcessInternalsHandler));
#if defined(OS_ANDROID)
  map->Add<blink::mojom::DateTimeChooser>(
      base::BindRepeating(&BindDateTimeChooserForFrame));
  map->Add<blink::mojom::TextSuggestionHost>(
      base::BindRepeating(&BindTextSuggestionHostForFrame));
#endif  // defined(OS_ANDROID)

  GetContentClient()->browser()->RegisterBrowserInterfaceBindersForFrame(map);
}

void PopulateBinderMap(RenderFrameHostImpl* host,
                       service_manager::BinderMap* map) {
  PopulateFrameBinders(host, map);
}

RenderFrameHost* GetContextForHost(RenderFrameHostImpl* host) {
  return host;
}

// Dedicated workers
const url::Origin& GetContextForHost(DedicatedWorkerHost* host) {
  return host->GetOrigin();
}

void PopulateDedicatedWorkerBinders(DedicatedWorkerHost* host,
                                    service_manager::BinderMap* map) {
  // static binders
  map->Add<blink::mojom::ScreenEnumeration>(
      base::BindRepeating(&ScreenEnumerationImpl::Create));
  map->Add<shape_detection::mojom::BarcodeDetectionProvider>(
      base::BindRepeating(&BindBarcodeDetectionProvider));
  map->Add<shape_detection::mojom::FaceDetectionProvider>(
      base::BindRepeating(&BindFaceDetectionProvider));
  map->Add<shape_detection::mojom::TextDetection>(
      base::BindRepeating(&BindTextDetection));

  // worker host binders
  // base::Unretained(host) is safe because the map is owned by
  // |DedicatedWorkerHost::broker_|.
  map->Add<blink::mojom::IdleManager>(base::BindRepeating(
      &DedicatedWorkerHost::CreateIdleManager, base::Unretained(host)));
  if (base::FeatureList::IsEnabled(features::kSmsReceiver)) {
    map->Add<blink::mojom::SmsReceiver>(base::BindRepeating(
        &DedicatedWorkerHost::BindSmsReceiverReceiver, base::Unretained(host)));
  }
  map->Add<blink::mojom::WebUsbService>(base::BindRepeating(
      &DedicatedWorkerHost::CreateWebUsbService, base::Unretained(host)));
  map->Add<blink::mojom::QuicTransportConnector>(
      base::BindRepeating(&DedicatedWorkerHost::CreateQuicTransportConnector,
                          base::Unretained(host)));
#if !defined(OS_ANDROID)
  map->Add<blink::mojom::SerialService>(base::BindRepeating(
      &DedicatedWorkerHost::BindSerialService, base::Unretained(host)));
#endif  // !defined(OS_ANDROID)

  // render process host binders
  map->Add<media::mojom::VideoDecodePerfHistory>(BindDedicatedWorkerReceiver(
      &RenderProcessHost::BindVideoDecodePerfHistory, host));

  // TODO(crbug.com/718652): find a way to pass other parameters to
  // Bind*WorkerReceiver.
  map->Add<blink::mojom::IDBFactory>(base::BindRepeating(
      &DedicatedWorkerHost::CreateIDBFactory, base::Unretained(host)));
}

void PopulateBinderMapWithContext(
    DedicatedWorkerHost* host,
    service_manager::BinderMapWithContext<const url::Origin&>* map) {
  map->Add<payments::mojom::PaymentManager>(
      BindDedicatedWorkerReceiverForOrigin(
          &RenderProcessHost::CreatePaymentManagerForOrigin, host));
  map->Add<blink::mojom::PermissionService>(
      BindDedicatedWorkerReceiverForOrigin(
          &RenderProcessHost::CreatePermissionService, host));
  map->Add<blink::mojom::FileSystemManager>(
      BindDedicatedWorkerReceiverForOrigin(
          &RenderProcessHost::BindFileSystemManager, host));

  // TODO(crbug.com/718652): convert this to BindDedicatedWorkerReceiver*.
  map->Add<blink::mojom::LockManager>(base::BindRepeating(
      &RenderProcessHost::CreateLockManager,
      base::Unretained(host->GetProcessHost()), MSG_ROUTING_NONE));
}

void PopulateBinderMap(DedicatedWorkerHost* host,
                       service_manager::BinderMap* map) {
  PopulateDedicatedWorkerBinders(host, map);
}

// Shared workers
url::Origin GetContextForHost(SharedWorkerHost* host) {
  return url::Origin::Create(host->instance().url());
}

void PopulateSharedWorkerBinders(SharedWorkerHost* host,
                                 service_manager::BinderMap* map) {
  // static binders
  map->Add<blink::mojom::ScreenEnumeration>(
      base::BindRepeating(&ScreenEnumerationImpl::Create));
  map->Add<shape_detection::mojom::BarcodeDetectionProvider>(
      base::BindRepeating(&BindBarcodeDetectionProvider));
  map->Add<shape_detection::mojom::FaceDetectionProvider>(
      base::BindRepeating(&BindFaceDetectionProvider));
  map->Add<shape_detection::mojom::TextDetection>(
      base::BindRepeating(&BindTextDetection));

  // worker host binders
  // base::Unretained(host) is safe because the map is owned by
  // |SharedWorkerHost::broker_|.
  map->Add<blink::mojom::AppCacheBackend>(base::BindRepeating(
      &SharedWorkerHost::CreateAppCacheBackend, base::Unretained(host)));

  // render process host binders
  map->Add<media::mojom::VideoDecodePerfHistory>(BindSharedWorkerReceiver(
      &RenderProcessHost::BindVideoDecodePerfHistory, host));

  // TODO(crbug.com/718652): find a way to pass other parameters to
  // Bind*WorkerReceiver.
  map->Add<blink::mojom::IDBFactory>(base::BindRepeating(
      &SharedWorkerHost::CreateIDBFactory, base::Unretained(host)));
  map->Add<blink::mojom::QuicTransportConnector>(base::BindRepeating(
      &SharedWorkerHost::CreateQuicTransportConnector, base::Unretained(host)));
}

void PopulateBinderMapWithContext(
    SharedWorkerHost* host,
    service_manager::BinderMapWithContext<const url::Origin&>* map) {
  map->Add<blink::mojom::FileSystemManager>(BindSharedWorkerReceiverForOrigin(
      &RenderProcessHost::BindFileSystemManager, host));
  map->Add<payments::mojom::PaymentManager>(BindSharedWorkerReceiverForOrigin(
      &RenderProcessHost::CreatePaymentManagerForOrigin, host));
  map->Add<blink::mojom::PermissionService>(BindSharedWorkerReceiverForOrigin(
      &RenderProcessHost::CreatePermissionService, host));

  // TODO(crbug.com/718652): convert this to BindSharedWorkerReceiver*.
  map->Add<blink::mojom::LockManager>(base::BindRepeating(
      &RenderProcessHost::CreateLockManager,
      base::Unretained(host->GetProcessHost()), MSG_ROUTING_NONE));
}

void PopulateBinderMap(SharedWorkerHost* host,
                       service_manager::BinderMap* map) {
  PopulateSharedWorkerBinders(host, map);
}

// Service workers
ServiceWorkerVersionInfo GetContextForHost(ServiceWorkerProviderHost* host) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());

  return host->running_hosted_version()->GetInfo();
}

void PopulateServiceWorkerBinders(ServiceWorkerProviderHost* host,
                                  service_manager::BinderMap* map) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());

  // static binders
  map->Add<blink::mojom::ScreenEnumeration>(
      base::BindRepeating(&ScreenEnumerationImpl::Create));
  map->Add<shape_detection::mojom::BarcodeDetectionProvider>(
      base::BindRepeating(&BindBarcodeDetectionProvider));
  map->Add<shape_detection::mojom::FaceDetectionProvider>(
      base::BindRepeating(&BindFaceDetectionProvider));
  map->Add<shape_detection::mojom::TextDetection>(
      base::BindRepeating(&BindTextDetection));

  // worker host binders
  map->Add<blink::mojom::QuicTransportConnector>(base::BindRepeating(
      &ServiceWorkerProviderHost::CreateQuicTransportConnector,
      base::Unretained(host)));

  // render process host binders
  map->Add<media::mojom::VideoDecodePerfHistory>(BindServiceWorkerReceiver(
      &RenderProcessHost::BindVideoDecodePerfHistory, host));

  // TODO(crbug.com/718652): convert this to BindServiceWorkerReceiver*.
  map->Add<blink::mojom::IDBFactory>(base::BindRepeating(
      &ServiceWorkerProviderHost::CreateIDBFactory, base::Unretained(host)));
  map->Add<blink::mojom::LockManager>(base::BindRepeating(
      &ServiceWorkerProviderHost::CreateLockManager, base::Unretained(host)));
}

void PopulateBinderMapWithContext(
    ServiceWorkerProviderHost* host,
    service_manager::BinderMapWithContext<const ServiceWorkerVersionInfo&>*
        map) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());

  map->Add<payments::mojom::PaymentManager>(BindServiceWorkerReceiverForOrigin(
      &RenderProcessHost::CreatePaymentManagerForOrigin, host));
  map->Add<blink::mojom::PermissionService>(BindServiceWorkerReceiverForOrigin(
      &RenderProcessHost::CreatePermissionService, host));

  // Use a task runner if ServiceWorkerProviderHost lives on the IO
  // thread, as CreateForWorker() needs to be called on the UI thread.
  if (ServiceWorkerContext::IsServiceWorkerOnUIEnabled()) {
    map->Add<blink::mojom::BackgroundFetchService>(
        base::BindRepeating(&BackgroundFetchServiceImpl::CreateForWorker));
    map->Add<blink::mojom::ContentIndexService>(
        base::BindRepeating(&ContentIndexServiceImpl::CreateForWorker));
    map->Add<blink::mojom::CookieStore>(
        base::BindRepeating(&CookieStoreContext::CreateServiceForWorker));
  } else {
    map->Add<blink::mojom::BackgroundFetchService>(
        base::BindRepeating(&BackgroundFetchServiceImpl::CreateForWorker),
        base::CreateSingleThreadTaskRunner(BrowserThread::UI));
    map->Add<blink::mojom::ContentIndexService>(
        base::BindRepeating(&ContentIndexServiceImpl::CreateForWorker),
        base::CreateSingleThreadTaskRunner(BrowserThread::UI));
    map->Add<blink::mojom::CookieStore>(
        base::BindRepeating(&CookieStoreContext::CreateServiceForWorker),
        base::CreateSingleThreadTaskRunner(BrowserThread::UI));
  }
}

void PopulateBinderMap(ServiceWorkerProviderHost* host,
                       service_manager::BinderMap* map) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  PopulateServiceWorkerBinders(host, map);
}

}  // namespace internal
}  // namespace content
