// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/342213636): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "content/renderer/renderer_blink_platform_impl.h"

#include <algorithm>
#include <memory>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check_is_test.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/pattern.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "base/time/time_delta_from_string.h"
#include "build/build_config.h"
#include "cc/trees/raster_context_provider_wrapper.h"
#include "components/url_formatter/url_formatter.h"
#include "components/viz/common/features.h"
#include "content/child/child_process.h"
#include "content/common/content_constants_internal.h"
#include "content/common/features.h"
#include "content/common/user_level_memory_pressure_signal_features.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/gpu_stream_constants.h"
#include "content/public/common/origin_util.h"
#include "content/public/common/url_utils.h"
#include "content/public/common/webplugininfo.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/public/renderer/render_frame.h"
#include "content/renderer/media/audio_decoder.h"
#include "content/renderer/media/batching_media_log.h"
#include "content/renderer/media/inspector_media_event_handler.h"
#include "content/renderer/media/render_media_event_handler.h"
#include "content/renderer/media/renderer_webaudiodevice_impl.h"
#include "content/renderer/render_frame_impl.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/service_worker/controller_service_worker_connector.h"
#include "content/renderer/service_worker/service_worker_subresource_loader.h"
#include "content/renderer/v8_value_converter_impl.h"
#include "content/renderer/variations_render_thread_observer.h"
#include "content/renderer/webgraphicscontext3d_provider_impl.h"
#include "content/renderer/worker/dedicated_worker_host_factory_client.h"
#include "content/renderer/worker/worker_thread_registry.h"
#include "device/gamepad/public/cpp/gamepads.h"
#include "gin/array_buffer.h"  // TODO(crbug.com/40837434) remove import once resolved.
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/config/gpu_info.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "media/audio/audio_output_device.h"
#include "media/base/media_permission.h"
#include "media/base/media_switches.h"
#include "media/filters/stream_parser_factory.h"
#include "media/video/gpu_video_accelerator_factories.h"
#include "media/webrtc/webrtc_features.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "net/base/schemeful_site.h"
#include "ppapi/buildflags/buildflags.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/wrapper_shared_url_loader_factory.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "services/viz/public/cpp/gpu/context_provider_command_buffer.h"
#include "storage/common/database/database_identifier.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/origin_trials/trial_token_validator.h"
#include "third_party/blink/public/common/security/protocol_handler_security_level.h"
#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_provider.mojom.h"
#include "third_party/blink/public/platform/file_path_conversion.h"
#include "third_party/blink/public/platform/modules/video_capture/web_video_capture_impl_manager.h"
#include "third_party/blink/public/platform/scheduler/web_thread_scheduler.h"
#include "third_party/blink/public/platform/url_conversion.h"
#include "third_party/blink/public/platform/web_audio_latency_hint.h"
#include "third_party/blink/public/platform/web_audio_sink_descriptor.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/platform/web_theme_engine.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/platform/web_v8_value_converter.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/public/web/modules/media/audio/audio_device_factory.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_media_inspector.h"
#include "third_party/blink/public/web/web_user_level_memory_pressure_signal_generator.h"
#include "third_party/sqlite/sqlite3.h"
#include "ui/base/ui_base_switches.h"
#include "ui/gl/buildflags.h"
#include "url/gurl.h"
#include "url/origin.h"

#if BUILDFLAG(IS_WIN)
#include "content/renderer/font_data/font_data_manager.h"
#include "skia/ext/font_utils.h"
#include "third_party/blink/public/web/win/web_font_rendering.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "content/child/child_process_sandbox_support_impl_mac.h"
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include "content/child/child_process_sandbox_support_impl_linux.h"
#include "content/child/sandboxed_process_thread_type_handler.h"
#endif

#if BUILDFLAG(IS_POSIX)
#include "base/file_descriptor_posix.h"
#endif

#if BUILDFLAG(IS_ANDROID)
#include "content/common/android/sync_compositor_statics.h"
#endif

using blink::Platform;
using blink::WebAudioDevice;
using blink::WebAudioLatencyHint;
using blink::WebAudioSinkDescriptor;
using blink::WebMediaStreamTrack;
using blink::WebString;
using blink::WebURL;
using blink::WebVector;

