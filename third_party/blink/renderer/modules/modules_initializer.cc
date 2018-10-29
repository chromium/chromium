// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/modules_initializer.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "third_party/blink/public/platform/interface_registry.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/web/web_view_client.h"
#include "third_party/blink/renderer/bindings/modules/v8/module_bindings_initializer.h"
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
#include "third_party/blink/renderer/core/inspector/inspector_session.h"
#include "third_party/blink/renderer/core/offscreencanvas/offscreen_canvas.h"
#include "third_party/blink/renderer/core/origin_trials/origin_trials.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/workers/worker_clients.h"
#include "third_party/blink/renderer/core/workers/worker_content_settings_client.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object_cache_impl.h"
#include "third_party/blink/renderer/modules/accessibility/inspector_accessibility_agent.h"
#include "third_party/blink/renderer/modules/app_banner/app_banner_controller.h"
#include "third_party/blink/renderer/modules/audio_output_devices/html_media_element_audio_output_device.h"
#include "third_party/blink/renderer/modules/cache_storage/inspector_cache_storage_agent.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_rendering_context_2d.h"
#include "third_party/blink/renderer/modules/canvas/imagebitmap/image_bitmap_rendering_context.h"
#include "third_party/blink/renderer/modules/canvas/offscreencanvas2d/offscreen_canvas_rendering_context_2d.h"
#include "third_party/blink/renderer/modules/csspaint/css_paint_image_generator_impl.h"
#include "third_party/blink/renderer/modules/device_orientation/device_motion_controller.h"
#include "third_party/blink/renderer/modules/device_orientation/device_orientation_absolute_controller.h"
#include "third_party/blink/renderer/modules/device_orientation/device_orientation_controller.h"
#include "third_party/blink/renderer/modules/device_orientation/device_orientation_inspector_agent.h"
#include "third_party/blink/renderer/modules/document_metadata/copyless_paste_server.h"
#include "third_party/blink/renderer/modules/encryptedmedia/html_media_element_encrypted_media.h"
#include "third_party/blink/renderer/modules/encryptedmedia/media_keys_controller.h"
#include "third_party/blink/renderer/modules/event_modules_factory.h"
#include "third_party/blink/renderer/modules/event_modules_names.h"
#include "third_party/blink/renderer/modules/event_target_modules_names.h"
#include "third_party/blink/renderer/modules/exported/web_embedded_worker_impl.h"
#include "third_party/blink/renderer/modules/filesystem/dragged_isolated_file_system_impl.h"
#include "third_party/blink/renderer/modules/filesystem/local_file_system_client.h"
#include "third_party/blink/renderer/modules/gamepad/navigator_gamepad.h"
#include "third_party/blink/renderer/modules/indexed_db_names.h"
#include "third_party/blink/renderer/modules/indexeddb/indexed_db_client.h"
#include "third_party/blink/renderer/modules/indexeddb/inspector_indexed_db_agent.h"
#include "third_party/blink/renderer/modules/installation/installation_service_impl.h"
#include "third_party/blink/renderer/modules/installedapp/installed_app_controller.h"
#include "third_party/blink/renderer/modules/media_controls/media_controls_impl.h"
#include "third_party/blink/renderer/modules/mediastream/user_media_client.h"
#include "third_party/blink/renderer/modules/mediastream/user_media_controller.h"
#include "third_party/blink/renderer/modules/navigatorcontentutils/navigator_content_utils.h"
#include "third_party/blink/renderer/modules/navigatorcontentutils/navigator_content_utils_client.h"
#include "third_party/blink/renderer/modules/picture_in_picture/picture_in_picture_controller_impl.h"
#include "third_party/blink/renderer/modules/presentation/presentation_controller.h"
#include "third_party/blink/renderer/modules/presentation/presentation_receiver.h"
#include "third_party/blink/renderer/modules/push_messaging/push_controller.h"
#include "third_party/blink/renderer/modules/remoteplayback/html_media_element_remote_playback.h"
#include "third_party/blink/renderer/modules/remoteplayback/remote_playback.h"
#include "third_party/blink/renderer/modules/screen_orientation/screen_orientation_controller_impl.h"
#include "third_party/blink/renderer/modules/service_worker/navigator_service_worker.h"
#include "third_party/blink/renderer/modules/speech/speech_recognition_controller.h"
#include "third_party/blink/renderer/modules/storage/dom_window_storage_controller.h"
#include "third_party/blink/renderer/modules/storage/inspector_dom_storage_agent.h"
#include "third_party/blink/renderer/modules/storage/storage_namespace.h"
#include "third_party/blink/renderer/modules/time_zone_monitor/time_zone_monitor_client.h"
#include "third_party/blink/renderer/modules/vr/navigator_vr.h"
#include "third_party/blink/renderer/modules/vr/vr_controller.h"
#include "third_party/blink/renderer/modules/webdatabase/database_client.h"
#include "third_party/blink/renderer/modules/webdatabase/database_manager.h"
#include "third_party/blink/renderer/modules/webdatabase/inspector_database_agent.h"
#include "third_party/blink/renderer/modules/webdatabase/web_database_impl.h"
#include "third_party/blink/renderer/modules/worklet/animation_and_paint_worklet_thread.h"
#if defined(SUPPORT_WEBGL2_COMPUTE_CONTEXT)
#include "third_party/blink/renderer/modules/webgl/webgl2_compute_rendering_context.h"
#endif
#include "third_party/blink/renderer/modules/accessibility/inspector_accessibility_agent.h"
#include "third_party/blink/renderer/modules/webgl/webgl2_rendering_context.h"
#include "third_party/blink/renderer/modules/webgl/webgl_rendering_context.h"
#include "third_party/blink/renderer/modules/xr/xr_presentation_context.h"
#include "third_party/blink/renderer/platform/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/mojo/mojo_helper.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

