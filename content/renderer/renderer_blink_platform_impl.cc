// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/renderer_blink_platform_impl.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/guid.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/shared_memory.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/url_formatter/url_formatter.h"
#include "content/child/child_process.h"
#include "content/child/thread_safe_sender.h"
#include "content/common/frame_messages.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/gpu_stream_constants.h"
#include "content/public/common/service_manager_connection.h"
#include "content/public/common/service_names.mojom.h"
#include "content/public/common/webplugininfo.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/public/renderer/media_stream_utils.h"
#include "content/public/renderer/render_frame.h"
#include "content/renderer/blob_storage/webblobregistry_impl.h"
#include "content/renderer/dom_storage/local_storage_cached_areas.h"
#include "content/renderer/dom_storage/local_storage_namespace.h"
#include "content/renderer/dom_storage/session_web_storage_namespace_impl.h"
#include "content/renderer/dom_storage/webstoragenamespace_impl.h"
#include "content/renderer/image_capture/image_capture_frame_grabber.h"
#include "content/renderer/loader/child_url_loader_factory_bundle.h"
#include "content/renderer/loader/code_cache_loader_impl.h"
#include "content/renderer/loader/resource_dispatcher.h"
#include "content/renderer/loader/web_data_consumer_handle_impl.h"
#include "content/renderer/loader/web_url_loader_impl.h"
#include "content/renderer/media/audio/audio_device_factory.h"
#include "content/renderer/media/audio_decoder.h"
#include "content/renderer/media/midi/renderer_webmidiaccessor_impl.h"
#include "content/renderer/media/renderer_webaudiodevice_impl.h"
#include "content/renderer/media_capture_from_element/canvas_capture_handler.h"
#include "content/renderer/media_capture_from_element/html_audio_element_capturer_source.h"
#include "content/renderer/media_capture_from_element/html_video_element_capturer_source.h"
#include "content/renderer/media_recorder/media_recorder_handler.h"
#include "content/renderer/mojo/blink_interface_provider_impl.h"
#include "content/renderer/p2p/port_allocator.h"
#include "content/renderer/push_messaging/push_provider.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/storage_util.h"
#include "content/renderer/web_database_observer_impl.h"
#include "content/renderer/webgraphicscontext3d_provider_impl.h"
#include "content/renderer/worker_thread_registry.h"
#include "device/gamepad/public/cpp/gamepads.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/config/gpu_driver_bug_workaround_type.h"
#include "gpu/config/gpu_info.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "media/audio/audio_output_device.h"
#include "media/blink/webcontentdecryptionmodule_impl.h"
#include "media/filters/stream_parser_factory.h"
#include "mojo/public/cpp/bindings/strong_associated_binding.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "net/base/features.h"
#include "ppapi/buildflags/buildflags.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/wrapper_shared_url_loader_factory.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "services/ws/public/cpp/gpu/context_provider_command_buffer.h"
#include "storage/common/database/database_identifier.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/origin_trials/trial_token_validator.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom.h"
#include "third_party/blink/public/platform/blame_context.h"
#include "third_party/blink/public/platform/file_path_conversion.h"
#include "third_party/blink/public/platform/modules/webmidi/web_midi_accessor.h"
#include "third_party/blink/public/platform/scheduler/web_thread_scheduler.h"
#include "third_party/blink/public/platform/url_conversion.h"
#include "third_party/blink/public/platform/web_audio_latency_hint.h"
#include "third_party/blink/public/platform/web_blob_registry.h"
#include "third_party/blink/public/platform/web_media_recorder_handler.h"
#include "third_party/blink/public/platform/web_media_stream_center.h"
#include "third_party/blink/public/platform/web_rtc_certificate_generator.h"
#include "third_party/blink/public/platform/web_rtc_peer_connection_handler.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_url_loader_factory.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/sqlite/sqlite3.h"
#include "url/gurl.h"

#if defined(OS_MACOSX)
#include "content/child/child_process_sandbox_support_impl_mac.h"
#include "content/common/mac/font_loader.h"
#include "third_party/blink/public/platform/mac/web_sandbox_support.h"
#endif

#if defined(OS_POSIX)
#include "base/file_descriptor_posix.h"
#if !defined(OS_MACOSX) && !defined(OS_ANDROID)
#include <map>
#include <string>

#include "base/synchronization/lock.h"
#include "content/child/child_process_sandbox_support_impl_linux.h"
#include "third_party/blink/public/platform/linux/out_of_process_font.h"
#include "third_party/blink/public/platform/linux/web_sandbox_support.h"
#include "third_party/icu/source/common/unicode/utf16.h"
#endif
#endif

#include "content/renderer/media/webrtc/peer_connection_dependency_factory.h"
#include "content/renderer/media/webrtc/rtc_certificate_generator.h"
#include "content/renderer/media/webrtc/webrtc_uma_histograms.h"

using blink::Platform;
using blink::WebAudioDevice;
using blink::WebAudioLatencyHint;
using blink::WebBlobRegistry;
using blink::WebCanvasCaptureHandler;
using blink::WebDatabaseObserver;
using blink::WebImageCaptureFrameGrabber;
using blink::WebMediaPlayer;
using blink::WebMediaRecorderHandler;
using blink::WebMediaStream;
using blink::WebMediaStreamCenter;
using blink::WebMediaStreamTrack;
using blink::WebRTCPeerConnectionHandler;
using blink::WebRTCPeerConnectionHandlerClient;
using blink::WebStorageNamespace;
using blink::WebSize;
using blink::WebString;
using blink::WebURL;
using blink::WebVector;