namespace content {

namespace {

// TODO(crbug.com/40550966): Move this method and its callers to Blink.
media::AudioParameters GetAudioHardwareParams() {
  blink::WebLocalFrame* const web_frame =
      blink::WebLocalFrame::FrameForCurrentContext();
  RenderFrame* const render_frame = RenderFrame::FromWebFrame(web_frame);
  if (!render_frame)
    return media::AudioParameters::UnavailableDeviceParams();

  return blink::AudioDeviceFactory::GetInstance()
      ->GetOutputDeviceInfo(render_frame->GetWebFrame()->GetLocalFrameToken(),
                            std::string())
      .output_params();
}

gpu::ContextType ToGpuContextType(blink::Platform::ContextType type) {
  switch (type) {
    case blink::Platform::kWebGL1ContextType:
      return gpu::CONTEXT_TYPE_WEBGL1;
    case blink::Platform::kWebGL2ContextType:
      return gpu::CONTEXT_TYPE_WEBGL2;
    case blink::Platform::kGLES2ContextType:
      return gpu::CONTEXT_TYPE_OPENGLES2;
    case blink::Platform::kGLES3ContextType:
      return gpu::CONTEXT_TYPE_OPENGLES3;
    case blink::Platform::kWebGPUContextType:
      return gpu::CONTEXT_TYPE_WEBGPU;
  }
  NOTREACHED_IN_MIGRATION();
  return gpu::CONTEXT_TYPE_OPENGLES2;
}

}  // namespace

//------------------------------------------------------------------------------

RendererBlinkPlatformImpl::RendererBlinkPlatformImpl(
    blink::scheduler::WebThreadScheduler* main_thread_scheduler)
    : BlinkPlatformImpl(RenderThreadImpl::current()
                            ? RenderThreadImpl::current()->GetIOTaskRunner()
                            : nullptr),
      sudden_termination_disables_(0),
      is_locked_to_site_(false),
      main_thread_scheduler_(main_thread_scheduler),
      next_frame_sink_id_(uint32_t{std::numeric_limits<int32_t>::max()} + 1) {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  sk_sp<font_service::FontLoader> font_loader;
#endif

  // RenderThread may not exist in some tests.
  if (RenderThreadImpl::current()) {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
    mojo::PendingRemote<font_service::mojom::FontService> font_service;
    RenderThreadImpl::current()->BindHostReceiver(
        font_service.InitWithNewPipeAndPassReceiver());
    font_loader = sk_make_sp<font_service::FontLoader>(std::move(font_service));
    SkFontConfigInterface::SetGlobal(font_loader);
#endif

#if BUILDFLAG(IS_WIN)
    if (base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kUseSkiaFontManager)) {
      sk_sp<font_data_service::FontDataManager> font_data_manager =
          sk_make_sp<font_data_service::FontDataManager>();

      blink::WebFontRendering::SetSkiaFontManager(font_data_manager);
      skia::OverrideDefaultSkFontMgr(font_data_manager);
    }
#endif
  }

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC)
  if (sandboxEnabled()) {
#if BUILDFLAG(IS_MAC)
    sandbox_support_ = std::make_unique<WebSandboxSupportMac>();
#else
    sandbox_support_ = std::make_unique<WebSandboxSupportLinux>(font_loader);
#endif
  } else {
    DVLOG(1) << "Disabling sandbox support for testing.";
  }
#endif

  auto io_task_runner = GetIOTaskRunner();
  if (io_task_runner) {
    io_task_runner->PostTask(
        FROM_HERE, base::BindOnce(
                       [](base::PlatformThreadId* id,
                          base::WaitableEvent* io_thread_id_ready_event) {
                         *id = base::PlatformThread::CurrentId();
                         io_thread_id_ready_event->Signal();
                       },
                       &io_thread_id_, &io_thread_id_ready_event_));
  } else {
    // Match the `Wait` in destructor even if there is no IO runner.
    io_thread_id_ready_event_.Signal();
  }
}

RendererBlinkPlatformImpl::~RendererBlinkPlatformImpl() {
  base::ScopedAllowBaseSyncPrimitives allow;
  // Ensure task posted to IO thread is finished because it contains
  // pointers to fields of `this`.
  io_thread_id_ready_event_.Wait();
}

void RendererBlinkPlatformImpl::Shutdown() {}

//------------------------------------------------------------------------------

std::string RendererBlinkPlatformImpl::GetNameForHistogram(const char* name) {
  RenderThreadImpl* render_thread_impl = RenderThreadImpl::current();
  // render_thread_impl can be null in tests.
  return render_thread_impl ? render_thread_impl->histogram_customizer()
                                  ->ConvertToCustomHistogramName(name)
                            : std::string{name};
}

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
void RendererBlinkPlatformImpl::SetThreadType(base::PlatformThreadId thread_id,
                                              base::ThreadType thread_type) {
  // TODO: both of the usages of this method could just be switched to use
  // base::PlatformThread::SetCurrentThreadType().
  if (SandboxedProcessThreadTypeHandler* sandboxed_process_thread_type_handler =
          SandboxedProcessThreadTypeHandler::Get()) {
    sandboxed_process_thread_type_handler->HandleThreadTypeChange(thread_id,
                                                                  thread_type);
  }
}
#endif

blink::WebSandboxSupport* RendererBlinkPlatformImpl::GetSandboxSupport() {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC)
  return sandbox_support_.get();
#else
  // These platforms do not require sandbox support.
  return nullptr;
#endif
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

uint64_t RendererBlinkPlatformImpl::VisitedLinkHash(
    std::string_view canonical_url) {
  return GetContentClient()->renderer()->VisitedLinkHash(canonical_url);
}

uint64_t RendererBlinkPlatformImpl::PartitionedVisitedLinkFingerprint(
    std::string_view canonical_link_url,
    const net::SchemefulSite& top_level_site,
    const blink::WebSecurityOrigin& frame_origin) {
  return GetContentClient()->renderer()->PartitionedVisitedLinkFingerprint(
      canonical_link_url, top_level_site, frame_origin);
}