void ModulesInitializer::Initialize() {
  // Strings must be initialized before calling CoreInitializer::init().
  const unsigned kModulesStaticStringsCount =
      EventNames::kModulesNamesCount + EventTargetNames::kModulesNamesCount +
      IndexedDBNames::kNamesCount;
  StringImpl::ReserveStaticStringsCapacityForSize(kModulesStaticStringsCount);

  EventNames::initModules();
  EventTargetNames::initModules();
  Document::RegisterEventFactory(EventModulesFactory::Create());
  ModuleBindingsInitializer::Init();
  IndexedDBNames::init();
  AXObjectCache::Init(AXObjectCacheImpl::Create);
  DraggedIsolatedFileSystem::Init(
      DraggedIsolatedFileSystemImpl::PrepareForDataObject);
  CSSPaintImageGenerator::Init(CSSPaintImageGeneratorImpl::Create);
  // Some unit tests may have no message loop ready, so we can't initialize the
  // mojo stuff here. They can initialize those mojo stuff they're interested in
  // later after they got a message loop ready.
  if (CanInitializeMojo()) {
    TimeZoneMonitorClient::Init();
  }

  CoreInitializer::Initialize();

  // Canvas context types must be registered with the HTMLCanvasElement.
  HTMLCanvasElement::RegisterRenderingContextFactory(
      std::make_unique<CanvasRenderingContext2D::Factory>());
  HTMLCanvasElement::RegisterRenderingContextFactory(
      std::make_unique<WebGLRenderingContext::Factory>());
  HTMLCanvasElement::RegisterRenderingContextFactory(
      std::make_unique<WebGL2RenderingContext::Factory>());
#if defined(SUPPORT_WEBGL2_COMPUTE_CONTEXT)
  HTMLCanvasElement::RegisterRenderingContextFactory(
      std::make_unique<WebGL2ComputeRenderingContext::Factory>());
#endif
  HTMLCanvasElement::RegisterRenderingContextFactory(
      std::make_unique<ImageBitmapRenderingContext::Factory>());
  HTMLCanvasElement::RegisterRenderingContextFactory(
      std::make_unique<XRPresentationContext::Factory>());

  // OffscreenCanvas context types must be registered with the OffscreenCanvas.
  OffscreenCanvas::RegisterRenderingContextFactory(
      std::make_unique<OffscreenCanvasRenderingContext2D::Factory>());
  OffscreenCanvas::RegisterRenderingContextFactory(
      std::make_unique<WebGLRenderingContext::Factory>());
  OffscreenCanvas::RegisterRenderingContextFactory(
      std::make_unique<WebGL2RenderingContext::Factory>());
#if defined(SUPPORT_WEBGL2_COMPUTE_CONTEXT)
  OffscreenCanvas::RegisterRenderingContextFactory(
      std::make_unique<WebGL2ComputeRenderingContext::Factory>());
#endif
}

