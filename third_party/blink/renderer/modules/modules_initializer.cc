// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/modules_initializer.h"

#include <memory>

#include "base/feature_list.h"
#include "base/task/thread_pool.h"
#include "build/android_buildflags.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/binder_map.h"
#include "third_party/blink/public/mojom/dom_storage/session_storage_namespace.mojom-blink.h"
#include "third_party/blink/public/mojom/filesystem/file_system.mojom-blink.h"
#include "third_party/blink/public/platform/interface_registry.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_local_frame_client.h"
#include "third_party/blink/public/web/web_view_client.h"
#include "third_party/blink/renderer/bindings/modules/v8/module_bindings_initializer.h"
#include "third_party/blink/renderer/core/css/background_color_paint_image_generator.h"
#include "third_party/blink/renderer/core/css/clip_path_paint_image_generator.h"
#include "third_party/blink/renderer/core/css/css_paint_image_generator.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/editing/suggestion/text_suggestion_backend_impl.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/exported/web_shared_worker_impl.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/html/canvas/html_canvas_element.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/core/inspector/devtools_session.h"
#include "third_party/blink/renderer/core/inspector/inspector_media_context_impl.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/offscreencanvas/offscreen_canvas.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/scheduler/task_attribution_tracker_impl.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object_cache_impl.h"
#include "third_party/blink/renderer/modules/accessibility/inspector_accessibility_agent.h"
#include "third_party/blink/renderer/modules/app_banner/app_banner_controller.h"
#include "third_party/blink/renderer/modules/audio_output_devices/html_media_element_audio_output_device.h"
#include "third_party/blink/renderer/modules/cache_storage/inspector_cache_storage_agent.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_rendering_context_2d.h"
#include "third_party/blink/renderer/modules/canvas/imagebitmap/image_bitmap_rendering_context.h"
#include "third_party/blink/renderer/modules/canvas/offscreencanvas2d/offscreen_canvas_rendering_context_2d.h"
#include "third_party/blink/renderer/modules/content_extraction/inner_html_agent.h"
#include "third_party/blink/renderer/modules/content_extraction/inner_text_agent.h"
#include "third_party/blink/renderer/modules/csspaint/css_paint_image_generator_impl.h"
#include "third_party/blink/renderer/modules/csspaint/nativepaint/background_color_paint_image_generator_impl.h"
#include "third_party/blink/renderer/modules/csspaint/nativepaint/clip_path_paint_image_generator_impl.h"
#include "third_party/blink/renderer/modules/device_orientation/device_motion_controller.h"
#include "third_party/blink/renderer/modules/device_orientation/device_orientation_absolute_controller.h"
#include "third_party/blink/renderer/modules/device_orientation/device_orientation_controller.h"
#include "third_party/blink/renderer/modules/device_orientation/device_orientation_inspector_agent.h"
#include "third_party/blink/renderer/modules/document_metadata/document_metadata_server.h"
#include "third_party/blink/renderer/modules/document_picture_in_picture/picture_in_picture_controller_impl.h"
#include "third_party/blink/renderer/modules/encryptedmedia/html_media_element_encrypted_media.h"
#include "third_party/blink/renderer/modules/event_interface_modules_names.h"
#include "third_party/blink/renderer/modules/event_modules_factory.h"
#include "third_party/blink/renderer/modules/event_target_modules_names.h"
#include "third_party/blink/renderer/modules/exported/web_embedded_worker_impl.h"
#include "third_party/blink/renderer/modules/file_system_access/bucket_file_system_agent.h"
#include "third_party/blink/renderer/modules/filesystem/dragged_isolated_file_system_impl.h"
#include "third_party/blink/renderer/modules/filesystem/file_system_dispatcher.h"
#include "third_party/blink/renderer/modules/gamepad/navigator_gamepad.h"
#include "third_party/blink/renderer/modules/image_downloader/image_downloader_impl.h"
#include "third_party/blink/renderer/modules/indexed_db_names.h"
#include "third_party/blink/renderer/modules/indexeddb/inspector_indexed_db_agent.h"
#include "third_party/blink/renderer/modules/installation/installation_service_impl.h"
#include "third_party/blink/renderer/modules/launch/web_launch_service_impl.h"
#include "third_party/blink/renderer/modules/manifest/manifest_manager.h"
#include "third_party/blink/renderer/modules/media/audio/audio_renderer_sink_cache.h"
#include "third_party/blink/renderer/modules/media_capabilities_names.h"
#include "third_party/blink/renderer/modules/media_controls/media_controls_impl.h"
#include "third_party/blink/renderer/modules/mediasource/media_source_registry_impl.h"
#include "third_party/blink/renderer/modules/peerconnection/peer_connection_tracker.h"
#include "third_party/blink/renderer/modules/presentation/presentation.h"
#include "third_party/blink/renderer/modules/push_messaging/push_messaging_client.h"
#include "third_party/blink/renderer/modules/remoteplayback/html_media_element_remote_playback.h"
#include "third_party/blink/renderer/modules/remoteplayback/remote_playback.h"
#include "third_party/blink/renderer/modules/screen_details/screen_details.h"
#include "third_party/blink/renderer/modules/screen_details/window_screen_details.h"
#include "third_party/blink/renderer/modules/screen_orientation/screen_orientation_controller.h"
#include "third_party/blink/renderer/modules/service_worker/navigator_service_worker.h"
#include "third_party/blink/renderer/modules/speech/speech_synthesis.h"
#include "third_party/blink/renderer/modules/storage/dom_window_storage.h"
#include "third_party/blink/renderer/modules/storage/dom_window_storage_controller.h"
#include "third_party/blink/renderer/modules/storage/inspector_dom_storage_agent.h"
#include "third_party/blink/renderer/modules/storage/storage_namespace.h"
#include "third_party/blink/renderer/modules/webaudio/audio_graph_tracer.h"
#include "third_party/blink/renderer/modules/webaudio/inspector_web_audio_agent.h"
#include "third_party/blink/renderer/modules/webdatabase/database_client.h"
#include "third_party/blink/renderer/modules/webdatabase/inspector_database_agent.h"
#include "third_party/blink/renderer/modules/webdatabase/web_database_host.h"
#include "third_party/blink/renderer/modules/webdatabase/web_database_impl.h"
#include "third_party/blink/renderer/modules/webgl/webgl2_rendering_context.h"
#include "third_party/blink/renderer/modules/webgl/webgl_rendering_context.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_canvas_context.h"
#include "third_party/blink/renderer/modules/worklet/animation_and_paint_worklet_thread.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/widget/compositing/blink_categorized_worker_pool_delegate.h"
#include "third_party/blink/renderer/platform/widget/frame_widget.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/webrtc_overrides/init_webrtc.h"
#include "ui/accessibility/accessibility_features.h"