namespace content {

namespace {

bool g_sandbox_enabled = true;

media::AudioParameters GetAudioHardwareParams() {
  blink::WebLocalFrame* const web_frame =
      blink::WebLocalFrame::FrameForCurrentContext();
  RenderFrame* const render_frame = RenderFrame::FromWebFrame(web_frame);
  if (!render_frame)
    return media::AudioParameters::UnavailableDeviceParams();

  return AudioDeviceFactory::GetOutputDeviceInfo(render_frame->GetRoutingID(),
                                                 media::AudioSinkParameters())
      .output_params();
}

gpu::ContextType ToGpuContextType(blink::Platform::ContextType type) {
  switch (type) {
    case blink::Platform::kWebGL1ContextType:
      return gpu::CONTEXT_TYPE_WEBGL1;
    case blink::Platform::kWebGL2ContextType:
      return gpu::CONTEXT_TYPE_WEBGL2;
    case blink::Platform::kWebGL2ComputeContextType:
      return gpu::CONTEXT_TYPE_WEBGL2_COMPUTE;
    case blink::Platform::kGLES2ContextType:
      return gpu::CONTEXT_TYPE_OPENGLES2;
    case blink::Platform::kGLES3ContextType:
      return gpu::CONTEXT_TYPE_OPENGLES3;
    case blink::Platform::kWebGPUContextType:
      return gpu::CONTEXT_TYPE_WEBGPU;
  }
  NOTREACHED();
  return gpu::CONTEXT_TYPE_OPENGLES2;
}

}  // namespace

//------------------------------------------------------------------------------

#if !defined(OS_ANDROID) && !defined(OS_WIN) && !defined(OS_FUCHSIA)
class RendererBlinkPlatformImpl::SandboxSupport
    : public blink::WebSandboxSupport {
 public:
#if defined(OS_LINUX)
  explicit SandboxSupport(sk_sp<font_service::FontLoader> font_loader)
      : font_loader_(std::move(font_loader)) {}
#endif
  ~SandboxSupport() override {}

#if defined(OS_MACOSX)
  bool LoadFont(CTFontRef src_font,
                CGFontRef* container,
                uint32_t* font_id) override;
#elif defined(OS_LINUX)
  void GetFallbackFontForCharacter(
      blink::WebUChar32 character,
      const char* preferred_locale,
      blink::OutOfProcessFont* fallbackFont) override;
  void MatchFontByPostscriptNameOrFullFontName(
      const char* font_unique_name,
      blink::OutOfProcessFont* fallback_font) override;
  void GetWebFontRenderStyleForStrike(const char* family,
                                      int size,
                                      bool is_bold,
                                      bool is_italic,
                                      float device_scale_factor,
                                      blink::WebFontRenderStyle* out) override;

 private:
  // WebKit likes to ask us for the correct font family to use for a set of
  // unicode code points. It needs this information frequently so we cache it
  // here.
  base::Lock unicode_font_families_mutex_;
  std::map<int32_t, blink::OutOfProcessFont> unicode_font_families_;
  sk_sp<font_service::FontLoader> font_loader_;
#endif
};
#endif  // !defined(OS_ANDROID) && !defined(OS_WIN)

//------------------------------------------------------------------------------

RendererBlinkPlatformImpl::RendererBlinkPlatformImpl(
    blink::scheduler::WebThreadScheduler* main_thread_scheduler)
    : BlinkPlatformImpl(main_thread_scheduler->DefaultTaskRunner(),
                        RenderThreadImpl::current()
                            ? RenderThreadImpl::current()->GetIOTaskRunner()
                            : nullptr),
      sudden_termination_disables_(0),
      is_locked_to_site_(false),
      default_task_runner_(main_thread_scheduler->DefaultTaskRunner()),
      main_thread_scheduler_(main_thread_scheduler) {

  // RenderThread may not exist in some tests.
  if (RenderThreadImpl::current()) {
    io_runner_ = RenderThreadImpl::current()->GetIOTaskRunner();
    connector_ = RenderThreadImpl::current()
                     ->GetServiceManagerConnection()
                     ->GetConnector()
                     ->Clone();
    thread_safe_sender_ = RenderThreadImpl::current()->thread_safe_sender();
    blob_registry_.reset(new WebBlobRegistryImpl(thread_safe_sender_.get()));
#if defined(OS_LINUX)
    font_loader_ = sk_make_sp<font_service::FontLoader>(connector_.get());
    SkFontConfigInterface::SetGlobal(font_loader_);
#endif
  } else {
    service_manager::mojom::ConnectorRequest request;
    connector_ = service_manager::Connector::Create(&request);
  }

#if !defined(OS_ANDROID) && !defined(OS_WIN) && !defined(OS_FUCHSIA)
  if (g_sandbox_enabled && sandboxEnabled()) {
#if defined(OS_MACOSX)
    sandbox_support_.reset(new RendererBlinkPlatformImpl::SandboxSupport());
#else
    sandbox_support_.reset(
        new RendererBlinkPlatformImpl::SandboxSupport(font_loader_));
#endif
  } else {
    DVLOG(1) << "Disabling sandbox support for testing.";
  }
#endif

  blink_interface_provider_.reset(
      new BlinkInterfaceProviderImpl(connector_.get()));
  top_level_blame_context_.Initialize();
  main_thread_scheduler_->SetTopLevelBlameContext(&top_level_blame_context_);

  GetInterfaceProvider()->GetInterface(
      mojo::MakeRequest(&web_database_host_info_));
  GetInterfaceProvider()->GetInterface(
      mojo::MakeRequest(&code_cache_host_info_));
}

RendererBlinkPlatformImpl::~RendererBlinkPlatformImpl() {
  main_thread_scheduler_->SetTopLevelBlameContext(nullptr);
}

void RendererBlinkPlatformImpl::Shutdown() {
#if !defined(OS_ANDROID) && !defined(OS_WIN) && !defined(OS_FUCHSIA)
  // SandboxSupport contains a map of OutOfProcessFont objects, which hold
  // WebStrings and WebVectors, which become invalidated when blink is shut
  // down. Hence, we need to clear that map now, just before blink::shutdown()
  // is called.
  sandbox_support_.reset();
#endif
}

//------------------------------------------------------------------------------

std::unique_ptr<blink::WebURLLoaderFactory>
RendererBlinkPlatformImpl::CreateDefaultURLLoaderFactory() {
  if (!RenderThreadImpl::current()) {
    // RenderThreadImpl is null in some tests, the default factory impl
    // takes care of that in the case.
    return std::make_unique<WebURLLoaderFactoryImpl>(nullptr, nullptr);
  }
  return std::make_unique<WebURLLoaderFactoryImpl>(
      RenderThreadImpl::current()->resource_dispatcher()->GetWeakPtr(),
      CreateDefaultURLLoaderFactoryBundle());
}

std::unique_ptr<blink::CodeCacheLoader>
RendererBlinkPlatformImpl::CreateCodeCacheLoader() {
  return std::make_unique<CodeCacheLoaderImpl>();
}

std::unique_ptr<blink::WebURLLoaderFactory>
RendererBlinkPlatformImpl::WrapURLLoaderFactory(
    mojo::ScopedMessagePipeHandle url_loader_factory_handle) {
  return std::make_unique<WebURLLoaderFactoryImpl>(
      RenderThreadImpl::current()->resource_dispatcher()->GetWeakPtr(),
      base::MakeRefCounted<network::WrapperSharedURLLoaderFactory>(
          network::mojom::URLLoaderFactoryPtrInfo(
              std::move(url_loader_factory_handle),
              network::mojom::URLLoaderFactory::Version_)));
}

std::unique_ptr<blink::WebURLLoaderFactory>
RendererBlinkPlatformImpl::WrapSharedURLLoaderFactory(
    scoped_refptr<network::SharedURLLoaderFactory> factory) {
  return std::make_unique<WebURLLoaderFactoryImpl>(
      RenderThreadImpl::current()->resource_dispatcher()->GetWeakPtr(),
      std::move(factory));
}

std::unique_ptr<blink::WebDataConsumerHandle>
RendererBlinkPlatformImpl::CreateDataConsumerHandle(
    mojo::ScopedDataPipeConsumerHandle handle) {
  return std::make_unique<WebDataConsumerHandleImpl>(std::move(handle));
}

scoped_refptr<ChildURLLoaderFactoryBundle>
RendererBlinkPlatformImpl::CreateDefaultURLLoaderFactoryBundle() {
  return base::MakeRefCounted<ChildURLLoaderFactoryBundle>(
      base::BindOnce(&RendererBlinkPlatformImpl::CreateNetworkURLLoaderFactory,
                     base::Unretained(this)));
}

PossiblyAssociatedInterfacePtr<network::mojom::URLLoaderFactory>
RendererBlinkPlatformImpl::CreateNetworkURLLoaderFactory() {
  RenderThreadImpl* render_thread = RenderThreadImpl::current();
  DCHECK(render_thread);
  PossiblyAssociatedInterfacePtr<network::mojom::URLLoaderFactory>
      url_loader_factory;

  if (base::FeatureList::IsEnabled(network::features::kNetworkService)) {
    network::mojom::URLLoaderFactoryPtr factory_ptr;
    connector_->BindInterface(mojom::kBrowserServiceName, &factory_ptr);
    url_loader_factory = std::move(factory_ptr);
  } else {
    network::mojom::URLLoaderFactoryAssociatedPtr factory_ptr;
    render_thread->channel()->GetRemoteAssociatedInterface(&factory_ptr);
    url_loader_factory = std::move(factory_ptr);
  }
  return url_loader_factory;
}

void RendererBlinkPlatformImpl::SetDisplayThreadPriority(
    base::PlatformThreadId thread_id) {
#if defined(OS_LINUX)
  if (RenderThreadImpl* render_thread = RenderThreadImpl::current()) {
    render_thread->render_message_filter()->SetThreadPriority(
        thread_id, base::ThreadPriority::DISPLAY);
  }
#endif
}

blink::BlameContext* RendererBlinkPlatformImpl::GetTopLevelBlameContext() {
  return &top_level_blame_context_;
}

blink::WebSandboxSupport* RendererBlinkPlatformImpl::GetSandboxSupport() {
#if defined(OS_ANDROID) || defined(OS_WIN) || defined(OS_FUCHSIA)
  // These platforms do not require sandbox support.
  return NULL;
#else
  return sandbox_support_.get();
#endif
}

blink::WebCookieJar* RendererBlinkPlatformImpl::CookieJar() {
  NOTREACHED() << "Use WebLocalFrameClient::cookieJar() instead!";
  return nullptr;
}

blink::WebThemeEngine* RendererBlinkPlatformImpl::ThemeEngine() {
  blink::WebThemeEngine* theme_engine =
      GetContentClient()->renderer()->OverrideThemeEngine();
  if (theme_engine)
    return theme_engine;
  return BlinkPlatformImpl::ThemeEngine();
}

bool RendererBlinkPlatformImpl::sandboxEnabled() {
  // As explained in Platform.h, this function is used to decide
  // whether to allow file system operations to come out of WebKit or not.
  // Even if the sandbox is disabled, there's no reason why the code should
  // act any differently...unless we're in single process mode.  In which
  // case, we have no other choice.  Platform.h discourages using
  // this switch unless absolutely necessary, so hopefully we won't end up
  // with too many code paths being different in single-process mode.
  return !base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kSingleProcess);
}