void ModulesInitializer::InitLocalFrame(LocalFrame& frame) const {
  // CoreInitializer::RegisterLocalFrameInitCallback([](LocalFrame& frame) {
  if (frame.IsMainFrame()) {
    frame.GetInterfaceRegistry()->AddInterface(WTF::BindRepeating(
        &CopylessPasteServer::BindMojoRequest, WrapWeakPersistent(&frame)));
  }
  frame.GetInterfaceRegistry()->AddInterface(WTF::BindRepeating(
      &InstallationServiceImpl::Create, WrapWeakPersistent(&frame)));
  // TODO(dominickn): This interface should be document-scoped rather than
  // frame-scoped, as the resulting banner event is dispatched to
  // frame()->document().
  frame.GetInterfaceRegistry()->AddInterface(WTF::BindRepeating(
      &AppBannerController::BindMojoRequest, WrapWeakPersistent(&frame)));
  frame.GetInterfaceRegistry()->AddInterface(WTF::BindRepeating(
      &TextSuggestionBackendImpl::Create, WrapWeakPersistent(&frame)));
}

void ModulesInitializer::InstallSupplements(LocalFrame& frame) const {
  WebLocalFrameImpl* web_frame = WebLocalFrameImpl::FromFrame(&frame);
  WebLocalFrameClient* client = web_frame->Client();
  DCHECK(client);
  ProvidePushControllerTo(frame, client->PushClient());
  ProvideUserMediaTo(frame, UserMediaClient::Create(client->UserMediaClient()));
  ProvideIndexedDBClientTo(frame, IndexedDBClient::Create(frame));
  ProvideLocalFileSystemTo(frame, LocalFileSystemClient::Create());
  NavigatorContentUtils::ProvideTo(
      *frame.DomWindow()->navigator(),
      NavigatorContentUtilsClient::Create(web_frame));

  ScreenOrientationControllerImpl::ProvideTo(frame);
  if (RuntimeEnabledFeatures::PresentationEnabled())
    PresentationController::ProvideTo(frame);
  InstalledAppController::ProvideTo(frame, client->GetRelatedAppsFetcher());
  ::blink::ProvideSpeechRecognitionTo(frame);
  InspectorAccessibilityAgent::ProvideTo(&frame);
}

void ModulesInitializer::ProvideLocalFileSystemToWorker(
    WorkerClients& worker_clients) const {
  ::blink::ProvideLocalFileSystemToWorker(&worker_clients,
                                          LocalFileSystemClient::Create());
}

void ModulesInitializer::ProvideIndexedDBClientToWorker(
    WorkerClients& worker_clients) const {
  ::blink::ProvideIndexedDBClientToWorker(
      &worker_clients, IndexedDBClient::Create(worker_clients));
}

MediaControls* ModulesInitializer::CreateMediaControls(
    HTMLMediaElement& media_element,
    ShadowRoot& shadow_root) const {
  return MediaControlsImpl::Create(media_element, shadow_root);
}

PictureInPictureController*
ModulesInitializer::CreatePictureInPictureController(Document& document) const {
  return PictureInPictureControllerImpl::Create(document);
}