bool RendererBlinkPlatformImpl::IsLinkVisited(uint64_t link_hash) {
  return GetContentClient()->renderer()->IsLinkVisited(link_hash);
}

void RendererBlinkPlatformImpl::AddOrUpdateVisitedLinkSalt(
    const url::Origin& origin,
    uint64_t salt) {
  GetContentClient()->renderer()->AddOrUpdateVisitedLinkSalt(origin, salt);
}

blink::WebString RendererBlinkPlatformImpl::UserAgent() {
  auto* render_thread = RenderThreadImpl::current();
  // RenderThreadImpl is null in some tests.
  if (!render_thread)
    return WebString();
  return render_thread->GetUserAgent();
}

blink::UserAgentMetadata RendererBlinkPlatformImpl::UserAgentMetadata() {
  auto* render_thread = RenderThreadImpl::current();
  // RenderThreadImpl is null in some tests.
  if (!render_thread)
    return blink::UserAgentMetadata();
  return render_thread->GetUserAgentMetadata();
}

bool RendererBlinkPlatformImpl::IsRedirectSafe(const GURL& from_url,
                                               const GURL& to_url) {
  return IsSafeRedirectTarget(from_url, to_url) &&
         (!GetContentClient()->renderer() ||  // null in unit tests.
          GetContentClient()->renderer()->IsSafeRedirectTarget(from_url,
                                                               to_url));
}

void RendererBlinkPlatformImpl::AppendVariationsThrottles(
    const url::Origin& top_origin,
    std::vector<std::unique_ptr<blink::URLLoaderThrottle>>* throttles) {
  VariationsRenderThreadObserver::AppendThrottleIfNeeded(top_origin, throttles);
}

std::unique_ptr<blink::URLLoaderThrottleProvider>
RendererBlinkPlatformImpl::CreateURLLoaderThrottleProviderForWorker(
    blink::URLLoaderThrottleProviderType provider_type) {
  return GetContentClient()->renderer()->CreateURLLoaderThrottleProvider(
      blink::URLLoaderThrottleProviderType::kWorker);
}

std::unique_ptr<blink::WebSocketHandshakeThrottleProvider>
RendererBlinkPlatformImpl::CreateWebSocketHandshakeThrottleProvider() {
  return GetContentClient()
      ->renderer()
      ->CreateWebSocketHandshakeThrottleProvider();
}

bool RendererBlinkPlatformImpl::ShouldUseCodeCacheWithHashing(
    const blink::WebURL& request_url) const {
  return GetContentClient()->renderer()->ShouldUseCodeCacheWithHashing(
      request_url);
}

bool RendererBlinkPlatformImpl::IsolateStartsInBackground() {
  if (auto* renderer = GetContentClient()->renderer()) {
    // Isolates start in the background if we do not handle hidden/visibility
    // changes for this process. See `RenderThreadImpl::OnRendererHidden` and
    // `RenderThreadImpl::OnRendererVisible`.
    return !renderer->RunIdleHandlerWhenWidgetsHidden();
  }
  return BlinkPlatformImpl::IsolateStartsInBackground();
}

WebString RendererBlinkPlatformImpl::DefaultLocale() {
  return WebString::FromASCII(RenderThread::Get()->GetLocale());
}

void RendererBlinkPlatformImpl::SuddenTerminationChanged(bool enabled) {
  if (enabled) {
    // We should not get more enables than disables, but we want it to be a
    // non-fatal error if it does happen.
    DCHECK_GT(sudden_termination_disables_, 0);
    sudden_termination_disables_ =
        std::max(sudden_termination_disables_ - 1, 0);
    if (sudden_termination_disables_ != 0)
      return;
  } else {
    sudden_termination_disables_++;
    if (sudden_termination_disables_ != 1)
      return;
  }

  RenderThreadImpl* thread = RenderThreadImpl::current();
  if (!thread) {
    CHECK_IS_TEST();
    return;
  }

  thread->GetRendererHost()->SuddenTerminationChanged(enabled);
}

//------------------------------------------------------------------------------

viz::FrameSinkId RendererBlinkPlatformImpl::GenerateFrameSinkId() {
  uint32_t frame_sink_id = next_frame_sink_id_++;
  CHECK_LT(frame_sink_id, next_frame_sink_id_);
  return viz::FrameSinkId(RenderThread::Get()->GetClientId(), frame_sink_id);
}

bool RendererBlinkPlatformImpl::IsLockedToSite() const {
  return is_locked_to_site_;
}

void RendererBlinkPlatformImpl::SetIsLockedToSite() {
  is_locked_to_site_ = true;
}

bool RendererBlinkPlatformImpl::IsGpuCompositingDisabled() const {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);
  RenderThreadImpl* thread = RenderThreadImpl::current();
  if (!thread) {
    CHECK_IS_TEST();
    return true;
  }

  return thread->IsGpuCompositingDisabled();
}