unsigned long long RendererBlinkPlatformImpl::VisitedLinkHash(
    const char* canonical_url,
    size_t length) {
  return GetContentClient()->renderer()->VisitedLinkHash(canonical_url, length);
}

bool RendererBlinkPlatformImpl::IsLinkVisited(unsigned long long link_hash) {
  return GetContentClient()->renderer()->IsLinkVisited(link_hash);
}

blink::WebPrescientNetworking*
RendererBlinkPlatformImpl::PrescientNetworking() {
  return GetContentClient()->renderer()->GetPrescientNetworking();
}

blink::WebString RendererBlinkPlatformImpl::UserAgent() {
  auto* render_thread = RenderThreadImpl::current();
  // RenderThreadImpl is null in some tests.
  if (!render_thread)
    return WebString();
  return render_thread->GetUserAgent();
}

void RendererBlinkPlatformImpl::CacheMetadata(
    blink::mojom::CodeCacheType cache_type,
    const blink::WebURL& url,
    base::Time response_time,
    const char* data,
    size_t size) {
  // Only cache WebAssembly if we have isolated code caches.
  // TODO(bbudge) Remove this check when isolated code caches are on by default.
  if (cache_type == blink::mojom::CodeCacheType::kJavascript ||
      base::FeatureList::IsEnabled(net::features::kIsolatedCodeCache)) {
    // Let the browser know we generated cacheable metadata for this resource.
    // The browser may cache it and return it on subsequent responses to speed
    // the processing of this resource.
    std::vector<uint8_t> copy(data, data + size);
    GetCodeCacheHost().DidGenerateCacheableMetadata(cache_type, url,
                                                    response_time, copy);
  }
}

