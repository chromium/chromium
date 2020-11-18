// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/modules_initializer.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/binder_map.h"
#include "third_party/blink/public/mojom/dom_storage/session_storage_namespace.mojom-blink.h"
#include "third_party/blink/public/platform/interface_registry.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_local_frame_client.h"
#include "third_party/blink/public/web/web_view_client.h"
#include "third_party/blink/renderer/bindings/modules/v8/module_bindings_initializer.h"
#include "third_party/blink/renderer/core/css/background_color_paint_image_generator.h"
#include "third_party/blink/renderer/core/css/css_paint_image_generator.h"
#include "third_party/blink/renderer/core/dom/context_features_client_impl.h"
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
#include "third_party/blink/renderer/modules/accessibility/ax_object_cache_impl.h"
#include "third_party/blink/renderer/modules/accessibility/inspector_accessibility_agent.h"
#include "third_party/blink/renderer/modules/app_banner/app_banner_controller.h"
#include "third_party/blink/renderer/modules/audio_output_devices/html_media_element_audio_output_device.h"
#include "third_party/blink/renderer/modules/cache_storage/inspector_cache_storage_agent.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_rendering_context_2d.h"
#include "third_party/blink/renderer/modules/canvas/imagebitmap/image_bitmap_rendering_context.h"
#include "third_party/blink/renderer/modules/canvas/offscreencanvas2d/offscreen_canvas_rendering_context_2d.h"
#include "third_party/blink/renderer/modules/csspaint/background_color_paint_image_generator_impl.h"
#include "third_party/blink/renderer/modules/csspaint/css_paint_image_generator_impl.h"
#include "third_party/blink/renderer/modules/device_orientation/device_motion_controller.h"
#include "third_party/blink/renderer/modules/device_orientation/device_orientation_absolute_controller.h"
#include "third_party/blink/renderer/modules/device_orientation/device_orientation_controller.h"
#include "third_party/blink/renderer/modules/device_orientation/device_orientation_inspector_agent.h"
#include "third_party/blink/renderer/modules/document_metadata/document_metadata_server.h"
#include "third_party/blink/renderer/modules/encryptedmedia/html_media_element_encrypted_media.h"
#include "third_party/blink/renderer/modules/encryptedmedia/media_keys_controller.h"
#include "third_party/blink/renderer/modules/event_interface_modules_names.h"
#include "third_party/blink/renderer/modules/event_modules_factory.h"
#include "third_party/blink/renderer/modules/event_target_modules_names.h"
#include "third_party/blink/renderer/modules/exported/web_embedded_worker_impl.h"
#include "third_party/blink/renderer/modules/filesystem/dragged_isolated_file_system_impl.h"
#include "third_party/blink/renderer/modules/gamepad/navigator_gamepad.h"
#include "third_party/blink/renderer/modules/image_downloader/image_downloader_impl.h"
#include "third_party/blink/renderer/modules/indexed_db_names.h"
#include "third_party/blink/renderer/modules/indexeddb/inspector_indexed_db_agent.h"
#include "third_party/blink/renderer/modules/installation/installation_service_impl.h"
#include "third_party/blink/renderer/modules/launch/file_handling_expiry_impl.h"
#include "third_party/blink/renderer/modules/launch/web_launch_service_impl.h"
#include "third_party/blink/renderer/modules/manifest/manifest_manager.h"
#include "third_party/blink/renderer/modules/media/audio/audio_renderer_sink_cache.h"
#include "third_party/blink/renderer/modules/media_controls/media_controls_impl.h"
#include "third_party/blink/renderer/modules/mediasource/media_source_registry_impl.h"
#include "third_party/blink/renderer/modules/mediastream/user_media_client.h"
#include "third_party/blink/renderer/modules/mediastream/user_media_controller.h"
#include "third_party/blink/renderer/modules/peerconnection/peer_connection_tracker.h"
#include "third_party/blink/renderer/modules/picture_in_picture/picture_in_picture_controller_impl.h"
#include "third_party/blink/renderer/modules/presentation/presentation.h"
#include "third_party/blink/renderer/modules/push_messaging/push_messaging_client.h"
#include "third_party/blink/renderer/modules/remoteplayback/html_media_element_remote_playback.h"
#include "third_party/blink/renderer/modules/remoteplayback/remote_playback.h"
#include "third_party/blink/renderer/modules/screen_orientation/screen_orientation_controller.h"
#include "third_party/blink/renderer/modules/service_worker/navigator_service_worker.h"
#include "third_party/blink/renderer/modules/storage/dom_window_storage_controller.h"
#include "third_party/blink/renderer/modules/storage/inspector_dom_storage_agent.h"
#include "third_party/blink/renderer/modules/storage/storage_namespace.h"
#include "third_party/blink/renderer/modules/webaudio/audio_graph_tracer.h"
#include "third_party/blink/renderer/modules/webaudio/inspector_web_audio_agent.h"
#include "third_party/blink/renderer/modules/webdatabase/database_client.h"
#include "third_party/blink/renderer/modules/webdatabase/database_manager.h"
#include "third_party/blink/renderer/modules/webdatabase/inspector_database_agent.h"
#include "third_party/blink/renderer/modules/webdatabase/web_database_host.h"
#include "third_party/blink/renderer/modules/webdatabase/web_database_impl.h"
#include "third_party/blink/renderer/modules/webgl/webgl2_rendering_context.h"
#include "third_party/blink/renderer/modules/webgl/webgl_rendering_context.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_canvas_context.h"
#include "third_party/blink/renderer/modules/worklet/animation_and_paint_worklet_thread.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/mojo/mojo_helper.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