#if BUILDFLAG(IS_ANDROID)
bool RendererBlinkPlatformImpl::
    IsSynchronousCompositingEnabledForAndroidWebView() {
  return GetContentClient()->UsingSynchronousCompositing();
}

bool RendererBlinkPlatformImpl::
    IsZeroCopySynchronousSwDrawEnabledForAndroidWebView() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kSingleProcess);
}

SkCanvas*
RendererBlinkPlatformImpl::SynchronousCompositorGetSkCanvasForAndroidWebView() {
  return content::SynchronousCompositorGetSkCanvas();
}
#endif

bool RendererBlinkPlatformImpl::IsLcdTextEnabled() {
  RenderThreadImpl* thread = RenderThreadImpl::current();
  return thread ? thread->IsLcdTextEnabled() : false;
}

bool RendererBlinkPlatformImpl::IsElasticOverscrollEnabled() {
  RenderThreadImpl* thread = RenderThreadImpl::current();
  return thread ? thread->IsElasticOverscrollEnabled() : false;
}

bool RendererBlinkPlatformImpl::IsScrollAnimatorEnabled() {
  RenderThreadImpl* thread = RenderThreadImpl::current();
  return thread ? thread->IsScrollAnimatorEnabled() : false;
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

base::TimeDelta RendererBlinkPlatformImpl::GetHungRendererDelay() {
  return kHungRendererDelay;
}

std::unique_ptr<WebAudioDevice> RendererBlinkPlatformImpl::CreateAudioDevice(
    const WebAudioSinkDescriptor& sink_descriptor,
    unsigned number_of_output_channels,
    const blink::WebAudioLatencyHint& latency_hint,
    media::AudioRendererSink::RenderCallback* callback) {
  return RendererWebAudioDeviceImpl::Create(
      sink_descriptor, number_of_output_channels, latency_hint, callback);
}

bool RendererBlinkPlatformImpl::DecodeAudioFileData(
    blink::WebAudioBus* destination_bus,
    const char* audio_file_data,
    size_t data_size) {
  return content::DecodeAudioFileData(destination_bus, audio_file_data,
                                      data_size);
}

//------------------------------------------------------------------------------

scoped_refptr<media::AudioCapturerSource>
RendererBlinkPlatformImpl::NewAudioCapturerSource(
    blink::WebLocalFrame* web_frame,
    const media::AudioSourceParameters& params) {
  return blink::AudioDeviceFactory::GetInstance()->NewAudioCapturerSource(
      web_frame, params);
}

scoped_refptr<viz::RasterContextProvider>
RendererBlinkPlatformImpl::SharedMainThreadContextProvider() {
  return RenderThreadImpl::current()->SharedMainThreadContextProvider();
}

scoped_refptr<cc::RasterContextProviderWrapper>
RendererBlinkPlatformImpl::SharedCompositorWorkerContextProvider(
    cc::RasterDarkModeFilter* dark_mode_filter) {
  return RenderThreadImpl::current()->SharedCompositorWorkerContextProvider(
      dark_mode_filter);
}

bool RendererBlinkPlatformImpl::IsGpuRemoteDisconnected() {
  return RenderThreadImpl::current()->IsGpuRemoteDisconnected();
}

scoped_refptr<gpu::GpuChannelHost>
RendererBlinkPlatformImpl::EstablishGpuChannelSync() {
  return RenderThreadImpl::current()->EstablishGpuChannelSync();
}

void RendererBlinkPlatformImpl::EstablishGpuChannel(
    EstablishGpuChannelCallback callback) {
  RenderThreadImpl::current()->EstablishGpuChannel(std::move(callback));
}

bool RendererBlinkPlatformImpl::RTCSmoothnessAlgorithmEnabled() {
  return !base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableRTCSmoothnessAlgorithm);
}

//------------------------------------------------------------------------------

std::optional<double>
RendererBlinkPlatformImpl::GetWebRtcMaxCaptureFrameRate() {
  const std::string max_fps_str =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kWebRtcMaxCaptureFramerate);
  if (!max_fps_str.empty()) {
    double value;
    if (base::StringToDouble(max_fps_str, &value) && value >= 0.0)
      return value;
  }
  return std::nullopt;
}

scoped_refptr<media::AudioRendererSink>
RendererBlinkPlatformImpl::NewAudioRendererSink(
    blink::WebAudioDeviceSourceType source_type,
    blink::WebLocalFrame* web_frame,
    const media::AudioSinkParameters& params) {
  return blink::AudioDeviceFactory::GetInstance()->NewAudioRendererSink(
      source_type, web_frame->GetLocalFrameToken(), params);
}

media::AudioLatency::Type RendererBlinkPlatformImpl::GetAudioSourceLatencyType(
    blink::WebAudioDeviceSourceType source_type) {
  return blink::AudioDeviceFactory::GetSourceLatencyType(source_type);
}

bool RendererBlinkPlatformImpl::ShouldEnforceWebRTCRoutingPreferences() {
  return GetContentClient()
      ->renderer()
      ->ShouldEnforceWebRTCRoutingPreferences();
}