void RendererBlinkPlatformImpl::FetchCachedCode(
    blink::mojom::CodeCacheType cache_type,
    const GURL& url,
    base::OnceCallback<void(base::Time, const std::vector<uint8_t>&)>
        callback) {
  GetCodeCacheHost().FetchCachedCode(cache_type, url, std::move(callback));
}

void RendererBlinkPlatformImpl::ClearCodeCacheEntry(
    blink::mojom::CodeCacheType cache_type,
    const GURL& url) {
  GetCodeCacheHost().ClearCodeCacheEntry(cache_type, url);
}

void RendererBlinkPlatformImpl::CacheMetadataInCacheStorage(
    const blink::WebURL& url,
    base::Time response_time,
    const char* data,
    size_t size,
    const blink::WebSecurityOrigin& cacheStorageOrigin,
    const blink::WebString& cacheStorageCacheName) {
  // Let the browser know we generated cacheable metadata for this resource in
  // CacheStorage. The browser may cache it and return it on subsequent
  // responses to speed the processing of this resource.
  std::vector<uint8_t> copy(data, data + size);
  GetCodeCacheHost().DidGenerateCacheableMetadataInCacheStorage(
      url, response_time, copy, cacheStorageOrigin,
      cacheStorageCacheName.Utf8());
}

WebString RendererBlinkPlatformImpl::DefaultLocale() {
  return WebString::FromASCII(RenderThread::Get()->GetLocale());
}

void RendererBlinkPlatformImpl::SuddenTerminationChanged(bool enabled) {
  if (enabled) {
    // We should not get more enables than disables, but we want it to be a
    // non-fatal error if it does happen.
    DCHECK_GT(sudden_termination_disables_, 0);
    sudden_termination_disables_ = std::max(sudden_termination_disables_ - 1,
                                            0);
    if (sudden_termination_disables_ != 0)
      return;
  } else {
    sudden_termination_disables_++;
    if (sudden_termination_disables_ != 1)
      return;
  }

  RenderThreadImpl* thread = RenderThreadImpl::current();
  if (thread)  // NULL in unittests.
    thread->GetRendererHost()->SuddenTerminationChanged(enabled);
}

std::unique_ptr<WebStorageNamespace>
RendererBlinkPlatformImpl::CreateLocalStorageNamespace() {
  if (!local_storage_cached_areas_) {
    local_storage_cached_areas_.reset(new LocalStorageCachedAreas(
        RenderThreadImpl::current()->GetStoragePartitionService(),
        main_thread_scheduler_));
  }
  return std::make_unique<LocalStorageNamespace>(
      local_storage_cached_areas_.get());
}

std::unique_ptr<blink::WebStorageNamespace>
RendererBlinkPlatformImpl::CreateSessionStorageNamespace(
    base::StringPiece namespace_id) {
  if (base::FeatureList::IsEnabled(blink::features::kOnionSoupDOMStorage)) {
    if (!local_storage_cached_areas_) {
      local_storage_cached_areas_.reset(new LocalStorageCachedAreas(
          RenderThreadImpl::current()->GetStoragePartitionService(),
          main_thread_scheduler_));
    }
    return std::make_unique<SessionWebStorageNamespaceImpl>(
        namespace_id.as_string(), local_storage_cached_areas_.get());
  }

  return std::make_unique<WebStorageNamespaceImpl>(namespace_id.as_string());
}

void RendererBlinkPlatformImpl::CloneSessionStorageNamespace(
    const std::string& source_namespace,
    const std::string& destination_namespace) {
  if (!local_storage_cached_areas_) {
    // Some browser tests don't have a RenderThreadImpl.
    RenderThreadImpl* render_thread = RenderThreadImpl::current();
    if (!render_thread)
      return;
    local_storage_cached_areas_.reset(new LocalStorageCachedAreas(
        render_thread->GetStoragePartitionService(), main_thread_scheduler_));
  }
  local_storage_cached_areas_->CloneNamespace(source_namespace,
                                              destination_namespace);
}

//------------------------------------------------------------------------------

WebString RendererBlinkPlatformImpl::FileSystemCreateOriginIdentifier(
    const blink::WebSecurityOrigin& origin) {
  return WebString::FromUTF8(
      storage::GetIdentifierFromOrigin(WebSecurityOriginToGURL(origin)));
}

//------------------------------------------------------------------------------

#if defined(OS_MACOSX)

bool RendererBlinkPlatformImpl::SandboxSupport::LoadFont(CTFontRef src_font,
                                                         CGFontRef* out,
                                                         uint32_t* font_id) {
  return content::LoadFont(src_font, out, font_id);
}

#elif defined(OS_POSIX) && !defined(OS_ANDROID)

void RendererBlinkPlatformImpl::SandboxSupport::GetFallbackFontForCharacter(
    blink::WebUChar32 character,
    const char* preferred_locale,
    blink::OutOfProcessFont* fallbackFont) {
  base::AutoLock lock(unicode_font_families_mutex_);
  const std::map<int32_t, blink::OutOfProcessFont>::const_iterator iter =
      unicode_font_families_.find(character);
  if (iter != unicode_font_families_.end()) {
    fallbackFont->name = iter->second.name;
    fallbackFont->filename = iter->second.filename;
    fallbackFont->fontconfig_interface_id =
        iter->second.fontconfig_interface_id;
    fallbackFont->ttc_index = iter->second.ttc_index;
    fallbackFont->is_bold = iter->second.is_bold;
    fallbackFont->is_italic = iter->second.is_italic;
    return;
  }

  content::GetFallbackFontForCharacter(font_loader_, character,
                                       preferred_locale, fallbackFont);
  unicode_font_families_.insert(std::make_pair(character, *fallbackFont));
}