#if BUILDFLAG(IS_ANDROID)
#include "third_party/blink/public/platform/modules/video_capture/web_video_capture_impl_manager.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/web/modules/mediastream/web_media_stream_device_observer.h"
#include "third_party/blink/renderer/core/page/page_visibility_observer.h"
#include "third_party/blink/renderer/modules/remote_objects/remote_object_gateway_impl.h"
#endif

namespace blink {
namespace {

// Serves as a kill switch.
BASE_FEATURE(kBlinkEnableInnerTextAgent,
             "BlinkEnableInnerTextAgent",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Serves as a kill switch.
BASE_FEATURE(kBlinkEnableInnerHtmlAgent,
             "BlinkEnableInnerHtmlAgent",
             base::FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_DESKTOP_ANDROID)

class SuspendCaptureObserver : public GarbageCollected<SuspendCaptureObserver>,
                               public Supplement<Page>,
                               public PageVisibilityObserver {
 public:
  static const char kSupplementName[];

  explicit SuspendCaptureObserver(Page& page)
      : Supplement<Page>(page), PageVisibilityObserver(&page) {}

  // PageVisibilityObserver overrides:
  void PageVisibilityChanged() override {
    // TODO(crbug.com/487935): We don't yet suspend video capture devices for
    // OOPIFs.
    WebLocalFrameImpl* frame = WebLocalFrameImpl::FromFrame(
        DynamicTo<LocalFrame>(GetPage()->MainFrame()));
    if (!frame)
      return;
    WebMediaStreamDeviceObserver* media_stream_device_observer =
        frame->Client()->MediaStreamDeviceObserver();
    if (!media_stream_device_observer)
      return;
    // Don't suspend media capture devices if page visibility is
    // PageVisibilityState::kHiddenButPainting (e.g. Picture-in-Picture).
    // TODO(crbug.com/1339252): Add tests.
    bool suspend = (GetPage()->GetVisibilityState() ==
                    mojom::blink::PageVisibilityState::kHidden);
    MediaStreamDevices video_devices =
        media_stream_device_observer->GetNonScreenCaptureDevices();
    Platform::Current()->GetVideoCaptureImplManager()->SuspendDevices(
        video_devices, suspend);
  }

  void Trace(Visitor* visitor) const override {
    Supplement<Page>::Trace(visitor);
    PageVisibilityObserver::Trace(visitor);
  }
};

const char SuspendCaptureObserver::kSupplementName[] = "SuspendCaptureObserver";
#endif  // BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_DESKTOP_ANDROID)

}  // namespace

void ModulesInitializer::Initialize() {
  // Strings must be initialized before calling CoreInitializer::init().
  const unsigned kModulesStaticStringsCount =
      event_interface_names::kModulesNamesCount +
      event_target_names::kModulesNamesCount + indexed_db_names::kNamesCount;
  StringImpl::ReserveStaticStringsCapacityForSize(
      kModulesStaticStringsCount + StringImpl::AllStaticStrings().size());

  event_interface_names::InitModules();
  event_target_names::InitModules();
  Document::RegisterEventFactory(EventModulesFactory::Create());
  ModuleBindingsInitializer::Init();
  indexed_db_names::Init();
  media_capabilities_names::Init();
  AXObjectCache::Init(AXObjectCacheImpl::Create);
  DraggedIsolatedFileSystem::Init(
      DraggedIsolatedFileSystemImpl::PrepareForDataObject);
  CSSPaintImageGenerator::Init(CSSPaintImageGeneratorImpl::Create);
  BackgroundColorPaintImageGenerator::Init(
      BackgroundColorPaintImageGeneratorImpl::Create);
  ClipPathPaintImageGenerator::Init(ClipPathPaintImageGeneratorImpl::Create);
  WebDatabaseHost::GetInstance().Init();
  MediaSourceRegistryImpl::Init();
  if (::features::IsTextBasedAudioDescriptionEnabled())
    SpeechSynthesisBase::Init(SpeechSynthesis::Create);

  CoreInitializer::Initialize();

  // Canvas context types must be registered with the HTMLCanvasElement.
  HTMLCanvasElement::RegisterRenderingContextFactory(
      std::make_unique<CanvasRenderingContext2D::Factory>());
  HTMLCanvasElement::RegisterRenderingContextFactory(
      std::make_unique<WebGLRenderingContext::Factory>());
  HTMLCanvasElement::RegisterRenderingContextFactory(
      std::make_unique<WebGL2RenderingContext::Factory>());
  HTMLCanvasElement::RegisterRenderingContextFactory(
      std::make_unique<ImageBitmapRenderingContext::Factory>());
  HTMLCanvasElement::RegisterRenderingContextFactory(
      std::make_unique<GPUCanvasContext::Factory>());

  // OffscreenCanvas context types must be registered with the OffscreenCanvas.
  OffscreenCanvas::RegisterRenderingContextFactory(
      std::make_unique<OffscreenCanvasRenderingContext2D::Factory>());
  OffscreenCanvas::RegisterRenderingContextFactory(
      std::make_unique<WebGLRenderingContext::Factory>());
  OffscreenCanvas::RegisterRenderingContextFactory(
      std::make_unique<WebGL2RenderingContext::Factory>());
  OffscreenCanvas::RegisterRenderingContextFactory(
      std::make_unique<ImageBitmapRenderingContext::Factory>());
  OffscreenCanvas::RegisterRenderingContextFactory(
      std::make_unique<GPUCanvasContext::Factory>());

  V8PerIsolateData::SetTaskAttributionTrackerFactory(
      &scheduler::TaskAttributionTrackerImpl::Create);

  ::InitializeWebRtcModule();
}

void ModulesInitializer::InitLocalFrame(LocalFrame& frame) const {
  if (frame.IsMainFrame()) {
    frame.GetInterfaceRegistry()->AddInterface(WTF::BindRepeating(
        &DocumentMetadataServer::BindReceiver, WrapWeakPersistent(&frame)));
  }
  frame.GetInterfaceRegistry()->AddAssociatedInterface(WTF::BindRepeating(
      &WebLaunchServiceImpl::BindReceiver, WrapWeakPersistent(&frame)));

  frame.GetInterfaceRegistry()->AddInterface(WTF::BindRepeating(
      &InstallationServiceImpl::BindReceiver, WrapWeakPersistent(&frame)));
  // TODO(dominickn): This interface should be document-scoped rather than
  // frame-scoped, as the resulting banner event is dispatched to
  // frame()->document().
  frame.GetInterfaceRegistry()->AddInterface(WTF::BindRepeating(
      &AppBannerController::BindReceiver, WrapWeakPersistent(&frame)));
  frame.GetInterfaceRegistry()->AddInterface(WTF::BindRepeating(
      &TextSuggestionBackendImpl::Bind, WrapWeakPersistent(&frame)));
#if BUILDFLAG(IS_ANDROID)
  frame.GetInterfaceRegistry()->AddInterface(WTF::BindRepeating(
      &RemoteObjectGatewayFactoryImpl::Bind, WrapWeakPersistent(&frame)));
#endif  // BUILDFLAG(IS_ANDROID)

  frame.GetInterfaceRegistry()->AddInterface(
      WTF::BindRepeating(&PeerConnectionTracker::BindToFrame,
                         WrapCrossThreadWeakPersistent(&frame)));

  if (base::FeatureList::IsEnabled(kBlinkEnableInnerTextAgent)) {
    frame.GetInterfaceRegistry()->AddInterface(WTF::BindRepeating(
        &InnerTextAgent::BindReceiver, WrapWeakPersistent(&frame)));
  }

  if (base::FeatureList::IsEnabled(kBlinkEnableInnerHtmlAgent)) {
    frame.GetInterfaceRegistry()->AddInterface(WTF::BindRepeating(
        &InnerHtmlAgent::BindReceiver, WrapWeakPersistent(&frame)));
  }
}

void ModulesInitializer::InstallSupplements(LocalFrame& frame) const {
  DCHECK(WebLocalFrameImpl::FromFrame(&frame)->Client());
  InspectorAccessibilityAgent::ProvideTo(&frame);
  ImageDownloaderImpl::ProvideTo(frame);
  AudioRendererSinkCache::InstallWindowObserver(*frame.DomWindow());
}

MediaControls* ModulesInitializer::CreateMediaControls(
    HTMLMediaElement& media_element,
    ShadowRoot& shadow_root) const {
  return MediaControlsImpl::Create(media_element, shadow_root);
}

PictureInPictureController*
ModulesInitializer::CreatePictureInPictureController(Document& document) const {
  return MakeGarbageCollected<PictureInPictureControllerImpl>(document);
}

void ModulesInitializer::InitInspectorAgentSession(
    DevToolsSession* session,
    bool allow_view_agents,
    InspectorDOMAgent* dom_agent,
    InspectedFrames* inspected_frames,
    Page* page) const {
  session->CreateAndAppend<InspectorIndexedDBAgent>(inspected_frames,
                                                    session->V8Session());
  session->CreateAndAppend<DeviceOrientationInspectorAgent>(inspected_frames);
  session->CreateAndAppend<InspectorDOMStorageAgent>(inspected_frames);
  session->CreateAndAppend<InspectorAccessibilityAgent>(inspected_frames,
                                                        dom_agent);
  session->CreateAndAppend<InspectorWebAudioAgent>(page);
  session->CreateAndAppend<InspectorCacheStorageAgent>(inspected_frames);
  session->CreateAndAppend<BucketFileSystemAgent>(inspected_frames);
  if (allow_view_agents) {
    session->CreateAndAppend<InspectorDatabaseAgent>(page);
  }
}

void ModulesInitializer::OnClearWindowObjectInMainWorld(
    Document& document,
    const Settings& settings) const {
  LocalDOMWindow& window = *document.domWindow();
  DeviceMotionController::From(window);
  DeviceOrientationController::From(window);
  DeviceOrientationAbsoluteController::From(window);
  NavigatorGamepad::From(*window.navigator());

  // TODO(nhiroki): Figure out why ServiceWorkerContainer needs to be eagerly
  // initialized.
  if (!document.IsInitialEmptyDocument())
    NavigatorServiceWorker::From(window);

  DOMWindowStorageController::From(window);
  if (RuntimeEnabledFeatures::PresentationEnabled() &&
      settings.GetPresentationReceiver()) {
    // We eagerly create Presentation and associated PresentationReceiver so
    // that the frame creating the presentation can offer a connection to the
    // presentation receiver.
    Presentation::presentation(*window.navigator());
  }
  ManifestManager::From(window);

#if BUILDFLAG(IS_ANDROID)
  LocalFrame* frame = window.GetFrame();
  DCHECK(frame);
  if (auto* gateway = RemoteObjectGatewayImpl::From(*frame))
    gateway->OnClearWindowObjectInMainWorld();
#endif  // BUILDFLAG(IS_ANDROID)
}

std::unique_ptr<WebMediaPlayer> ModulesInitializer::CreateWebMediaPlayer(
    WebLocalFrameClient* web_frame_client,
    HTMLMediaElement& html_media_element,
    const WebMediaPlayerSource& source,
    WebMediaPlayerClient* media_player_client) const {
  HTMLMediaElementEncryptedMedia& encrypted_media =
      HTMLMediaElementEncryptedMedia::From(html_media_element);
  WebString sink_id(
      HTMLMediaElementAudioOutputDevice::sinkId(html_media_element));
  MediaInspectorContextImpl* context_impl = MediaInspectorContextImpl::From(
      *To<LocalDOMWindow>(html_media_element.GetExecutionContext()));
  FrameWidget* frame_widget =
      html_media_element.GetDocument().GetFrame()->GetWidgetForLocalRoot();
  return web_frame_client->CreateMediaPlayer(
      source, media_player_client, context_impl, &encrypted_media,
      encrypted_media.ContentDecryptionModule(), sink_id,
      frame_widget->GetLayerTreeSettings(),
      base::ThreadPool::CreateTaskRunner(base::TaskTraits{}));
}

WebRemotePlaybackClient* ModulesInitializer::CreateWebRemotePlaybackClient(
    HTMLMediaElement& html_media_element) const {
  return &RemotePlayback::From(html_media_element);
}

void ModulesInitializer::ProvideModulesToPage(
    Page& page,
    const SessionStorageNamespaceId& namespace_id) const {
  page.ProvideSupplement(MakeGarbageCollected<DatabaseClient>(page));
  StorageNamespace::ProvideSessionStorageNamespaceTo(page, namespace_id);
  AudioGraphTracer::ProvideAudioGraphTracerTo(page);
#if BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_DESKTOP_ANDROID)
  page.ProvideSupplement(MakeGarbageCollected<SuspendCaptureObserver>(page));
#endif  // BUILDFLAG(IS_ANDROID)  && !BUILDFLAG(IS_DESKTOP_ANDROID)
}

void ModulesInitializer::ForceNextWebGLContextCreationToFail() const {
  WebGLRenderingContext::ForceNextWebGLContextCreationToFail();
}

void ModulesInitializer::
    CollectAllGarbageForAnimationAndPaintWorkletForTesting() const {
  AnimationAndPaintWorkletThread::CollectAllGarbageForTesting();
}

void ModulesInitializer::CloneSessionStorage(
    Page* clone_from_page,
    const SessionStorageNamespaceId& clone_to_namespace) {
  StorageNamespace* storage_namespace = StorageNamespace::From(clone_from_page);
  if (storage_namespace)
    storage_namespace->CloneTo(WebString::FromLatin1(clone_to_namespace));
}

void ModulesInitializer::EvictSessionStorageCachedData(Page* page) {
  StorageNamespace* storage_namespace = StorageNamespace::From(page);
  if (storage_namespace)
    storage_namespace->EvictSessionStorageCachedData();
}

void ModulesInitializer::DidChangeManifest(LocalFrame& frame) {
  ManifestManager::From(*frame.DomWindow())->DidChangeManifest();
}

void ModulesInitializer::NotifyOrientationChanged(LocalFrame& frame) {
  ScreenOrientationController::From(*frame.DomWindow())
      ->NotifyOrientationChanged();
}

void ModulesInitializer::DidUpdateScreens(
    LocalFrame& frame,
    const display::ScreenInfos& screen_infos) {
  auto* window = frame.DomWindow();
  if (auto* supplement =
          Supplement<LocalDOMWindow>::From<WindowScreenDetails>(window)) {
    // screen_details() may be null if permission has not been granted.
    if (auto* screen_details = supplement->screen_details()) {
      screen_details->UpdateScreenInfos(window, screen_infos);
    }
  }
}

void ModulesInitializer::SetLocalStorageArea(
    LocalFrame& frame,
    mojo::PendingRemote<mojom::blink::StorageArea> local_storage_area) {
  if (!frame.DomWindow())
    return;
  DOMWindowStorage::From(*frame.DomWindow())
      .InitLocalStorage(std::move(local_storage_area));
}

void ModulesInitializer::SetSessionStorageArea(
    LocalFrame& frame,
    mojo::PendingRemote<mojom::blink::StorageArea> session_storage_area) {
  if (!frame.DomWindow())
    return;
  DOMWindowStorage::From(*frame.DomWindow())
      .InitSessionStorage(std::move(session_storage_area));
}

mojom::blink::FileSystemManager& ModulesInitializer::GetFileSystemManager(
    ExecutionContext* context) {
  return FileSystemDispatcher::From(context).GetFileSystemManager();
}

void ModulesInitializer::RegisterInterfaces(mojo::BinderMap& binders) {
  DCHECK(Platform::Current());
  binders.Add<mojom::blink::WebDatabase>(
      ConvertToBaseRepeatingCallback(
          CrossThreadBindRepeating(&WebDatabaseImpl::Bind)),
      Platform::Current()->GetIOTaskRunner());
}

}  // namespace blink