bool RendererBlinkPlatformImpl::UsesFakeCodecForPeerConnection() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kUseFakeCodecForPeerConnection);
}

bool RendererBlinkPlatformImpl::IsWebRtcEncryptionEnabled() {
  return !base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableWebRtcEncryption);
}

media::MediaPermission* RendererBlinkPlatformImpl::GetWebRTCMediaPermission(
    blink::WebLocalFrame* web_frame) {
  DCHECK(ShouldEnforceWebRTCRoutingPreferences());

  media::MediaPermission* media_permission = nullptr;
  RenderFrameImpl* render_frame = RenderFrameImpl::FromWebFrame(web_frame);
  if (render_frame)
    media_permission = render_frame->GetMediaPermission();
  DCHECK(media_permission);
  return media_permission;
}

void RendererBlinkPlatformImpl::GetWebRTCRendererPreferences(
    blink::WebLocalFrame* web_frame,
    blink::WebString* ip_handling_policy,
    uint16_t* udp_min_port,
    uint16_t* udp_max_port,
    bool* allow_mdns_obfuscation) {
  DCHECK(ip_handling_policy);
  DCHECK(udp_min_port);
  DCHECK(udp_max_port);
  DCHECK(allow_mdns_obfuscation);

  auto* render_frame = RenderFrameImpl::FromWebFrame(web_frame);
  if (!render_frame)
    return;

  *ip_handling_policy = blink::WebString::FromUTF8(
      render_frame->GetRendererPreferences().webrtc_ip_handling_policy);
  *udp_min_port = render_frame->GetRendererPreferences().webrtc_udp_min_port;
  *udp_max_port = render_frame->GetRendererPreferences().webrtc_udp_max_port;
  const std::vector<std::string>& allowed_urls =
      render_frame->GetRendererPreferences().webrtc_local_ips_allowed_urls;
  const std::string url(web_frame->GetSecurityOrigin().ToString().Utf8());
  for (const auto& allowed_url : allowed_urls) {
    if (base::MatchPattern(url, allowed_url)) {
      *allow_mdns_obfuscation = false;
      return;
    }
  }
  *allow_mdns_obfuscation = true;
}

bool RendererBlinkPlatformImpl::IsWebRtcHWEncodingEnabled() {
  return base::FeatureList::IsEnabled(::features::kWebRtcHWEncoding);
}

bool RendererBlinkPlatformImpl::IsWebRtcHWDecodingEnabled() {
  return base::FeatureList::IsEnabled(::features::kWebRtcHWDecoding);
}

bool RendererBlinkPlatformImpl::IsWebRtcSrtpEncryptedHeadersEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableWebRtcSrtpEncryptedHeaders);
}

bool RendererBlinkPlatformImpl::AllowsLoopbackInPeerConnection() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kAllowLoopbackInPeerConnection);
}

blink::WebVideoCaptureImplManager*
RendererBlinkPlatformImpl::GetVideoCaptureImplManager() {
  RenderThreadImpl* thread = RenderThreadImpl::current();
  return thread ? thread->video_capture_impl_manager() : nullptr;
}

//------------------------------------------------------------------------------

void RendererBlinkPlatformImpl::Collect3DContextInformation(
    blink::Platform::GraphicsInfo* gl_info,
    const gpu::GPUInfo& gpu_info) const {
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
  gl_info->using_gpu_compositing = !IsGpuCompositingDisabled();
  gl_info->using_passthrough_command_decoder = gpu_info.passthrough_cmd_decoder;
  gl_info->angle_implementation = gpu_info.gl_implementation_parts.angle;
}

std::unique_ptr<blink::WebGraphicsContext3DProvider>
RendererBlinkPlatformImpl::CreateOffscreenGraphicsContext3DProvider(
    const blink::Platform::ContextAttributes& web_attributes,
    const blink::WebURL& document_url,
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

  gpu::ContextCreationAttribs attributes;
  attributes.bind_generates_resource = false;
  attributes.enable_raster_interface = web_attributes.enable_raster_interface;
  attributes.enable_oop_rasterization =
      attributes.enable_raster_interface &&
      gpu_channel_host->gpu_feature_info()
              .status_values[gpu::GPU_FEATURE_TYPE_CANVAS_OOP_RASTERIZATION] ==
          gpu::kGpuFeatureStatusEnabled;
  attributes.enable_gles2_interface = !attributes.enable_oop_rasterization;
  attributes.enable_grcontext =
      !attributes.enable_oop_rasterization && web_attributes.support_grcontext;

  attributes.gpu_preference = web_attributes.prefer_low_power_gpu
                                  ? gl::GpuPreference::kLowPower
                                  : gl::GpuPreference::kHighPerformance;

  attributes.fail_if_major_perf_caveat =
      web_attributes.fail_if_major_performance_caveat;

  attributes.context_type = ToGpuContextType(web_attributes.context_type);

  constexpr bool automatic_flushes = true;
  constexpr bool support_locking = false;

  return std::make_unique<WebGraphicsContext3DProviderImpl>(
      base::MakeRefCounted<viz::ContextProviderCommandBuffer>(
          std::move(gpu_channel_host), kGpuStreamIdDefault,
          kGpuStreamPriorityDefault, gpu::kNullSurfaceHandle,
          GURL(document_url), automatic_flushes, support_locking,
          gpu::SharedMemoryLimits(), attributes,
          viz::command_buffer_metrics::ContextType::WEBGL));
}