void RendererBlinkPlatformImpl::SandboxSupport::
    MatchFontByPostscriptNameOrFullFontName(
        const char* font_unique_name,
        blink::OutOfProcessFont* fallback_font) {
  content::MatchFontByPostscriptNameOrFullFontName(
      font_loader_, font_unique_name, fallback_font);
}

void RendererBlinkPlatformImpl::SandboxSupport::GetWebFontRenderStyleForStrike(
    const char* family,
    int size,
    bool is_bold,
    bool is_italic,
    float device_scale_factor,
    blink::WebFontRenderStyle* out) {
  GetRenderStyleForStrike(font_loader_, family, size, is_bold, is_italic,
                          device_scale_factor, out);
}

#endif

//------------------------------------------------------------------------------

Platform::FileHandle RendererBlinkPlatformImpl::DatabaseOpenFile(
    const WebString& vfs_file_name,
    int desired_flags) {
  base::File file;
  GetWebDatabaseHost().OpenFile(vfs_file_name.Utf16(), desired_flags, &file);
  return file.TakePlatformFile();
}

int RendererBlinkPlatformImpl::DatabaseDeleteFile(
    const WebString& vfs_file_name,
    bool sync_dir) {
  int rv = SQLITE_IOERR_DELETE;
  GetWebDatabaseHost().DeleteFile(vfs_file_name.Utf16(), sync_dir, &rv);
  return rv;
}

long RendererBlinkPlatformImpl::DatabaseGetFileAttributes(
    const WebString& vfs_file_name) {
  int32_t rv = -1;
  GetWebDatabaseHost().GetFileAttributes(vfs_file_name.Utf16(), &rv);
  return rv;
}

long long RendererBlinkPlatformImpl::DatabaseGetFileSize(
    const WebString& vfs_file_name) {
  int64_t rv = 0LL;
  GetWebDatabaseHost().GetFileSize(vfs_file_name.Utf16(), &rv);
  return rv;
}

long long RendererBlinkPlatformImpl::DatabaseGetSpaceAvailableForOrigin(
    const blink::WebSecurityOrigin& origin) {
  int64_t rv = 0LL;
  GetWebDatabaseHost().GetSpaceAvailable(origin, &rv);
  return rv;
}

bool RendererBlinkPlatformImpl::DatabaseSetFileSize(
    const WebString& vfs_file_name,
    long long size) {
  bool rv = false;
  GetWebDatabaseHost().SetFileSize(vfs_file_name.Utf16(), size, &rv);
  return rv;
}

WebString RendererBlinkPlatformImpl::DatabaseCreateOriginIdentifier(
    const blink::WebSecurityOrigin& origin) {
  return WebString::FromUTF8(
      storage::GetIdentifierFromOrigin(WebSecurityOriginToGURL(origin)));
}

viz::FrameSinkId RendererBlinkPlatformImpl::GenerateFrameSinkId() {
  return viz::FrameSinkId(RenderThread::Get()->GetClientId(),
                          RenderThread::Get()->GenerateRoutingID());
}

bool RendererBlinkPlatformImpl::IsLockedToSite() const {
  return is_locked_to_site_;
}

void RendererBlinkPlatformImpl::SetIsLockedToSite() {
  is_locked_to_site_ = true;
}

bool RendererBlinkPlatformImpl::IsGpuCompositingDisabled() {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);
  RenderThreadImpl* thread = RenderThreadImpl::current();
  // |thread| can be NULL in tests.
  return !thread || thread->IsGpuCompositingDisabled();
}

bool RendererBlinkPlatformImpl::IsThreadedAnimationEnabled() {
  RenderThreadImpl* thread = RenderThreadImpl::current();
  return thread ? thread->IsThreadedAnimationEnabled() : true;
}

double RendererBlinkPlatformImpl::AudioHardwareSampleRate() {
  return GetAudioHardwareParams().sample_rate();
}

size_t RendererBlinkPlatformImpl::AudioHardwareBufferSize() {
  return GetAudioHardwareParams().frames_per_buffer();
}

unsigned RendererBlinkPlatformImpl::AudioHardwareOutputChannels() {
  return GetAudioHardwareParams().channels();
}

WebDatabaseObserver* RendererBlinkPlatformImpl::DatabaseObserver() {
  if (!web_database_observer_impl_) {
    InitializeWebDatabaseHostIfNeeded();
    web_database_observer_impl_ =
        std::make_unique<WebDatabaseObserverImpl>(web_database_host_);
  }
  return web_database_observer_impl_.get();
}

std::unique_ptr<WebAudioDevice> RendererBlinkPlatformImpl::CreateAudioDevice(
    unsigned input_channels,
    unsigned channels,
    const blink::WebAudioLatencyHint& latency_hint,
    WebAudioDevice::RenderCallback* callback,
    const blink::WebString& input_device_id) {
  // The |channels| does not exactly identify the channel layout of the
  // device. The switch statement below assigns a best guess to the channel
  // layout based on number of channels.
  media::ChannelLayout layout = media::GuessChannelLayout(channels);
  if (layout == media::CHANNEL_LAYOUT_UNSUPPORTED)
    layout = media::CHANNEL_LAYOUT_DISCRETE;

  int session_id = 0;
  if (input_device_id.IsNull() ||
      !base::StringToInt(input_device_id.Utf8(), &session_id)) {
    session_id = 0;
  }

  return RendererWebAudioDeviceImpl::Create(layout, channels, latency_hint,
                                            callback, session_id);
}

bool RendererBlinkPlatformImpl::DecodeAudioFileData(
    blink::WebAudioBus* destination_bus,
    const char* audio_file_data,
    size_t data_size) {
  return content::DecodeAudioFileData(destination_bus, audio_file_data,
                                      data_size);
}

//------------------------------------------------------------------------------

std::unique_ptr<blink::WebMIDIAccessor>
RendererBlinkPlatformImpl::CreateMIDIAccessor(
    blink::WebMIDIAccessorClient* client) {
  std::unique_ptr<blink::WebMIDIAccessor> accessor =
      GetContentClient()->renderer()->OverrideCreateMIDIAccessor(client);
  if (accessor)
    return accessor;

  return std::make_unique<RendererWebMIDIAccessorImpl>(client);
}

//------------------------------------------------------------------------------