#if defined(OS_ANDROID)
#include "third_party/blink/renderer/modules/remote_objects/remote_object_gateway_impl.h"
#endif

namespace blink {

void ModulesInitializer::Initialize() {
  // Strings must be initialized before calling CoreInitializer::init().
  const unsigned kModulesStaticStringsCount =
      event_interface_names::kModulesNamesCount +
      event_target_names::kModulesNamesCount + indexed_db_names::kNamesCount;
  StringImpl::ReserveStaticStringsCapacityForSize(kModulesStaticStringsCount);

  event_interface_names::InitModules();
  event_target_names::InitModules();
  Document::RegisterEventFactory(EventModulesFactory::Create());
  ModuleBindingsInitializer::Init();
  indexed_db_names::Init();
  AXObjectCache::Init(AXObjectCacheImpl::Create);
  DraggedIsolatedFileSystem::Init(
      DraggedIsolatedFileSystemImpl::PrepareForDataObject);
  CSSPaintImageGenerator::Init(CSSPaintImageGeneratorImpl::Create);
  BackgroundColorPaintImageGenerator::Init(
      BackgroundColorPaintImageGeneratorImpl::Create);
  WebDatabaseHost::GetInstance().Init();
  MediaSourceRegistryImpl::Init();

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
}

void ModulesInitializer::InitLocalFrame(LocalFrame& frame) const {
  if (frame.IsMainFrame()) {
    frame.GetInterfaceRegistry()->AddInterface(WTF::BindRepeating(
        &DocumentMetadataServer::BindMojoReceiver, WrapWeakPersistent(&frame)));
  }
  frame.GetInterfaceRegistry()->AddAssociatedInterface(WTF::BindRepeating(
      &WebLaunchServiceImpl::Create, WrapWeakPersistent(&frame)));
  frame.GetInterfaceRegistry()->AddAssociatedInterface(WTF::BindRepeating(
      &FileHandlingExpiryImpl::Create, WrapWeakPersistent(&frame)));

  frame.GetInterfaceRegistry()->AddInterface(WTF::BindRepeating(
      &InstallationServiceImpl::Create, WrapWeakPersistent(&frame)));
  // TODO(dominickn): This interface should be document-scoped rather than
  // frame-scoped, as the resulting banner event is dispatched to
  // frame()->document().
  frame.GetInterfaceRegistry()->AddInterface(WTF::BindRepeating(
      &AppBannerController::BindMojoRequest, WrapWeakPersistent(&frame)));
  frame.GetInterfaceRegistry()->AddInterface(WTF::BindRepeating(
      &TextSuggestionBackendImpl::Create, WrapWeakPersistent(&frame)));
#if defined(OS_ANDROID)
  frame.GetInterfaceRegistry()->AddInterface(WTF::BindRepeating(
      &RemoteObjectGatewayFactoryImpl::Create, WrapWeakPersistent(&frame)));
#endif  // OS_ANDROID
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
  session->Append(MakeGarbageCollected<InspectorIndexedDBAgent>(
      inspected_frames, session->V8Session()));
  session->Append(
      MakeGarbageCollected<DeviceOrientationInspectorAgent>(inspected_frames));
  session->Append(
      MakeGarbageCollected<InspectorDOMStorageAgent>(inspected_frames));
  session->Append(MakeGarbageCollected<InspectorAccessibilityAgent>(
      inspected_frames, dom_agent));
  session->Append(MakeGarbageCollected<InspectorWebAudioAgent>(page));
  if (allow_view_agents) {
    session->Append(MakeGarbageCollected<InspectorDatabaseAgent>(page));
    session->Append(
        MakeGarbageCollected<InspectorCacheStorageAgent>(inspected_frames));
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

#if defined(OS_ANDROID)
  LocalFrame* frame = window.GetFrame();
  DCHECK(frame);
  if (auto* gateway = RemoteObjectGatewayImpl::From(*frame))
    gateway->OnClearWindowObjectInMainWorld();
#endif  // OS_ANDROID
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
  return base::WrapUnique(web_frame_client->CreateMediaPlayer(
      source, media_player_client, context_impl, &encrypted_media,
      encrypted_media.ContentDecryptionModule(), sink_id));
}

WebRemotePlaybackClient* ModulesInitializer::CreateWebRemotePlaybackClient(
    HTMLMediaElement& html_media_element) const {
  return &RemotePlayback::From(html_media_element);
}

void ModulesInitializer::ProvideModulesToPage(Page& page,
                                              WebViewClient* client) const {
  MediaKeysController::ProvideMediaKeysTo(page);
  ::blink::ProvideContextFeaturesTo(
      page, std::make_unique<ContextFeaturesClientImpl>());
  ::blink::ProvideDatabaseClientTo(page,
                                   MakeGarbageCollected<DatabaseClient>());
  StorageNamespace::ProvideSessionStorageNamespaceTo(page, client);
  AudioGraphTracer::ProvideAudioGraphTracerTo(page);
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

void ModulesInitializer::DidChangeManifest(LocalFrame& frame) {
  ManifestManager::From(*frame.DomWindow())->DidChangeManifest();
}

void ModulesInitializer::NotifyOrientationChanged(LocalFrame& frame) {
  ScreenOrientationController::From(*frame.DomWindow())
      ->NotifyOrientationChanged();
}

void ModulesInitializer::RegisterInterfaces(mojo::BinderMap& binders) {
  DCHECK(Platform::Current());
  binders.Add(ConvertToBaseRepeatingCallback(
                  CrossThreadBindRepeating(&WebDatabaseImpl::Create)),
              Platform::Current()->GetIOTaskRunner());
  binders.Add(
      ConvertToBaseRepeatingCallback(CrossThreadBindRepeating(
          &PeerConnectionTracker::Bind,
          WTF::CrossThreadUnretained(PeerConnectionTracker::GetInstance()))),
      Thread::MainThread()->GetTaskRunner());
}

}  // namespace blink