//------------------------------------------------------------------------------

std::unique_ptr<blink::WebGraphicsContext3DProvider>
RendererBlinkPlatformImpl::CreateSharedOffscreenGraphicsContext3DProvider() {
  auto* thread = RenderThreadImpl::current();

  scoped_refptr<viz::ContextProviderCommandBuffer> provider =
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

static std::unique_ptr<blink::WebGraphicsContext3DProvider>
CreateWebGPUGraphicsContext3DImpl(
    const blink::WebURL& document_url,
    scoped_refptr<gpu::GpuChannelHost> gpu_channel_host) {
  gpu::ContextCreationAttribs attributes;
  // TODO(kainino): It's not clear yet how GPU preferences work for WebGPU.
  attributes.gpu_preference = gl::GpuPreference::kHighPerformance;
  attributes.enable_gles2_interface = false;
  attributes.context_type = gpu::CONTEXT_TYPE_WEBGPU;

  constexpr bool automatic_flushes = true;
  constexpr bool support_locking = false;

  // WebGPU GPUBuffers, which are backed by shared memory transfer buffers, may
  // be accessed as ArrayBuffers from JavaScript. As such, the underlying
  // buffers need to be mapped using the ArrayBuffer shared memory mapper. As
  // there is currently no way of specifying a custom mapper per buffer, we
  // have to map all buffers created by this provider using the custom mapper.
  // TODO(crbug.com/40837434) instead of mapping all buffers created by this
  // provider with the array buffer mapper, only map those that will actually
  // be used as ArrayBuffers and remove this per-provider mapper again.
  base::SharedMemoryMapper* buffer_mapper =
      gin::GetSharedMemoryMapperForArrayBuffers();

  return std::make_unique<WebGraphicsContext3DProviderImpl>(
      base::MakeRefCounted<viz::ContextProviderCommandBuffer>(
          std::move(gpu_channel_host), kGpuStreamIdDefault,
          kGpuStreamPriorityDefault, gpu::kNullSurfaceHandle,
          GURL(document_url), automatic_flushes, support_locking,
          gpu::SharedMemoryLimits::ForWebGPUContext(), attributes,
          viz::command_buffer_metrics::ContextType::WEBGPU, buffer_mapper));
}

std::unique_ptr<blink::WebGraphicsContext3DProvider>
RendererBlinkPlatformImpl::CreateWebGPUGraphicsContext3DProvider(
    const blink::WebURL& document_url) {
#if !BUILDFLAG(USE_DAWN)
  return nullptr;
#else
  scoped_refptr<gpu::GpuChannelHost> gpu_channel_host(
      RenderThreadImpl::current()->EstablishGpuChannelSync());
  if (!gpu_channel_host) {
    // TODO(crbug.com/41464325): Collect GPU info and surface context creation
    // error.
    return nullptr;
  }

  return CreateWebGPUGraphicsContext3DImpl(document_url, gpu_channel_host);
#endif
}

void RendererBlinkPlatformImpl::CreateWebGPUGraphicsContext3DProviderAsync(
    const blink::WebURL& document_url,
    base::OnceCallback<
        void(std::unique_ptr<blink::WebGraphicsContext3DProvider>)> callback) {
#if !BUILDFLAG(USE_DAWN)
  std::move(callback).Run(nullptr);
#else
  // Initiate the asynchronous call to establish the GPU channel
  RenderThreadImpl::current()->EstablishGpuChannel(base::BindOnce(
      &RendererBlinkPlatformImpl::OnGpuChannelEstablished,
      weak_factory_.GetWeakPtr(), document_url, std::move(callback)));
#endif
}

void RendererBlinkPlatformImpl::OnGpuChannelEstablished(
    const blink::WebURL& document_url,
    base::OnceCallback<
        void(std::unique_ptr<blink::WebGraphicsContext3DProvider>)> callback,
    scoped_refptr<gpu::GpuChannelHost> gpu_channel_host) {
  if (!gpu_channel_host) {
    std::move(callback).Run(nullptr);
    return;
  }

  std::move(callback).Run(
      CreateWebGPUGraphicsContext3DImpl(document_url, gpu_channel_host));
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

std::unique_ptr<blink::WebDedicatedWorkerHostFactoryClient>
RendererBlinkPlatformImpl::CreateDedicatedWorkerHostFactoryClient(
    blink::WebDedicatedWorker* worker,
    const blink::BrowserInterfaceBrokerProxy& interface_broker) {
  return std::make_unique<DedicatedWorkerHostFactoryClient>(worker,
                                                            interface_broker);
}

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

bool RendererBlinkPlatformImpl::AllowScriptExtensionForServiceWorker(
    const blink::WebSecurityOrigin& script_origin) {
  return GetContentClient()->renderer()->AllowScriptExtensionForServiceWorker(
      script_origin);
}

blink::ProtocolHandlerSecurityLevel
RendererBlinkPlatformImpl::GetProtocolHandlerSecurityLevel(
    const blink::WebSecurityOrigin& origin) {
  url::Origin url_origin(origin);
  return GetContentClient()->renderer()->GetProtocolHandlerSecurityLevel(
      url_origin);
}

bool RendererBlinkPlatformImpl::OriginCanAccessServiceWorkers(const GURL& url) {
  return content::OriginCanAccessServiceWorkers(url);
}

std::tuple<blink::CrossVariantMojoRemote<
               blink::mojom::ServiceWorkerContainerHostInterfaceBase>,
           blink::CrossVariantMojoRemote<
               blink::mojom::ServiceWorkerContainerHostInterfaceBase>>
RendererBlinkPlatformImpl::CloneServiceWorkerContainerHost(
    blink::CrossVariantMojoRemote<
        blink::mojom::ServiceWorkerContainerHostInterfaceBase>
        service_worker_container_host) {
  mojo::Remote<blink::mojom::ServiceWorkerContainerHost>
      service_worker_container_host_remote(
          std::move(service_worker_container_host));
  mojo::PendingRemote<blink::mojom::ServiceWorkerContainerHost>
      service_worker_container_host_pending_remote;

  service_worker_container_host_remote->CloneContainerHost(
      service_worker_container_host_pending_remote
          .InitWithNewPipeAndPassReceiver());
  return std::make_tuple(
      service_worker_container_host_remote.Unbind(),
      std::move(service_worker_container_host_pending_remote));
}

void RendererBlinkPlatformImpl::CreateServiceWorkerSubresourceLoaderFactory(
    blink::CrossVariantMojoRemote<
        blink::mojom::ServiceWorkerContainerHostInterfaceBase>
        service_worker_container_host,
    const blink::WebString& client_id,
    std::unique_ptr<network::PendingSharedURLLoaderFactory> fallback_factory,
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver,
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  // TODO(crbug.com/40241479): plumb `router_rules` with the function callers
  // if there is such use case. As of 2023-06-01, only
  // `DedicatedOrSharedWorkerFetchContextImpl` calls the function, and
  // no need to allow it set the `router_rules`.
  ServiceWorkerSubresourceLoaderFactory::Create(
      base::MakeRefCounted<ControllerServiceWorkerConnector>(
          std::move(service_worker_container_host),
          /*remote_controller=*/mojo::NullRemote(),
          /*remote_cache_storage=*/mojo::NullRemote(), client_id.Utf8(),
          blink::mojom::ServiceWorkerFetchHandlerBypassOption::kDefault,
          /*router_rules=*/std::nullopt, blink::EmbeddedWorkerStatus::kStopped,
          /*running_status_receiver=*/mojo::NullReceiver()),
      network::SharedURLLoaderFactory::Create(std::move(fallback_factory)),
      std::move(receiver), std::move(task_runner));
}

//------------------------------------------------------------------------------

// The returned BatchingMediaLog can be used on any thread, but must be
// destroyed on |owner_task_runner|. The aggregated MediaLogRecords will be
// sent back to the Browser via Mojo objects bound to |owner_task_runner|.
std::unique_ptr<media::MediaLog> RendererBlinkPlatformImpl::GetMediaLog(
    blink::MediaInspectorContext* inspector_context,
    scoped_refptr<base::SingleThreadTaskRunner> owner_task_runner,
    bool is_on_worker) {
  std::vector<std::unique_ptr<BatchingMediaLog::EventHandler>> handlers;

  // For chrome://media-internals.
  // This should only be created in the main Window context, and not from
  // a worker context.
  if (!is_on_worker)
    handlers.push_back(std::make_unique<RenderMediaEventHandler>(
        media::GetNextMediaPlayerLoggingID()));

  // For devtools' media tab.
  handlers.push_back(
      std::make_unique<InspectorMediaEventHandler>(inspector_context));

  return std::make_unique<BatchingMediaLog>(owner_task_runner,
                                            std::move(handlers));
}

//------------------------------------------------------------------------------
media::GpuVideoAcceleratorFactories*
RendererBlinkPlatformImpl::GetGpuFactories() {
  auto* render_thread = RenderThreadImpl::current();
  if (!render_thread)
    return nullptr;

  return render_thread->GetGpuFactories();
}

scoped_refptr<base::SequencedTaskRunner>
RendererBlinkPlatformImpl::MediaThreadTaskRunner() {
  auto* render_thread = RenderThreadImpl::current();
  if (!render_thread)
    return nullptr;

  return render_thread->GetMediaSequencedTaskRunner();
}

base::WeakPtr<media::DecoderFactory>
RendererBlinkPlatformImpl::GetMediaDecoderFactory() {
  blink::WebLocalFrame* const web_frame =
      blink::WebLocalFrame::FrameForCurrentContext();
  CHECK(web_frame);
  RenderFrameImpl* render_frame = RenderFrameImpl::FromWebFrame(web_frame);
  return render_frame->GetMediaDecoderFactory();
}

void RendererBlinkPlatformImpl::SetRenderingColorSpace(
    const gfx::ColorSpace& color_space) {
  auto* render_thread = RenderThreadImpl::current();
  if (!render_thread)
    return;

  render_thread->SetRenderingColorSpace(color_space);
}

gfx::ColorSpace RendererBlinkPlatformImpl::GetRenderingColorSpace() const {
  auto* render_thread = RenderThreadImpl::current();
  if (!render_thread)
    return {};

  return render_thread->GetRenderingColorSpace();
}

//------------------------------------------------------------------------------

void RendererBlinkPlatformImpl::SetActiveURL(const blink::WebURL& url,
                                             const blink::WebString& top_url) {
  GetContentClient()->SetActiveURL(url, top_url.Utf8());
}

//------------------------------------------------------------------------------

SkBitmap* RendererBlinkPlatformImpl::GetSadPageBitmap() {
  return GetContentClient()->renderer()->GetSadWebViewBitmap();
}

//------------------------------------------------------------------------------

std::unique_ptr<blink::WebV8ValueConverter>
RendererBlinkPlatformImpl::CreateWebV8ValueConverter() {
  return std::make_unique<V8ValueConverterImpl>();
}

void RendererBlinkPlatformImpl::AppendContentSecurityPolicy(
    const blink::WebURL& url,
    blink::WebVector<blink::WebContentSecurityPolicyHeader>* csp) {
  GetContentClient()->renderer()->AppendContentSecurityPolicy(url, csp);
}

base::PlatformThreadId RendererBlinkPlatformImpl::GetIOThreadId() const {
  auto io_task_runner = GetIOTaskRunner();
  if (!io_task_runner)
    return base::kInvalidThreadId;
  // Cannot be called from IO thread due to potential deadlock.
  CHECK(!io_task_runner->BelongsToCurrentThread());
  {
    base::ScopedAllowBaseSyncPrimitives allow;
    io_thread_id_ready_event_.Wait();
  }
  return io_thread_id_;
}

scoped_refptr<base::SingleThreadTaskRunner>
RendererBlinkPlatformImpl::VideoFrameCompositorTaskRunner() {
  auto compositor_task_runner = CompositorThreadTaskRunner();
  if (::features::UseSurfaceLayerForVideo() || !compositor_task_runner) {
    if (!video_frame_compositor_thread_) {
      // All of Chromium's GPU code must know which thread it's running on, and
      // be the same thread on which the rendering context was initialized. This
      // is why this must be a SingleThreadTaskRunner instead of a
      // SequencedTaskRunner.
      video_frame_compositor_thread_ =
          std::make_unique<base::Thread>("VideoFrameCompositor");
      video_frame_compositor_thread_->StartWithOptions(
          base::Thread::Options(base::ThreadType::kDisplayCritical));
    }

    return video_frame_compositor_thread_->task_runner();
  }
  return compositor_task_runner;
}

#if BUILDFLAG(IS_ANDROID)
void RendererBlinkPlatformImpl::SetPrivateMemoryFootprint(
    uint64_t private_memory_footprint_bytes) {
  auto* render_thread = RenderThreadImpl::current();
  CHECK(render_thread);
  render_thread->SetPrivateMemoryFootprint(private_memory_footprint_bytes);
}

bool RendererBlinkPlatformImpl::IsUserLevelMemoryPressureSignalEnabled() {
  return features::IsUserLevelMemoryPressureSignalEnabledOn3GbDevices() ||
         features::IsUserLevelMemoryPressureSignalEnabledOn4GbDevices() ||
         features::IsUserLevelMemoryPressureSignalEnabledOn6GbDevices();
}

std::pair<base::TimeDelta, base::TimeDelta> RendererBlinkPlatformImpl::
    InertAndMinimumIntervalOfUserLevelMemoryPressureSignal() {
  if (features::IsUserLevelMemoryPressureSignalEnabledOn3GbDevices()) {
    return std::make_pair(
        features::InertIntervalFor3GbDevices(),
        features::MinUserMemoryPressureIntervalOn3GbDevices());
  }
  if (features::IsUserLevelMemoryPressureSignalEnabledOn4GbDevices()) {
    return std::make_pair(
        features::InertIntervalFor4GbDevices(),
        features::MinUserMemoryPressureIntervalOn4GbDevices());
  }
  if (features::IsUserLevelMemoryPressureSignalEnabledOn6GbDevices()) {
    return std::make_pair(
        features::InertIntervalFor6GbDevices(),
        features::MinUserMemoryPressureIntervalOn6GbDevices());
  }

  constexpr std::pair<base::TimeDelta, base::TimeDelta>
      kDefaultInertAndMinInterval =
          std::make_pair(base::TimeDelta::Min(), base::Minutes(10));
  return kDefaultInertAndMinInterval;
}

#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace content