WebBlobRegistry* RendererBlinkPlatformImpl::GetBlobRegistry() {
  // blob_registry_ can be NULL when running some tests.
  return blob_registry_.get();
}

//------------------------------------------------------------------------------

std::unique_ptr<WebMediaRecorderHandler>
RendererBlinkPlatformImpl::CreateMediaRecorderHandler(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  return std::make_unique<content::MediaRecorderHandler>(
      std::move(task_runner));
}

//------------------------------------------------------------------------------

std::unique_ptr<WebRTCPeerConnectionHandler>
RendererBlinkPlatformImpl::CreateRTCPeerConnectionHandler(
    WebRTCPeerConnectionHandlerClient* client,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  RenderThreadImpl* render_thread = RenderThreadImpl::current();
  DCHECK(render_thread);
  if (!render_thread)
    return nullptr;

  PeerConnectionDependencyFactory* rtc_dependency_factory =
      render_thread->GetPeerConnectionDependencyFactory();
  return rtc_dependency_factory->CreateRTCPeerConnectionHandler(client,
                                                                task_runner);
}

//------------------------------------------------------------------------------

std::unique_ptr<blink::WebRTCCertificateGenerator>
RendererBlinkPlatformImpl::CreateRTCCertificateGenerator() {
  return std::make_unique<RTCCertificateGenerator>();
}

//------------------------------------------------------------------------------

std::unique_ptr<WebMediaStreamCenter>
RendererBlinkPlatformImpl::CreateMediaStreamCenter() {
  RenderThreadImpl* render_thread = RenderThreadImpl::current();
  DCHECK(render_thread);
  if (!render_thread)
    return nullptr;
  return render_thread->CreateMediaStreamCenter();
}

// static
bool RendererBlinkPlatformImpl::SetSandboxEnabledForTesting(bool enable) {
  bool was_enabled = g_sandbox_enabled;
  g_sandbox_enabled = enable;
  return was_enabled;
}

//------------------------------------------------------------------------------

scoped_refptr<base::SingleThreadTaskRunner>
RendererBlinkPlatformImpl::GetWebRtcWorkerThread() {
  RenderThreadImpl* render_thread = RenderThreadImpl::current();
  DCHECK(render_thread);
  PeerConnectionDependencyFactory* rtc_dependency_factory =
      render_thread->GetPeerConnectionDependencyFactory();
  rtc_dependency_factory->EnsureInitialized();
  return rtc_dependency_factory->GetWebRtcWorkerThread();
}

rtc::Thread* RendererBlinkPlatformImpl::GetWebRtcWorkerThreadRtcThread() {
  RenderThreadImpl* render_thread = RenderThreadImpl::current();
  DCHECK(render_thread);
  PeerConnectionDependencyFactory* rtc_dependency_factory =
      render_thread->GetPeerConnectionDependencyFactory();
  rtc_dependency_factory->EnsureInitialized();
  return rtc_dependency_factory->GetWebRtcWorkerThreadRtcThread();
}

std::unique_ptr<cricket::PortAllocator>
RendererBlinkPlatformImpl::CreateWebRtcPortAllocator(
    blink::WebLocalFrame* frame) {
  RenderThreadImpl* render_thread = RenderThreadImpl::current();
  DCHECK(render_thread);
  PeerConnectionDependencyFactory* rtc_dependency_factory =
      render_thread->GetPeerConnectionDependencyFactory();
  rtc_dependency_factory->EnsureInitialized();
  return rtc_dependency_factory->CreatePortAllocator(frame);
}

//------------------------------------------------------------------------------

std::unique_ptr<WebCanvasCaptureHandler>
RendererBlinkPlatformImpl::CreateCanvasCaptureHandler(
    const WebSize& size,
    double frame_rate,
    WebMediaStreamTrack* track) {
  return CanvasCaptureHandler::CreateCanvasCaptureHandler(
      size, frame_rate, RenderThread::Get()->GetIOTaskRunner(), track);
}

//------------------------------------------------------------------------------

void RendererBlinkPlatformImpl::CreateHTMLVideoElementCapturer(
    WebMediaStream* web_media_stream,
    WebMediaPlayer* web_media_player,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  DCHECK(web_media_stream);
  DCHECK(web_media_player);
  AddVideoTrackToMediaStream(
      HtmlVideoElementCapturerSource::CreateFromWebMediaPlayerImpl(
          web_media_player, content::RenderThread::Get()->GetIOTaskRunner(),
          task_runner),
      false,  // is_remote
      web_media_stream);
}

void RendererBlinkPlatformImpl::CreateHTMLAudioElementCapturer(
    WebMediaStream* web_media_stream,
    WebMediaPlayer* web_media_player) {
  DCHECK(web_media_stream);
  DCHECK(web_media_player);

  blink::WebMediaStreamSource web_media_stream_source;
  blink::WebMediaStreamTrack web_media_stream_track;
  const WebString track_id = WebString::FromUTF8(base::GenerateGUID());

  web_media_stream_source.Initialize(track_id,
                                     blink::WebMediaStreamSource::kTypeAudio,
                                     track_id, false /* is_remote */);
  web_media_stream_track.Initialize(web_media_stream_source);

  MediaStreamAudioSource* const media_stream_source =
      HtmlAudioElementCapturerSource::CreateFromWebMediaPlayerImpl(
          web_media_player);

  // Takes ownership of |media_stream_source|.
  web_media_stream_source.SetExtraData(media_stream_source);

  blink::WebMediaStreamSource::Capabilities capabilities;
  capabilities.device_id = track_id;
  capabilities.echo_cancellation = std::vector<bool>({false});
  capabilities.auto_gain_control = std::vector<bool>({false});
  capabilities.noise_suppression = std::vector<bool>({false});
  web_media_stream_source.SetCapabilities(capabilities);

  media_stream_source->ConnectToTrack(web_media_stream_track);
  web_media_stream->AddTrack(web_media_stream_track);
}

//------------------------------------------------------------------------------

std::unique_ptr<WebImageCaptureFrameGrabber>
RendererBlinkPlatformImpl::CreateImageCaptureFrameGrabber() {
  return std::make_unique<ImageCaptureFrameGrabber>();
}