void ModulesInitializer::InitInspectorAgentSession(
    InspectorSession* session,
    bool allow_view_agents,
    InspectorDOMAgent* dom_agent,
    InspectedFrames* inspected_frames,
    Page* page) const {
  session->Append(
      new InspectorIndexedDBAgent(inspected_frames, session->V8Session()));
  session->Append(new DeviceOrientationInspectorAgent(inspected_frames));
  session->Append(new InspectorDOMStorageAgent(inspected_frames));
  if (allow_view_agents) {
    session->Append(InspectorDatabaseAgent::Create(page));
    session->Append(new InspectorAccessibilityAgent(inspected_frames, dom_agent));
    session->Append(InspectorCacheStorageAgent::Create(inspected_frames));
  }
}

void ModulesInitializer::OnClearWindowObjectInMainWorld(
    Document& document,
    const Settings& settings) const {
  DeviceMotionController::From(document);
  DeviceOrientationController::From(document);
  DeviceOrientationAbsoluteController::From(document);
  NavigatorGamepad::From(document);
  NavigatorServiceWorker::From(document);
  DOMWindowStorageController::From(document);
  if (OriginTrials::WebVREnabled(document.GetExecutionContext()))
    NavigatorVR::From(document);
  if (RuntimeEnabledFeatures::PresentationEnabled() &&
      settings.GetPresentationReceiver()) {
    // We eagerly create PresentationReceiver so that the frame creating the
    // presentation can offer a connection to the presentation receiver.
    PresentationReceiver::From(document);
  }
}

std::unique_ptr<WebMediaPlayer> ModulesInitializer::CreateWebMediaPlayer(
    WebLocalFrameClient* web_frame_client,
    HTMLMediaElement& html_media_element,
    const WebMediaPlayerSource& source,
    WebMediaPlayerClient* media_player_client,
    WebLayerTreeView* view) const {
  HTMLMediaElementEncryptedMedia& encrypted_media =
      HTMLMediaElementEncryptedMedia::From(html_media_element);
  WebString sink_id(
      HTMLMediaElementAudioOutputDevice::sinkId(html_media_element));
  return base::WrapUnique(web_frame_client->CreateMediaPlayer(
      source, media_player_client, &encrypted_media,
      encrypted_media.ContentDecryptionModule(), sink_id, view));
}

WebRemotePlaybackClient* ModulesInitializer::CreateWebRemotePlaybackClient(
    HTMLMediaElement& html_media_element) const {
  return HTMLMediaElementRemotePlayback::remote(html_media_element);
}

void ModulesInitializer::ProvideModulesToPage(Page& page,
                                              WebViewClient* client) const {
  MediaKeysController::ProvideMediaKeysTo(page);
  ::blink::ProvideContextFeaturesTo(page, ContextFeaturesClientImpl::Create());
  ::blink::ProvideDatabaseClientTo(page, new DatabaseClient);
  StorageNamespace::ProvideSessionStorageNamespaceTo(page, client);
}

void ModulesInitializer::ForceNextWebGLContextCreationToFail() const {
  WebGLRenderingContext::ForceNextWebGLContextCreationToFail();
}

void ModulesInitializer::CollectAllGarbageForAnimationAndPaintWorklet() const {
  AnimationAndPaintWorkletThread::CollectAllGarbage();
}

void ModulesInitializer::CloneSessionStorage(
    Page* clone_from_page,
    const SessionStorageNamespaceId& clone_to_namespace) {
  StorageNamespace* storage_namespace = StorageNamespace::From(clone_from_page);
  if (storage_namespace)
    storage_namespace->CloneTo(WebString::FromLatin1(clone_to_namespace));
}

void ModulesInitializer::RegisterInterfaces(
    service_manager::BinderRegistry& registry) {
  DCHECK(Platform::Current());
  registry.AddInterface(
      ConvertToBaseCallback(blink::CrossThreadBind(&WebDatabaseImpl::Create)),
      Platform::Current()->GetIOTaskRunner());
}

}  // namespace blink