//------------------------------------------------------------------------------

std::unique_ptr<webrtc::RtpCapabilities>
RendererBlinkPlatformImpl::GetRtpSenderCapabilities(
    const blink::WebString& kind) {
  PeerConnectionDependencyFactory* pc_dependency_factory =
      RenderThreadImpl::current()->GetPeerConnectionDependencyFactory();
  pc_dependency_factory->EnsureInitialized();
  return pc_dependency_factory->GetSenderCapabilities(kind.Utf8());
}

std::unique_ptr<webrtc::RtpCapabilities>
RendererBlinkPlatformImpl::GetRtpReceiverCapabilities(
    const blink::WebString& kind) {
  PeerConnectionDependencyFactory* pc_dependency_factory =
      RenderThreadImpl::current()->GetPeerConnectionDependencyFactory();
  pc_dependency_factory->EnsureInitialized();
  return pc_dependency_factory->GetReceiverCapabilities(kind.Utf8());
}

//------------------------------------------------------------------------------

void RendererBlinkPlatformImpl::UpdateWebRTCAPICount(
    blink::WebRTCAPIName api_name) {
  UpdateWebRTCMethodCount(api_name);
}

//------------------------------------------------------------------------------

std::unique_ptr<blink::WebSpeechSynthesizer>
RendererBlinkPlatformImpl::CreateSpeechSynthesizer(
    blink::WebSpeechSynthesizerClient* client) {
  return GetContentClient()->renderer()->OverrideSpeechSynthesizer(client);
}

//------------------------------------------------------------------------------

static void Collect3DContextInformation(
    blink::Platform::GraphicsInfo* gl_info,
    const gpu::GPUInfo& gpu_info) {
  DCHECK(gl_info);
  const gpu::GPUInfo::GPUDevice& active_gpu = gpu_info.active_gpu();
  gl_info->vendor_id = active_gpu.vendor_id;
  gl_info->device_id = active_gpu.device_id;
  gl_info->renderer_info = WebString::FromUTF8(gpu_info.gl_renderer);
  gl_info->vendor_info = WebString::FromUTF8(gpu_info.gl_vendor);
  gl_info->driver_version = WebString::FromUTF8(active_gpu.driver_version);
  gl_info->reset_notification_strategy =
      gpu_info.gl_reset_notification_strategy;
  gl_info->sandboxed = gpu_info.sandboxed;
  gl_info->amd_switchable = gpu_info.amd_switchable;
  gl_info->optimus = gpu_info.optimus;
}

std::unique_ptr<blink::WebGraphicsContext3DProvider>
RendererBlinkPlatformImpl::CreateOffscreenGraphicsContext3DProvider(
    const blink::Platform::ContextAttributes& web_attributes,
    const blink::WebURL& top_document_web_url,
    blink::Platform::GraphicsInfo* gl_info) {
  DCHECK(gl_info);
  if (!RenderThreadImpl::current()) {
    std::string error_message("Failed to run in Current RenderThreadImpl");
    gl_info->error_message = WebString::FromUTF8(error_message);
    return nullptr;
  }

  scoped_refptr<gpu::GpuChannelHost> gpu_channel_host(
      RenderThreadImpl::current()->EstablishGpuChannelSync());
  if (!gpu_channel_host) {
    std::string error_message(
        "OffscreenContext Creation failed, GpuChannelHost creation failed");
    gl_info->error_message = WebString::FromUTF8(error_message);
    return nullptr;
  }
  Collect3DContextInformation(gl_info, gpu_channel_host->gpu_info());

  // This is an offscreen context. Generally it won't use the default
  // frame buffer, in that case don't request any alpha, depth, stencil,
  // antialiasing. But we do need those attributes for the "own
  // offscreen surface" optimization which supports directly drawing
  // to a custom surface backed frame buffer.
  gpu::ContextCreationAttribs attributes;
  attributes.alpha_size = web_attributes.support_alpha ? 8 : -1;
  attributes.depth_size = web_attributes.support_depth ? 24 : 0;
  attributes.stencil_size = web_attributes.support_stencil ? 8 : 0;
  attributes.samples = web_attributes.support_antialias ? 4 : 0;
  attributes.own_offscreen_surface =
      web_attributes.support_alpha || web_attributes.support_depth ||
      web_attributes.support_stencil || web_attributes.support_antialias;
  attributes.sample_buffers = 0;
  attributes.bind_generates_resource = false;
  attributes.enable_raster_interface = web_attributes.enable_raster_interface;
  // Prefer discrete GPU for WebGL.
  attributes.gpu_preference = gl::PreferDiscreteGpu;

  attributes.fail_if_major_perf_caveat =
      web_attributes.fail_if_major_performance_caveat;

  attributes.context_type = ToGpuContextType(web_attributes.context_type);

  constexpr bool automatic_flushes = true;
  constexpr bool support_locking = false;

  uint32_t stream_id = kGpuStreamIdDefault;
  gpu::SchedulingPriority priority = kGpuStreamPriorityDefault;
  if (gpu_channel_host->gpu_feature_info().IsWorkaroundEnabled(
          gpu::USE_HIGH_PRIORITY_FOR_WEBGL)) {
    stream_id = kGpuStreamIdHighPriorityWebGL;
    priority = kGpuStreamPriorityHighPriorityWebGL;
  }

  scoped_refptr<ws::ContextProviderCommandBuffer> provider(
      new ws::ContextProviderCommandBuffer(
          std::move(gpu_channel_host),
          RenderThreadImpl::current()->GetGpuMemoryBufferManager(), stream_id,
          priority, gpu::kNullSurfaceHandle, GURL(top_document_web_url),
          automatic_flushes, support_locking, web_attributes.support_grcontext,
          gpu::SharedMemoryLimits(), attributes,
          ws::command_buffer_metrics::ContextType::WEBGL));
  return std::make_unique<WebGraphicsContext3DProviderImpl>(
      std::move(provider));
}

//------------------------------------------------------------------------------

std::unique_ptr<blink::WebGraphicsContext3DProvider>
RendererBlinkPlatformImpl::CreateSharedOffscreenGraphicsContext3DProvider() {
  auto* thread = RenderThreadImpl::current();

  scoped_refptr<ws::ContextProviderCommandBuffer> provider =
      thread->SharedMainThreadContextProvider();
  if (!provider)
    return nullptr;

  scoped_refptr<gpu::GpuChannelHost> host = thread->EstablishGpuChannelSync();
  // This shouldn't normally fail because we just got |provider|. But the
  // channel can become lost on the IO thread since then. It is important that
  // this happens after getting |provider|. In the case that this GpuChannelHost
  // is not the same one backing |provider|, the context behind the |provider|
  // will be already lost/dead on arrival.
  if (!host)
    return nullptr;

  return std::make_unique<WebGraphicsContext3DProviderImpl>(
      std::move(provider));
}

//------------------------------------------------------------------------------

std::unique_ptr<blink::WebGraphicsContext3DProvider>
RendererBlinkPlatformImpl::CreateWebGPUGraphicsContext3DProvider(
    const blink::WebURL& top_document_web_url,
    blink::Platform::GraphicsInfo* gl_info) {
  scoped_refptr<gpu::GpuChannelHost> gpu_channel_host(
      RenderThreadImpl::current()->EstablishGpuChannelSync());
  if (!gpu_channel_host) {
    std::string error_message(
        "OffscreenContext Creation failed, GpuChannelHost creation failed");
    gl_info->error_message = WebString::FromUTF8(error_message);
    return nullptr;
  }
  Collect3DContextInformation(gl_info, gpu_channel_host->gpu_info());

  gpu::ContextCreationAttribs attributes;
  // TODO(kainino): It's not clear yet how GPU preferences work for WebGPU.
  attributes.gpu_preference = gl::PreferDiscreteGpu;
  attributes.enable_gles2_interface = false;
  attributes.context_type = gpu::CONTEXT_TYPE_WEBGPU;

  constexpr bool automatic_flushes = true;
  constexpr bool support_locking = false;
  constexpr bool support_grcontext = false;

  scoped_refptr<ws::ContextProviderCommandBuffer> provider(
      new ws::ContextProviderCommandBuffer(
          std::move(gpu_channel_host),
          RenderThreadImpl::current()->GetGpuMemoryBufferManager(),
          kGpuStreamIdDefault, kGpuStreamPriorityDefault,
          gpu::kNullSurfaceHandle, GURL(top_document_web_url),
          automatic_flushes, support_locking, support_grcontext,
          gpu::SharedMemoryLimits(), attributes,
          ws::command_buffer_metrics::ContextType::WEBGPU));
  return std::make_unique<WebGraphicsContext3DProviderImpl>(
      std::move(provider));
}

//------------------------------------------------------------------------------

gpu::GpuMemoryBufferManager*
RendererBlinkPlatformImpl::GetGpuMemoryBufferManager() {
  RenderThreadImpl* thread = RenderThreadImpl::current();
  return thread ? thread->GetGpuMemoryBufferManager() : nullptr;
}

//------------------------------------------------------------------------------

blink::WebString RendererBlinkPlatformImpl::ConvertIDNToUnicode(
    const blink::WebString& host) {
  return WebString::FromUTF16(url_formatter::IDNToUnicode(host.Ascii()));
}

//------------------------------------------------------------------------------

void RendererBlinkPlatformImpl::RecordRappor(const char* metric,
                                             const blink::WebString& sample) {
  GetContentClient()->renderer()->RecordRappor(metric, sample.Utf8());
}

void RendererBlinkPlatformImpl::RecordRapporURL(const char* metric,
                                                const blink::WebURL& url) {
  GetContentClient()->renderer()->RecordRapporURL(metric, url);
}

service_manager::Connector* RendererBlinkPlatformImpl::GetConnector() {
  return connector_.get();
}

blink::InterfaceProvider* RendererBlinkPlatformImpl::GetInterfaceProvider() {
  return blink_interface_provider_.get();
}

//------------------------------------------------------------------------------

blink::WebPushProvider* RendererBlinkPlatformImpl::PushProvider() {
  return PushProvider::ThreadSpecificInstance(default_task_runner_);
}

//------------------------------------------------------------------------------

void RendererBlinkPlatformImpl::DidStartWorkerThread() {
  WorkerThreadRegistry::Instance()->DidStartCurrentWorkerThread();
}

void RendererBlinkPlatformImpl::WillStopWorkerThread() {
  WorkerThreadRegistry::Instance()->WillStopCurrentWorkerThread();
}

void RendererBlinkPlatformImpl::WorkerContextCreated(
    const v8::Local<v8::Context>& worker) {
  GetContentClient()->renderer()->DidInitializeWorkerContextOnWorkerThread(
      worker);
}

//------------------------------------------------------------------------------
void RendererBlinkPlatformImpl::RequestPurgeMemory() {
  base::MemoryPressureListener::NotifyMemoryPressure(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL);
}

void RendererBlinkPlatformImpl::SetMemoryPressureNotificationsSuppressed(
    bool suppressed) {
  base::MemoryPressureListener::SetNotificationsSuppressed(suppressed);
}

void RendererBlinkPlatformImpl::InitializeWebDatabaseHostIfNeeded() {
  if (!web_database_host_) {
    web_database_host_ = blink::mojom::ThreadSafeWebDatabaseHostPtr::Create(
        std::move(web_database_host_info_),
        base::CreateSequencedTaskRunnerWithTraits(
            {base::WithBaseSyncPrimitives()}));
  }
}

blink::mojom::WebDatabaseHost& RendererBlinkPlatformImpl::GetWebDatabaseHost() {
  InitializeWebDatabaseHostIfNeeded();
  return **web_database_host_;
}

blink::mojom::CodeCacheHost& RendererBlinkPlatformImpl::GetCodeCacheHost() {
  if (!code_cache_host_) {
    code_cache_host_ = blink::mojom::ThreadSafeCodeCacheHostPtr::Create(
        std::move(code_cache_host_info_),
        base::CreateSequencedTaskRunnerWithTraits(
            {base::WithBaseSyncPrimitives()}));
  }
  return **code_cache_host_;
}

}  // namespace content
