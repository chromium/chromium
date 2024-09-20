// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/webengine/browser/frame_impl.h"

#include <fidl/fuchsia.logger/cpp/fidl.h>
#include <fidl/fuchsia.logger/cpp/hlcpp_conversion.h>
#include <fidl/fuchsia.media.sessions2/cpp/hlcpp_conversion.h>
#include <fidl/fuchsia.ui.views/cpp/hlcpp_conversion.h>
#include <fuchsia/ui/gfx/cpp/fidl.h>
#include <lib/fpromise/result.h>
#include <lib/sys/cpp/component_context.h>

#include <limits>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/fuchsia/fuchsia_component_connect.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/mem_buffer_util.h"
#include "base/fuchsia/process_context.h"
#include "base/functional/bind.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "base/task/single_thread_task_runner.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "build/chromecast_buildflags.h"
#include "content/public/browser/audio_stream_broker.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/host_zoom_map.h"
#include "content/public/browser/media_session.h"
#include "content/public/browser/message_port_provider.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/renderer_preferences_util.h"
#include "content/public/browser/scoped_accessibility_mode.h"
#include "content/public/browser/web_contents.h"
#include "fuchsia_web/webengine/browser/context_impl.h"
#include "fuchsia_web/webengine/browser/event_filter.h"
#include "fuchsia_web/webengine/browser/frame_layout_manager.h"
#include "fuchsia_web/webengine/browser/frame_window_tree_host.h"
#include "fuchsia_web/webengine/browser/media_player_impl.h"
#include "fuchsia_web/webengine/browser/message_port.h"
#include "fuchsia_web/webengine/browser/navigation_policy_handler.h"
#include "fuchsia_web/webengine/browser/trace_event.h"
#include "fuchsia_web/webengine/browser/url_request_rewrite_type_converters.h"
#include "fuchsia_web/webengine/browser/web_engine_devtools_controller.h"
#include "media/mojo/mojom/audio_processing.mojom.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "net/base/net_errors.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/logging/logging_utils.h"
#include "third_party/blink/public/common/messaging/web_message_port.h"
#include "third_party/blink/public/common/page/page_zoom.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"
#include "third_party/blink/public/mojom/navigation/was_activated_option.mojom.h"
#include "third_party/perfetto/include/perfetto/tracing/track_event_args.h"
#include "ui/aura/window.h"
#include "ui/compositor/compositor.h"
#include "ui/gfx/switches.h"
#include "ui/ozone/public/ozone_switches.h"
#include "ui/platform_window/fuchsia/view_ref_pair.h"
#include "ui/wm/core/base_focus_rules.h"
#include "url/gurl.h"
#include "url/origin.h"

#if BUILDFLAG(ENABLE_CAST_RECEIVER)
#include "components/cast_streaming/common/public/features.h"  //nogncheck
#include "components/cast_streaming/common/public/mojom/demuxer_connector.mojom.h"  //nogncheck
#include "components/cast_streaming/common/public/mojom/renderer_controller.mojom.h"  //nogncheck
#include "fuchsia_web/webengine/browser/receiver_session_client.h"  //nogncheck
#include "fuchsia_web/webengine/common/cast_streaming.h"            // nogncheck
#endif

namespace {

// Simulated screen bounds to use when headless rendering is enabled.
constexpr gfx::Size kHeadlessWindowSize = {1, 1};

// Name of the Inspect node that holds accessibility information.
constexpr char kAccessibilityInspectNodeName[] = "accessibility";

// A special value which matches all origins when specified in an origin list.
constexpr char kWildcardOrigin[] = "*";

// Used for attaching popup-related metadata to a WebContents.
constexpr char kPopupCreationInfo[] = "popup-creation-info";
class PopupFrameCreationInfoUserData : public base::SupportsUserData::Data {
 public:
  fuchsia::web::PopupFrameCreationInfo info;
};

class FrameFocusRules : public wm::BaseFocusRules {
 public:
  FrameFocusRules() = default;

  FrameFocusRules(const FrameFocusRules&) = delete;
  FrameFocusRules& operator=(const FrameFocusRules&) = delete;

  ~FrameFocusRules() override = default;

  // wm::BaseFocusRules implementation.
  bool SupportsChildActivation(const aura::Window*) const override;
};

bool FrameFocusRules::SupportsChildActivation(const aura::Window*) const {
  // TODO(crbug.com/40591214): Return a result based on window properties such
  // as visibility.
  return true;
}

// TODO(crbug.com/40710183): Use OnLoadScriptInjectorHost's origin matching
// code.
bool IsUrlMatchedByOriginList(const GURL& url,
                              const std::vector<std::string>& allowed_origins) {
  for (const std::string& origin : allowed_origins) {
    if (origin == kWildcardOrigin)
      return true;

    GURL origin_url(origin);
    if (!origin_url.is_valid()) {
      DLOG(WARNING)
          << "Ignored invalid origin spec when checking allowed list: "
          << origin;
      continue;
    }

    if (origin_url != url.DeprecatedGetOriginAsURL())
      continue;

    return true;
  }
  return false;
}

logging::LogSeverity FuchsiaWebConsoleLogLevelToLogSeverity(
    fuchsia::web::ConsoleLogLevel level) {
  switch (level) {
    case fuchsia::web::ConsoleLogLevel::DEBUG:
      return logging::LOGGING_VERBOSE;
    case fuchsia::web::ConsoleLogLevel::INFO:
      return logging::LOGGING_INFO;
    case fuchsia::web::ConsoleLogLevel::WARN:
      return logging::LOGGING_WARNING;
    case fuchsia::web::ConsoleLogLevel::ERROR:
      return logging::LOGGING_ERROR;
    case fuchsia::web::ConsoleLogLevel::NONE:
      return logging::LOGGING_NUM_SEVERITIES;
  }
}

logging::LogSeverity BlinkConsoleMessageLevelToLogSeverity(
    blink::mojom::ConsoleMessageLevel level) {
  switch (level) {
    case blink::mojom::ConsoleMessageLevel::kVerbose:
      return logging::LOGGING_VERBOSE;
    case blink::mojom::ConsoleMessageLevel::kInfo:
      return logging::LOGGING_INFO;
    case blink::mojom::ConsoleMessageLevel::kWarning:
      return logging::LOGGING_WARNING;
    case blink::mojom::ConsoleMessageLevel::kError:
      return logging::LOGGING_ERROR;
  }
}

bool IsHeadless() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kHeadless);
}

bool IsClonable(const fuchsia::web::CreateFrameParams& params) {
  fuchsia::web::CreateFrameParams cloned_params;
  return params.Clone(&cloned_params) == ZX_OK;
}

using FrameImplMap =
    base::small_map<std::map<content::WebContents*, FrameImpl*>>;

FrameImplMap& WebContentsToFrameImplMap() {
  static FrameImplMap frame_impl_map;
  return frame_impl_map;
}

blink::PermissionType FidlPermissionTypeToContentPermissionType(
    fuchsia::web::PermissionType fidl_type) {
  switch (fidl_type) {
    case fuchsia::web::PermissionType::MICROPHONE:
      return blink::PermissionType::AUDIO_CAPTURE;
    case fuchsia::web::PermissionType::CAMERA:
      return blink::PermissionType::VIDEO_CAPTURE;
    case fuchsia::web::PermissionType::PROTECTED_MEDIA_IDENTIFIER:
      return blink::PermissionType::PROTECTED_MEDIA_IDENTIFIER;
    case fuchsia::web::PermissionType::PERSISTENT_STORAGE:
      return blink::PermissionType::DURABLE_STORAGE;
  }
}

// Permission request callback for FrameImpl::RequestMediaAccessPermission.
void HandleMediaPermissionsRequestResult(
    const content::MediaStreamRequest& request,
    content::MediaResponseCallback callback,
    const std::vector<blink::mojom::PermissionStatus>& result) {
  // TODO(crbug.com/40216442): Generalize to multiple streams.
  blink::mojom::StreamDevicesPtr devices = blink::mojom::StreamDevices::New();

  int result_pos = 0;

  if (request.audio_type ==
      blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE) {
    if (result[result_pos] == blink::mojom::PermissionStatus::GRANTED) {
      devices->audio_device = blink::MediaStreamDevice(
          request.audio_type,
          request.requested_audio_device_ids.empty()
              ? ""
              : request.requested_audio_device_ids.front(),
          /*name=*/"");
    }
    result_pos++;
  }

  if (request.video_type ==
      blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE) {
    if (result[result_pos] == blink::mojom::PermissionStatus::GRANTED) {
      devices->video_device = blink::MediaStreamDevice(
          request.video_type,
          request.requested_video_device_ids.empty()
              ? ""
              : request.requested_video_device_ids.front(),
          /*name=*/"");
    }
  }

  blink::mojom::StreamDevicesSet stream_devices_set;
  if (devices->audio_device.has_value() || devices->video_device.has_value()) {
    stream_devices_set.stream_devices.emplace_back(std::move(devices));
  }
  std::move(callback).Run(
      stream_devices_set,
      stream_devices_set.stream_devices.empty()
          ? blink::mojom::MediaStreamRequestResult::NO_HARDWARE
          : blink::mojom::MediaStreamRequestResult::OK,
      nullptr);
}

std::optional<url::Origin> ParseAndValidateWebOrigin(
    const std::string& origin_str) {
  GURL origin_url(origin_str);
  if (!origin_url.username().empty() || !origin_url.password().empty() ||
      !origin_url.query().empty() || !origin_url.ref().empty()) {
    return std::nullopt;
  }

  if (!origin_url.path().empty() && origin_url.path() != "/")
    return std::nullopt;

  auto origin = url::Origin::Create(origin_url);
  if (origin.opaque())
    return std::nullopt;

  return origin;
}

int GetEffectFlagsForRenderUsage(fuchsia::media::AudioRenderUsage usage) {
  switch (usage) {
    case fuchsia::media::AudioRenderUsage::BACKGROUND:
      return media::AudioParameters::FUCHSIA_RENDER_USAGE_BACKGROUND;
    case fuchsia::media::AudioRenderUsage::MEDIA:
      return media::AudioParameters::FUCHSIA_RENDER_USAGE_MEDIA;
    case fuchsia::media::AudioRenderUsage::INTERRUPTION:
      return media::AudioParameters::FUCHSIA_RENDER_USAGE_INTERRUPTION;
    case fuchsia::media::AudioRenderUsage::SYSTEM_AGENT:
      return media::AudioParameters::FUCHSIA_RENDER_USAGE_SYSTEM_AGENT;
    case fuchsia::media::AudioRenderUsage::COMMUNICATION:
      return media::AudioParameters::FUCHSIA_RENDER_USAGE_COMMUNICATION;
  }
}

class AudioStreamBrokerFactory final
    : public content::AudioStreamBrokerFactory {
 public:
  AudioStreamBrokerFactory() : base_factory_(CreateImpl()) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  }
  ~AudioStreamBrokerFactory() final {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  }

  base::RepeatingCallback<void(fuchsia::media::AudioRenderUsage output_usage)>
  GetSetOutputUsagerCallback() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    return base::BindRepeating(
        AudioStreamBrokerFactory::SetOutputUsageOnUIThread,
        weak_factory_.GetWeakPtr());
  }

  // contents::AudioStreamBrokerFactory implementation.
  std::unique_ptr<content::AudioStreamBroker> CreateAudioInputStreamBroker(
      int render_process_id,
      int render_frame_id,
      const std::string& device_id,
      const media::AudioParameters& params,
      uint32_t shared_memory_count,
      media::UserInputMonitorBase* user_input_monitor,
      bool enable_agc,
      media::mojom::AudioProcessingConfigPtr processing_config,
      content::AudioStreamBroker::DeleterCallback deleter,
      mojo::PendingRemote<blink::mojom::RendererAudioInputStreamFactoryClient>
          renderer_factory_client) final {
    return base_factory_->CreateAudioInputStreamBroker(
        render_process_id, render_frame_id, device_id, params,
        shared_memory_count, user_input_monitor, enable_agc,
        std::move(processing_config), std::move(deleter),
        std::move(renderer_factory_client));
  }

  std::unique_ptr<content::AudioStreamBroker> CreateAudioLoopbackStreamBroker(
      int render_process_id,
      int render_frame_id,
      content::AudioStreamBroker::LoopbackSource* source,
      const media::AudioParameters& params,
      uint32_t shared_memory_count,
      bool mute_source,
      content::AudioStreamBroker::DeleterCallback deleter,
      mojo::PendingRemote<blink::mojom::RendererAudioInputStreamFactoryClient>
          renderer_factory_client) final {
    return base_factory_->CreateAudioLoopbackStreamBroker(
        render_process_id, render_frame_id, source, params, shared_memory_count,
        mute_source, std::move(deleter), std::move(renderer_factory_client));
  }

  std::unique_ptr<content::AudioStreamBroker> CreateAudioOutputStreamBroker(
      int render_process_id,
      int render_frame_id,
      int stream_id,
      const std::string& output_device_id,
      const media::AudioParameters& params,
      const base::UnguessableToken& group_id,
      content::AudioStreamBroker::DeleterCallback deleter,
      mojo::PendingRemote<media::mojom::AudioOutputStreamProviderClient> client)
      final {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
    media::AudioParameters params_with_effects = params;
    if (output_usage_) {
      params_with_effects.set_effects(
          params.effects() |
          GetEffectFlagsForRenderUsage(output_usage_.value()));
    }
    return base_factory_->CreateAudioOutputStreamBroker(
        render_process_id, render_frame_id, stream_id, output_device_id,
        params_with_effects, group_id, std::move(deleter), std::move(client));
  }

 private:
  static void SetOutputUsageOnUIThread(
      base::WeakPtr<AudioStreamBrokerFactory> factory,
      fuchsia::media::AudioRenderUsage output_usage) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    content::GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&AudioStreamBrokerFactory::SetOutputUsageOnIOThread,
                       factory, output_usage));
  }

  void SetOutputUsageOnIOThread(fuchsia::media::AudioRenderUsage output_usage) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
    output_usage_ = output_usage;
  }

  std::unique_ptr<content::AudioStreamBrokerFactory> base_factory_;
  std::optional<fuchsia::media::AudioRenderUsage> output_usage_;
  base::WeakPtrFactory<AudioStreamBrokerFactory> weak_factory_{this};
};

}  // namespace

FrameImpl::PendingPopup::PendingPopup(
    FrameImpl* frame_ptr,
    fidl::InterfaceHandle<fuchsia::web::Frame> handle,
    fuchsia::web::PopupFrameCreationInfo creation_info)
    : frame_ptr(std::move(frame_ptr)),
      handle(std::move(handle)),
      creation_info(std::move(creation_info)) {}
FrameImpl::PendingPopup::PendingPopup(PendingPopup&& other) = default;
FrameImpl::PendingPopup::~PendingPopup() = default;

// static
FrameImpl* FrameImpl::FromWebContents(content::WebContents* web_contents) {
  if (!web_contents)
    return nullptr;

  auto& map = WebContentsToFrameImplMap();
  auto it = map.find(web_contents);
  if (it == map.end())
    return nullptr;
  return it->second;
}

// static
FrameImpl* FrameImpl::FromRenderFrameHost(
    content::RenderFrameHost* render_frame_host) {
  return FromWebContents(
      content::WebContents::FromRenderFrameHost(render_frame_host));
}

FrameImpl::FrameImpl(std::unique_ptr<content::WebContents> web_contents,
                     ContextImpl* context,
                     fuchsia::web::CreateFrameParams params,
                     inspect::Node inspect_node,
                     fidl::InterfaceRequest<fuchsia::web::Frame> frame_request)
    : web_contents_(std::move(web_contents)),
      context_(context),
      console_log_tag_(params.has_debug_name() ? params.debug_name()
                                               : std::string()),
      params_for_popups_(std::move(params)),
      navigation_controller_(web_contents_.get(), this),
      permission_controller_(web_contents_.get()),
      binding_(this, std::move(frame_request)),
      media_blocker_(web_contents_.get()),
      theme_manager_(web_contents_.get(),
                     base::BindOnce(&FrameImpl::OnThemeManagerError,
                                    base::Unretained(this))),
      inspect_node_(std::move(inspect_node)),
      inspect_name_property_(
          params_for_popups_.has_debug_name()
              ? inspect_node_.CreateString("name",
                                           params_for_popups_.debug_name())
              : inspect::StringProperty()) {
  DCHECK(!WebContentsToFrameImplMap()[web_contents_.get()]);
  DCHECK(IsClonable(params));
  TRACE_EVENT(kWebEngineFidlCategory, "fuchsia.web/Frame created",
              perfetto::Flow::FromPointer(context_),
              perfetto::Flow::FromPointer(this));

  WebContentsToFrameImplMap()[web_contents_.get()] = this;

  web_contents_->SetDelegate(this);
  web_contents_->SetPageBaseBackgroundColor(SK_AlphaTRANSPARENT);
  Observe(web_contents_.get());

  url_request_rewrite_rules_manager_.AddWebContents(web_contents_.get());

  binding_.set_error_handler([this](zx_status_t status) {
    ZX_LOG_IF(ERROR, status != ZX_ERR_PEER_CLOSED, status)
        << " Frame disconnected.";
    context_->DestroyFrame(this);
  });

  content::UpdateFontRendererPreferencesFromSystemSettings(
      web_contents_->GetMutableRendererPrefs());
}

FrameImpl::~FrameImpl() {
  TRACE_EVENT(kWebEngineFidlCategory, "fuchsia.web/Frame destroyed",
              perfetto::TerminatingFlow::FromPointer(this));

  DestroyWindowTreeHost();
  context_->devtools_controller()->OnFrameDestroyed(web_contents_.get());

  auto& map = WebContentsToFrameImplMap();
  auto it = WebContentsToFrameImplMap().find(web_contents_.get());
  DCHECK(it != map.end() && it->second == this);
  map.erase(it);
}

void FrameImpl::EnableExplicitSitesFilter(std::string error_page) {
  explicit_sites_filter_error_page_ = std::move(error_page);
}

void FrameImpl::OverrideWebPreferences(
    blink::web_pref::WebPreferences* web_prefs) {
  if (content_area_settings_.has_hide_scrollbars()) {
    web_prefs->hide_scrollbars = content_area_settings_.hide_scrollbars();
  } else {
    // Verify that hide_scrollbars defaults to false, per FIDL API.
    DCHECK(!web_prefs->hide_scrollbars);
  }

  if (content_area_settings_.has_autoplay_policy()) {
    switch (content_area_settings_.autoplay_policy()) {
      case fuchsia::web::AutoplayPolicy::ALLOW:
        web_prefs->autoplay_policy =
            blink::mojom::AutoplayPolicy::kNoUserGestureRequired;
        break;
      case fuchsia::web::AutoplayPolicy::REQUIRE_USER_ACTIVATION:
        web_prefs->autoplay_policy =
            blink::mojom::AutoplayPolicy::kDocumentUserActivationRequired;
        break;
    }
  } else {
    // REQUIRE_USER_ACTIVATION is the default per the FIDL API.
    web_prefs->autoplay_policy =
        blink::mojom::AutoplayPolicy::kDocumentUserActivationRequired;
  }

  theme_manager_.ApplyThemeToWebPreferences(web_prefs);
}

zx::unowned_channel FrameImpl::GetBindingChannelForTest() const {
  return zx::unowned_channel(binding_.channel());
}

aura::Window* FrameImpl::root_window() const {
  return window_tree_host_->window();
}

void FrameImpl::ExecuteJavaScriptInternal(std::vector<std::string> origins,
                                          fuchsia::mem::Buffer script,
                                          ExecuteJavaScriptCallback callback,
                                          bool need_result) {
  if (!context_->IsJavaScriptInjectionAllowed()) {
    callback(fpromise::error(fuchsia::web::FrameError::INTERNAL_ERROR));
    return;
  }

  // Prevents script injection into the wrong document if the renderer recently
  // navigated to a different origin.
  if (!IsUrlMatchedByOriginList(web_contents_->GetLastCommittedURL(),
                                origins)) {
    callback(fpromise::error(fuchsia::web::FrameError::INVALID_ORIGIN));
    return;
  }

  std::optional<std::u16string> script_utf16 =
      base::ReadUTF8FromVMOAsUTF16(script);
  if (!script_utf16) {
    callback(fpromise::error(fuchsia::web::FrameError::BUFFER_NOT_UTF8));
    return;
  }

  content::RenderFrameHost::JavaScriptResultCallback result_callback;
  if (need_result) {
    result_callback = base::BindOnce(
        [](ExecuteJavaScriptCallback callback, base::Value result_value) {
          std::string result_json;
          if (!base::JSONWriter::Write(result_value, &result_json)) {
            callback(fpromise::error(fuchsia::web::FrameError::INTERNAL_ERROR));
            return;
          }

          callback(fpromise::ok(base::MemBufferFromString(
              std::move(result_json), "cr-execute-js-response")));
        },
        std::move(callback));
  }

  web_contents_->GetPrimaryMainFrame()->ExecuteJavaScript(
      *script_utf16, std::move(result_callback));

  if (!need_result) {
    // If no result is required then invoke callback() immediately.
    callback(fpromise::ok(fuchsia::mem::Buffer()));
  }
}

bool FrameImpl::IsWebContentsCreationOverridden(
    content::SiteInstance* source_site_instance,
    content::mojom::WindowContainerType window_container_type,
    const GURL& opener_url,
    const std::string& frame_name,
    const GURL& target_url) {
  // Specify a generous upper bound for unacknowledged popup windows, so that we
  // can catch bad client behavior while not interfering with normal operation.
  constexpr size_t kMaxPendingWebContentsCount = 10;

  if (!popup_listener_)
    return true;

  if (pending_popups_.size() >= kMaxPendingWebContentsCount) {
    // The content is producing popups faster than the embedder can process
    // them. Drop the popups so as to prevent resource exhaustion.
    LOG(WARNING) << "Too many pending popups, ignoring request.";

    // Don't produce a WebContents for this popup.
    return true;
  }

  return false;
}

content::WebContents* FrameImpl::AddNewContents(
    content::WebContents* source,
    std::unique_ptr<content::WebContents> new_contents,
    const GURL& target_url,
    WindowOpenDisposition disposition,
    const blink::mojom::WindowFeatures& window_features,
    bool user_gesture,
    bool* was_blocked) {
  DCHECK_EQ(source, web_contents_.get());

  // TODO(crbug.com/41476982): Add window disposition to the FIDL interface.
  switch (disposition) {
    case WindowOpenDisposition::NEW_FOREGROUND_TAB:
    case WindowOpenDisposition::NEW_BACKGROUND_TAB:
    case WindowOpenDisposition::NEW_PICTURE_IN_PICTURE:
    case WindowOpenDisposition::NEW_POPUP:
    case WindowOpenDisposition::NEW_WINDOW: {
      if (url_request_rewrite_rules_manager_.GetCachedRules()) {
        // There is no support for URL request rules rewriting with popups.
        *was_blocked = true;
        return nullptr;
      }

      auto* popup_creation_info =
          reinterpret_cast<PopupFrameCreationInfoUserData*>(
              new_contents->GetUserData(kPopupCreationInfo));
      fuchsia::web::PopupFrameCreationInfo popup_frame_creation_info =
          std::move(popup_creation_info->info);
      popup_frame_creation_info.set_initiated_by_user(user_gesture);
      // The PopupFrameCreationInfo won't be needed anymore, so clear it out.
      new_contents->SetUserData(kPopupCreationInfo, nullptr);

      // The constructor requires that the params can be cloned, so it cannot
      // fail here.
      fuchsia::web::CreateFrameParams params;
      zx_status_t status = params_for_popups_.Clone(&params);
      ZX_DCHECK(status == ZX_OK, status);
      fidl::InterfaceHandle<fuchsia::web::Frame> frame_handle;
      auto* popup_frame = context_->CreateFrameForWebContents(
          std::move(new_contents), std::move(params),
          frame_handle.NewRequest());

      fuchsia::web::ContentAreaSettings settings;
      status = content_area_settings_.Clone(&settings);
      ZX_DCHECK(status == ZX_OK, status);
      popup_frame->SetContentAreaSettings(std::move(settings));

      pending_popups_.emplace_back(popup_frame, std::move(frame_handle),
                                   std::move(popup_frame_creation_info));
      MaybeSendPopup();
      return nullptr;
    }

    // These kinds of windows don't produce Frames.
    case WindowOpenDisposition::CURRENT_TAB:
    case WindowOpenDisposition::SINGLETON_TAB:
    case WindowOpenDisposition::SAVE_TO_DISK:
    case WindowOpenDisposition::OFF_THE_RECORD:
    case WindowOpenDisposition::IGNORE_ACTION:
    case WindowOpenDisposition::SWITCH_TO_TAB:
    case WindowOpenDisposition::UNKNOWN:
      NOTIMPLEMENTED() << "Dropped new web contents (disposition: "
                       << static_cast<int>(disposition) << ")";
      return nullptr;
  }
}

void FrameImpl::WebContentsCreated(content::WebContents* source_contents,
                                   int opener_render_process_id,
                                   int opener_render_frame_id,
                                   const std::string& frame_name,
                                   const GURL& target_url,
                                   content::WebContents* new_contents) {
  auto creation_info = std::make_unique<PopupFrameCreationInfoUserData>();
  creation_info->info.set_initial_url(target_url.spec());
  new_contents->SetUserData(kPopupCreationInfo, std::move(creation_info));
}

void FrameImpl::MaybeSendPopup() {
  if (!popup_listener_)
    return;

  if (popup_ack_outstanding_ || pending_popups_.empty())
    return;

  auto popup = std::move(pending_popups_.front());
  pending_popups_.pop_front();

  popup_listener_->OnPopupFrameCreated(std::move(popup.handle),
                                       std::move(popup.creation_info), [this] {
                                         popup_ack_outstanding_ = false;
                                         MaybeSendPopup();
                                       });
  popup_ack_outstanding_ = true;
}

void FrameImpl::DestroyWindowTreeHost() {
  if (!window_tree_host_)
    return;

  aura::client::SetFocusClient(root_window(), nullptr);
  wm::SetActivationClient(root_window(), nullptr);
  root_window()->RemovePreTargetHandler(&event_filter_);
  root_window()->RemovePreTargetHandler(focus_controller_.get());
  web_contents_->GetNativeView()->Hide();
  window_tree_host_->Hide();
  window_tree_host_->compositor()->SetVisible(false);
  window_tree_host_.reset();
  accessibility_bridge_.reset();

  // Allows posted focus events to process before the FocusController is torn
  // down.
  content::GetUIThreadTaskRunner({})->DeleteSoon(FROM_HERE,
                                                 std::move(focus_controller_));
}

void FrameImpl::CloseAndDestroyFrame(zx_status_t error) {
  DCHECK(binding_.is_bound());
  binding_.Close(error);
  context_->DestroyFrame(this);
}

void FrameImpl::OnPopupListenerDisconnected(zx_status_t status) {
  ZX_LOG_IF(WARNING, status != ZX_ERR_PEER_CLOSED, status)
      << "Popup listener disconnected.";
  pending_popups_.clear();
}

void FrameImpl::OnMediaPlayerDisconnect() {
  media_player_ = nullptr;
}

bool FrameImpl::OnAccessibilityError(zx_status_t error) {
  // The task is posted so |accessibility_bridge_| does not tear |this| down
  // while events are still being processed.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&FrameImpl::CloseAndDestroyFrame,
                                weak_factory_.GetWeakPtr(), error));

  // The return value indicates to the accessibility bridge whether we should
  // attempt to reconnect. Since the frame has been destroyed, no reconnect
  // attempt should be made.
  return false;
}

#if BUILDFLAG(ENABLE_CAST_RECEIVER)
bool FrameImpl::MaybeHandleCastStreamingMessage(
    std::string* origin,
    fuchsia::web::WebMessage* message,
    PostMessageCallback* callback) {
  if (!context_->has_cast_streaming_enabled()) {
    return false;
  }

  if (!IsCastStreamingAppOrigin(*origin)) {
    return false;
  }

  if (receiver_session_client_ || !IsValidCastStreamingMessage(*message)) {
    // The Cast Streaming MessagePort should only be set once and |message|
    // should be a valid Cast Streaming Message.
    (*callback)(fpromise::error(fuchsia::web::FrameError::INVALID_ORIGIN));
    return true;
  }

  receiver_session_client_ = std::make_unique<ReceiverSessionClient>(
      std::move((*message->mutable_outgoing_transfer())[0].message_port()),
      IsCastStreamingVideoOnlyAppOrigin(*origin));
  (*callback)(fpromise::ok());
  return true;
}

void FrameImpl::MaybeStartCastStreaming(
    content::NavigationHandle* navigation_handle) {
  if (!context_->has_cast_streaming_enabled() || !receiver_session_client_ ||
      receiver_session_client_->HasReceiverSession()) {
    return;
  }

  mojo::AssociatedRemote<cast_streaming::mojom::DemuxerConnector>
      demuxer_connector;
  mojo::AssociatedRemote<cast_streaming::mojom::RendererController>
      renderer_controller;
  auto* remote_interfaces =
      navigation_handle->GetRenderFrameHost()->GetRemoteAssociatedInterfaces();
  remote_interfaces->GetInterface(&demuxer_connector);
  if (cast_streaming::IsCastRemotingEnabled()) {
    remote_interfaces->GetInterface(&renderer_controller);
  }
  receiver_session_client_->SetMojoEndpoints(std::move(demuxer_connector),
                                             std::move(renderer_controller));
}
#endif  // BUILDFLAG(ENABLE_CAST_RECEIVER)

void FrameImpl::UpdateRenderFrameZoomLevel(
    content::RenderFrameHost* render_frame_host) {
  float page_scale = content_area_settings_.has_page_scale()
                         ? content_area_settings_.page_scale()
                         : 1.0;
  content::HostZoomMap* host_zoom_map =
      content::HostZoomMap::GetForWebContents(web_contents_.get());
  host_zoom_map->SetTemporaryZoomLevel(
      render_frame_host->GetGlobalId(),
      blink::ZoomFactorToZoomLevel(page_scale));
}

void FrameImpl::ConnectToAccessibilityBridge() {
  // TODO(crbug.com/40212813): Replace callbacks with an interface that
  // FrameImpl implements.
  accessibility_bridge_ = std::make_unique<ui::AccessibilityBridgeFuchsiaImpl>(
      root_window(), fidl::HLCPPToNatural(window_tree_host_->CreateViewRef()),
      base::BindRepeating(&FrameImpl::SetAccessibilityEnabled,
                          base::Unretained(this)),
      base::BindRepeating(&FrameImpl::OnAccessibilityError,
                          base::Unretained(this)),
      inspect_node_.CreateChild(kAccessibilityInspectNodeName));
}

void FrameImpl::CreateView(fuchsia::ui::views::ViewToken view_token) {
  TRACE_EVENT(kWebEngineFidlCategory, "fuchsia.web/Frame.CreateView",
              perfetto::Flow::FromPointer(this));

  auto view_ref_pair = ui::ViewRefPair::New();
  CreateViewImpl(std::move(view_token), std::move(view_ref_pair.control_ref),
                 std::move(view_ref_pair.view_ref));
}

void FrameImpl::CreateViewWithViewRef(
    fuchsia::ui::views::ViewToken view_token,
    fuchsia::ui::views::ViewRefControl control_ref,
    fuchsia::ui::views::ViewRef view_ref) {
  TRACE_EVENT(kWebEngineFidlCategory, "fuchsia.web/Frame.CreateViewWithViewRef",
              perfetto::Flow::FromPointer(this));

  CreateViewImpl(std::move(view_token), std::move(control_ref),
                 std::move(view_ref));
}

void FrameImpl::CreateViewImpl(fuchsia::ui::views::ViewToken view_token,
                               fuchsia::ui::views::ViewRefControl control_ref,
                               fuchsia::ui::views::ViewRef view_ref) {
  if (IsHeadless()) {
    LOG(WARNING) << "CreateView() called on a HEADLESS Context.";
    CloseAndDestroyFrame(ZX_ERR_INVALID_ARGS);
    return;
  }

  if (!view_token.value.is_valid()) {
    LOG(WARNING) << "CreateView() called with invalid view_token.";
    CloseAndDestroyFrame(ZX_ERR_INVALID_ARGS);
    return;
  }

  // If a View to this Frame is already active then disconnect it.
  DestroyWindowTreeHost();

  ui::ViewRefPair view_ref_pair;
  view_ref_pair.control_ref = std::move(control_ref);
  view_ref_pair.view_ref = std::move(view_ref);
  SetupWindowTreeHost(std::move(view_token), std::move(view_ref_pair));

  ConnectToAccessibilityBridge();
}

void FrameImpl::CreateView2(fuchsia::web::CreateView2Args view_args) {
  TRACE_EVENT(kWebEngineFidlCategory, "fuchsia.web/Frame.CreateView2",
              perfetto::Flow::FromPointer(this));

  if (IsHeadless()) {
    LOG(WARNING) << "CreateView2() called on a HEADLESS Context.";
    CloseAndDestroyFrame(ZX_ERR_INVALID_ARGS);
    return;
  }

  if (!view_args.has_view_creation_token() ||
      !view_args.view_creation_token().value.is_valid()) {
    LOG(WARNING) << "CreateView2() called with invalid view_creation_token.";
    CloseAndDestroyFrame(ZX_ERR_INVALID_ARGS);
    return;
  }

  // If a View to this Frame is already active then disconnect it.
  DestroyWindowTreeHost();

  auto view_ref_pair = ui::ViewRefPair::New();
  SetupWindowTreeHost(std::move(*view_args.mutable_view_creation_token()),
                      std::move(view_ref_pair));

  ConnectToAccessibilityBridge();
}

void FrameImpl::GetMediaPlayer(
    fidl::InterfaceRequest<fuchsia::media::sessions2::Player> player) {
  TRACE_EVENT(kWebEngineFidlCategory, "fuchsia.web/Frame.GetMediaPlayer",
              perfetto::Flow::FromPointer(this));

  media_player_ = std::make_unique<MediaPlayerImpl>(
      content::MediaSession::Get(web_contents_.get()),
      fidl::HLCPPToNatural(player),
      base::BindOnce(&FrameImpl::OnMediaPlayerDisconnect,
                     base::Unretained(this)));
}

void FrameImpl::GetNavigationController(
    fidl::InterfaceRequest<fuchsia::web::NavigationController> controller) {
  TRACE_EVENT(kWebEngineFidlCategory,
              "fuchsia.web/Frame.GetNavigationController",
              perfetto::Flow::FromPointer(this));

  navigation_controller_.AddBinding(std::move(controller));
}

void FrameImpl::ExecuteJavaScript(std::vector<std::string> origins,
                                  fuchsia::mem::Buffer script,
                                  ExecuteJavaScriptCallback callback) {
  TRACE_EVENT(kWebEngineFidlCategory, "fuchsia.web/Frame.ExecuteJavaScript",
              perfetto::Flow::FromPointer(this));

  ExecuteJavaScriptInternal(std::move(origins), std::move(script),
                            std::move(callback), true);
}

void FrameImpl::ExecuteJavaScriptNoResult(
    std::vector<std::string> origins,
    fuchsia::mem::Buffer script,
    ExecuteJavaScriptNoResultCallback callback) {
  TRACE_EVENT(kWebEngineFidlCategory,
              "fuchsia.web/Frame.ExecuteJavaScriptNoResult",
              perfetto::Flow::FromPointer(this));

  ExecuteJavaScriptInternal(
      std::move(origins), std::move(script),
      [callback = std::move(callback)](
          fuchsia::web::Frame_ExecuteJavaScript_Result result_with_value) {
        if (result_with_value.is_err()) {
          callback(fpromise::error(result_with_value.err()));
        } else {
          callback(fpromise::ok());
        }
      },
      false);
}

void FrameImpl::AddBeforeLoadJavaScript(
    uint64_t id,
    std::vector<std::string> origins,
    fuchsia::mem::Buffer script,
    AddBeforeLoadJavaScriptCallback callback) {
  TRACE_EVENT(kWebEngineFidlCategory,
              "fuchsia.web/Frame.AddBeforeLoadJavaScript",
              perfetto::Flow::FromPointer(this));

  if (!context_->IsJavaScriptInjectionAllowed()) {
    callback(fpromise::error(fuchsia::web::FrameError::INTERNAL_ERROR));
    return;
  }

  std::optional<std::string> script_as_string =
      base::StringFromMemBuffer(script);
  if (!script_as_string) {
    LOG(ERROR) << "Couldn't read script from buffer.";
    callback(fpromise::error(fuchsia::web::FrameError::INTERNAL_ERROR));
    return;
  }

  // TODO(crbug.com/40707541): Only allow wildcards to be specified standalone.
  if (base::Contains(origins, kWildcardOrigin)) {
    script_injector_.AddScriptForAllOrigins(id, *script_as_string);
  } else {
    std::vector<url::Origin> origins_converted;
    for (const std::string& origin : origins) {
      url::Origin origin_parsed = url::Origin::Create(GURL(origin));
      if (origin_parsed.opaque()) {
        callback(fpromise::error(fuchsia::web::FrameError::INVALID_ORIGIN));
        return;
      }
      origins_converted.push_back(origin_parsed);
    }

    script_injector_.AddScript(id, origins_converted, *script_as_string);
  }

  callback(fpromise::ok());
}

void FrameImpl::RemoveBeforeLoadJavaScript(uint64_t id) {
  TRACE_EVENT(kWebEngineFidlCategory,
              "fuchsia.web/Frame.RemoveBeforeLoadJavaScript",
              perfetto::Flow::FromPointer(this));

  script_injector_.RemoveScript(id);
}

void FrameImpl::PostMessage(std::string origin,
                            fuchsia::web::WebMessage message,
                            PostMessageCallback callback) {
  TRACE_EVENT(kWebEngineFidlCategory, "fuchsia.web/Frame.PostMessage",
              perfetto::Flow::FromPointer(this));

#if BUILDFLAG(ENABLE_CAST_RECEIVER)
  if (MaybeHandleCastStreamingMessage(&origin, &message, &callback))
    return;
#endif

  fuchsia::web::Frame_PostMessage_Result result;
  if (origin.empty()) {
    callback(fpromise::error(fuchsia::web::FrameError::INVALID_ORIGIN));
    return;
  }

  if (!message.has_data()) {
    callback(fpromise::error(fuchsia::web::FrameError::NO_DATA_IN_MESSAGE));
    return;
  }

  std::optional<std::u16string> origin_utf16;
  if (origin != kWildcardOrigin)
    origin_utf16 = base::UTF8ToUTF16(origin);

  std::optional<std::u16string> data_utf16 =
      base::ReadUTF8FromVMOAsUTF16(message.data());
  if (!data_utf16) {
    callback(fpromise::error(fuchsia::web::FrameError::BUFFER_NOT_UTF8));
    return;
  }

  // Convert and pass along any MessagePorts contained in the message.
  std::vector<blink::WebMessagePort> message_ports;
  if (message.has_outgoing_transfer()) {
    for (const fuchsia::web::OutgoingTransferable& outgoing :
         message.outgoing_transfer()) {
      if (!outgoing.is_message_port()) {
        callback(fpromise::error(fuchsia::web::FrameError::INTERNAL_ERROR));
        return;
      }
    }

    for (fuchsia::web::OutgoingTransferable& outgoing :
         *message.mutable_outgoing_transfer()) {
      blink::WebMessagePort blink_port =
          BlinkMessagePortFromFidl(std::move(outgoing.message_port()));
      if (!blink_port.IsValid()) {
        callback(fpromise::error(fuchsia::web::FrameError::INTERNAL_ERROR));
        return;
      }
      message_ports.push_back(std::move(blink_port));
    }
  }

  content::MessagePortProvider::PostMessageToFrame(
      web_contents_->GetPrimaryPage(), std::u16string(), origin_utf16,
      std::move(*data_utf16), std::move(message_ports));
  callback(fpromise::ok());
}

void FrameImpl::SetNavigationEventListener(
    fidl::InterfaceHandle<fuchsia::web::NavigationEventListener> listener) {
  SetNavigationEventListener2(std::move(listener), /*flags=*/{});
}

void FrameImpl::SetNavigationEventListener2(
    fidl::InterfaceHandle<fuchsia::web::NavigationEventListener> listener,
    fuchsia::web::NavigationEventListenerFlags flags) {
  TRACE_EVENT(kWebEngineFidlCategory,
              "fuchsia.web/Frame.SetNavigationEventListener",
              perfetto::Flow::FromPointer(this));

  navigation_controller_.SetEventListener(std::move(listener), flags);
}

void FrameImpl::SetJavaScriptLogLevel(fuchsia::web::ConsoleLogLevel level) {
  TRACE_EVENT(kWebEngineFidlCategory, "fuchsia.web/Frame.SetJavaScriptLogLevel",
              perfetto::Flow::FromPointer(this));

  log_level_ = FuchsiaWebConsoleLogLevelToLogSeverity(level);
}

void FrameImpl::SetConsoleLogSink(fuchsia::logger::LogSinkHandle sink) {
  TRACE_EVENT(kWebEngineFidlCategory, "fuchsia.web/Frame.SetConsoleLogSink",
              perfetto::Flow::FromPointer(this));

  if (sink) {
    console_logger_ = base::ScopedFxLogger::CreateFromLogSink(
        fidl::HLCPPToNatural(sink), {console_log_tag_});
  } else {
    console_logger_ = {};
  }
}

void FrameImpl::ConfigureInputTypes(fuchsia::web::InputTypes types,
                                    fuchsia::web::AllowInputState allow) {
  TRACE_EVENT(kWebEngineFidlCategory, "fuchsia.web/Frame.ConfigureInputTypes",
              perfetto::Flow::FromPointer(this));

  event_filter_.ConfigureInputTypes(types, allow);
}

void FrameImpl::SetPopupFrameCreationListener(
    fidl::InterfaceHandle<fuchsia::web::PopupFrameCreationListener> listener) {
  TRACE_EVENT(kWebEngineFidlCategory,
              "fuchsia.web/Frame.SetPopupFrameCreationListener",
              perfetto::Flow::FromPointer(this));

  popup_listener_ = listener.Bind();
  popup_listener_.set_error_handler(
      fit::bind_member(this, &FrameImpl::OnPopupListenerDisconnected));
}

void FrameImpl::SetUrlRequestRewriteRules(
    std::vector<fuchsia::web::UrlRequestRewriteRule> rules,
    SetUrlRequestRewriteRulesCallback callback) {
  TRACE_EVENT(kWebEngineFidlCategory,
              "fuchsia.web/Frame.SetUrlRequestRewriteRules",
              perfetto::Flow::FromPointer(this));

  auto mojom_rules =
      mojo::ConvertTo<url_rewrite::mojom::UrlRequestRewriteRulesPtr>(
          std::move(rules));
  if (url_request_rewrite_rules_manager_.OnRulesUpdated(
          std::move(mojom_rules))) {
    std::move(callback)();
  } else {
    CloseAndDestroyFrame(ZX_ERR_INVALID_ARGS);
  }
}

void FrameImpl::EnableHeadlessRendering() {
  TRACE_EVENT(kWebEngineFidlCategory,
              "fuchsia.web/Frame.EnableHeadlessRendering",
              perfetto::Flow::FromPointer(this));

  if (!IsHeadless()) {
    LOG(ERROR) << "EnableHeadlessRendering() on non-HEADLESS Context.";
    CloseAndDestroyFrame(ZX_ERR_INVALID_ARGS);
    return;
  }

  auto view_ref_pair = ui::ViewRefPair::New();
  SetupWindowTreeHost(fuchsia::ui::views::ViewToken(),
                      std::move(view_ref_pair));

  gfx::Rect bounds(kHeadlessWindowSize);

  if (window_size_for_test_) {
    ConnectToAccessibilityBridge();
    bounds.set_size(*window_size_for_test_);
  }

  window_tree_host_->SetBoundsInPixels(bounds);

  // FrameWindowTreeHost will Show() itself when the View is attached, but
  // in headless mode there is no View, so Show() it explicitly.
  window_tree_host_->Show();
}

void FrameImpl::DisableHeadlessRendering() {
  TRACE_EVENT(kWebEngineFidlCategory,
              "fuchsia.web/Frame.DisableHeadlessRendering",
              perfetto::Flow::FromPointer(this));

  if (!IsHeadless()) {
    LOG(ERROR)
        << "Attempted to disable headless rendering on non-HEADLESS Context.";
    CloseAndDestroyFrame(ZX_ERR_INVALID_ARGS);
    return;
  }

  DestroyWindowTreeHost();
}

void FrameImpl::SetupWindowTreeHost(fuchsia::ui::views::ViewToken view_token,
                                    ui::ViewRefPair view_ref_pair) {
  DCHECK(!window_tree_host_);

  window_tree_host_ = std::make_unique<FrameWindowTreeHost>(
      std::move(view_token), std::move(view_ref_pair), web_contents_.get(),
      base::BindRepeating(&FrameImpl::OnPixelScaleUpdate,
                          base::Unretained(this)));

  InitWindowTreeHost();
}

void FrameImpl::SetupWindowTreeHost(
    fuchsia::ui::views::ViewCreationToken view_creation_token,
    ui::ViewRefPair view_ref_pair) {
  DCHECK(!window_tree_host_);

  window_tree_host_ = std::make_unique<FrameWindowTreeHost>(
      std::move(view_creation_token), std::move(view_ref_pair),
      web_contents_.get(),
      base::BindRepeating(&FrameImpl::OnPixelScaleUpdate,
                          base::Unretained(this)));

  InitWindowTreeHost();
}

void FrameImpl::InitWindowTreeHost() {
  DCHECK(window_tree_host_);

  window_tree_host_->InitHost();
  root_window()->AddPreTargetHandler(&event_filter_);

  // Add hooks which automatically set the focus state when input events are
  // received.
  focus_controller_ =
      std::make_unique<wm::FocusController>(new FrameFocusRules);
  root_window()->AddPreTargetHandler(focus_controller_.get());
  aura::client::SetFocusClient(root_window(), focus_controller_.get());

  wm::SetActivationClient(root_window(), focus_controller_.get());

  layout_manager_ =
      root_window()->SetLayoutManager(std::make_unique<FrameLayoutManager>());
  if (!render_size_override_.IsEmpty())
    layout_manager_->ForceContentDimensions(render_size_override_);

  root_window()->AddChild(web_contents_->GetNativeView());
  web_contents_->GetNativeView()->Show();

  // FrameWindowTreeHost will Show() itself when the View is actually attached
  // to the view-tree to be displayed. See https://crbug.com/1109270
}

void FrameImpl::SetMediaSettings(
    fuchsia::web::FrameMediaSettings media_settings) {
  TRACE_EVENT(kWebEngineFidlCategory, "fuchsia.web/Frame.SetMediaSettings",
              perfetto::Flow::FromPointer(this));

  media_settings_ = std::move(media_settings);
  if (media_settings.has_renderer_usage() && set_audio_output_usage_callback_)
    set_audio_output_usage_callback_.Run(media_settings.renderer_usage());
}

void FrameImpl::ForceContentDimensions(
    std::unique_ptr<fuchsia::ui::gfx::vec2> web_dips) {
  TRACE_EVENT(kWebEngineFidlCategory,
              "fuchsia.web/Frame.ForceContentDimensions",
              perfetto::Flow::FromPointer(this));

  if (!web_dips) {
    render_size_override_ = {};
    if (layout_manager_)
      layout_manager_->ForceContentDimensions({});
    return;
  }

  gfx::Size web_dips_converted(web_dips->x, web_dips->y);
  if (web_dips_converted.IsEmpty()) {
    LOG(ERROR) << "Rejecting zero-area size for ForceContentDimensions().";
    CloseAndDestroyFrame(ZX_ERR_INVALID_ARGS);
    return;
  }

  render_size_override_ = web_dips_converted;
  if (layout_manager_)
    layout_manager_->ForceContentDimensions(web_dips_converted);
}

void FrameImpl::SetPermissionState(
    fuchsia::web::PermissionDescriptor fidl_permission,
    std::string web_origin_string,
    fuchsia::web::PermissionState fidl_state) {
  TRACE_EVENT(kWebEngineFidlCategory, "fuchsia.web/Frame.SetPermissionState",
              perfetto::Flow::FromPointer(this));

  if (!fidl_permission.has_type()) {
    LOG(ERROR) << "PermissionDescriptor.type is not specified in "
                  "SetPermissionState().";
    CloseAndDestroyFrame(ZX_ERR_INVALID_ARGS);
    return;
  }

  blink::PermissionType type =
      FidlPermissionTypeToContentPermissionType(fidl_permission.type());

  blink::mojom::PermissionStatus state =
      (fidl_state == fuchsia::web::PermissionState::GRANTED)
          ? blink::mojom::PermissionStatus::GRANTED
          : blink::mojom::PermissionStatus::DENIED;

  // TODO(crbug.com/40724536): Remove this once the PermissionManager API is
  // available.
  if (web_origin_string == "*" &&
      type == blink::PermissionType::PROTECTED_MEDIA_IDENTIFIER) {
    permission_controller_.SetDefaultPermissionState(type, state);
    return;
  }

  // Handle per-origin permissions specifications.
  auto web_origin = ParseAndValidateWebOrigin(web_origin_string);
  if (!web_origin) {
    LOG(ERROR) << "SetPermissionState() called with invalid web_origin: "
               << web_origin_string;
    CloseAndDestroyFrame(ZX_ERR_INVALID_ARGS);
    return;
  }

  permission_controller_.SetPermissionState(type, web_origin.value(), state);
}

void FrameImpl::GetPrivateMemorySize(GetPrivateMemorySizeCallback callback) {
  TRACE_EVENT(kWebEngineFidlCategory, "fuchsia.web/Frame.GetPrivateMemorySize",
              perfetto::Flow::FromPointer(this));

  if (!web_contents_->GetPrimaryMainFrame()->GetProcess()->IsReady()) {
    // Renderer process is not yet started.
    callback(0);
    return;
  }

  zx_info_task_stats_t task_stats;
  zx_status_t status = zx_object_get_info(
      web_contents_->GetPrimaryMainFrame()->GetProcess()->GetProcess().Handle(),
      ZX_INFO_TASK_STATS, &task_stats, sizeof(task_stats), nullptr, nullptr);

  if (status != ZX_OK) {
    // Fail gracefully by returning zero.
    ZX_LOG(WARNING, status) << "zx_object_get_info(ZX_INFO_TASK_STATS)";
    callback(0);
    return;
  }

  callback(task_stats.mem_private_bytes);
}

void FrameImpl::SetNavigationPolicyProvider(
    fuchsia::web::NavigationPolicyProviderParams params,
    fidl::InterfaceHandle<fuchsia::web::NavigationPolicyProvider> provider) {
  TRACE_EVENT(kWebEngineFidlCategory,
              "fuchsia.web/Frame.SetNavigationPolicyProvider",
              perfetto::Flow::FromPointer(this));

  navigation_policy_handler_ = std::make_unique<NavigationPolicyHandler>(
      std::move(params), std::move(provider));
}

void FrameImpl::SetContentAreaSettings(
    fuchsia::web::ContentAreaSettings settings) {
  TRACE_EVENT(kWebEngineFidlCategory,
              "fuchsia.web/Frame.SetContentAreaSettings",
              perfetto::Flow::FromPointer(this));

  if (settings.has_hide_scrollbars())
    content_area_settings_.set_hide_scrollbars(settings.hide_scrollbars());
  if (settings.has_autoplay_policy())
    content_area_settings_.set_autoplay_policy(settings.autoplay_policy());
  if (settings.has_theme()) {
    content_area_settings_.set_theme(settings.theme());
    theme_manager_.SetTheme(settings.theme());
  }
  if (settings.has_page_scale()) {
    if (settings.page_scale() <= 0.0) {
      LOG(ERROR) << "SetPageScale() called with nonpositive scale.";
      CloseAndDestroyFrame(ZX_ERR_INVALID_ARGS);
      return;
    }
    if (!(content_area_settings_.has_page_scale() &&
          (settings.page_scale() == content_area_settings_.page_scale()))) {
      content_area_settings_.set_page_scale(settings.page_scale());
      UpdateRenderFrameZoomLevel(web_contents_->GetPrimaryMainFrame());
    }
  }

  web_contents_->OnWebPreferencesChanged();
}

void FrameImpl::ResetContentAreaSettings() {
  TRACE_EVENT(kWebEngineFidlCategory,
              "fuchsia.web/Frame.ResetContentAreaSettings",
              perfetto::Flow::FromPointer(this));

  content_area_settings_ = fuchsia::web::ContentAreaSettings();
  web_contents_->OnWebPreferencesChanged();
  UpdateRenderFrameZoomLevel(web_contents_->GetPrimaryMainFrame());
}

void FrameImpl::Close(fuchsia::web::FrameCloseRequest request) {
  // By default allow a couple of seconds in case the page content needs to
  // e.g. collate metrics and send them to the network.
  constexpr auto kDefaultFrameCloseTimeout = base::Seconds(2u);

  auto timeout = request.has_timeout()
                     ? base::TimeDelta::FromZxDuration(request.timeout())
                     : kDefaultFrameCloseTimeout;

  // If the content does not need any handlers to be run, or a zero timeout was
  // specified, then teardown the content immediately and close.
  if (!web_contents_->NeedToFireBeforeUnloadOrUnloadEvents() ||
      timeout.is_zero()) {
    CloseAndDestroyFrame(ZX_OK);
    return;
  }

  // Request that `web_contents_` allow the page to gracefully teardown:
  // - Destroy the WindowTreeHost, causing the page to receive "pagehide" and
  //   "visibilitychange" events.
  // - Fire the "beforeunload" event, ignoring the result.
  // - Fire the "onunload" event, and teardown the page if that completes.
  DestroyWindowTreeHost();
  web_contents_->DispatchBeforeUnload(false /* auto_cancel */);
  web_contents_->ClosePage();

  // (Re-)start the teardown timeout. If the page closes before this timer
  // fires then `CloseContents()` will be invoked, causing the `Frame` to be
  // closed with `ZX_OK`.
  close_page_timeout_.Start(
      FROM_HERE, timeout,
      base::BindOnce(&FrameImpl::CloseAndDestroyFrame, base::Unretained(this),
                     ZX_ERR_TIMED_OUT));
}

void FrameImpl::CloseContents(content::WebContents* source) {
  DCHECK_EQ(source, web_contents_.get());
  CloseAndDestroyFrame(ZX_OK);
}

void FrameImpl::SetBlockMediaLoading(bool blocked) {
  TRACE_EVENT(kWebEngineFidlCategory, "fuchsia.web/Frame.SetBlockMediaLoading",
              perfetto::Flow::FromPointer(this));

  media_blocker_.BlockMediaLoading(blocked);
}

bool FrameImpl::DidAddMessageToConsole(
    content::WebContents* source,
    blink::mojom::ConsoleMessageLevel log_level,
    const std::u16string& message,
    int32_t line_no,
    const std::u16string& source_id) {
  // Assert that log severities are strictly ascending, before using numerical
  // comparison to determine whether to emit a log.
  static_assert(logging::LOGGING_VERBOSE < logging::LOGGING_INFO);
  static_assert(logging::LOGGING_INFO < logging::LOGGING_WARNING);
  static_assert(logging::LOGGING_WARNING < logging::LOGGING_ERROR);
  static_assert(logging::LOGGING_ERROR < logging::LOGGING_NUM_SEVERITIES);

  logging::LogSeverity severity =
      BlinkConsoleMessageLevelToLogSeverity(log_level);
  if (severity < log_level_) {
    // Prevent the default logging mechanism from logging the message.
    return true;
  }

  if (!console_logger_.is_valid()) {
    // Log via the process' LogSink service if none was set on the Frame.
    // Connect on-demand, so that embedders need not provide a LogSink in the
    // CreateContextParams services, unless they actually enable logging.
    auto log_sink_client_end =
        base::fuchsia_component::Connect<fuchsia_logger::LogSink>();
    if (log_sink_client_end.is_error()) {
      DLOG(ERROR) << base::FidlConnectionErrorMessage(log_sink_client_end);
      return false;
    }
    console_logger_ = base::ScopedFxLogger::CreateFromLogSink(
        std::move(log_sink_client_end.value()), {console_log_tag_});

    if (!console_logger_.is_valid())
      return false;
  }

  std::string source_id_utf8 = base::UTF16ToUTF8(source_id);
  std::string message_utf8 = base::UTF16ToUTF8(message);
  console_logger_.LogMessage(source_id_utf8, line_no, message_utf8, severity);

  return true;
}

void FrameImpl::RequestMediaAccessPermission(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    content::MediaResponseCallback callback) {
  DCHECK_EQ(web_contents_.get(), web_contents);

  std::vector<blink::PermissionType> permissions;

  if (request.audio_type ==
      blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE) {
    permissions.push_back(blink::PermissionType::AUDIO_CAPTURE);
  } else if (request.audio_type != blink::mojom::MediaStreamType::NO_SERVICE) {
    std::move(callback).Run(
        blink::mojom::StreamDevicesSet(),
        blink::mojom::MediaStreamRequestResult::NOT_SUPPORTED, nullptr);
    return;
  }

  if (request.video_type ==
      blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE) {
    permissions.push_back(blink::PermissionType::VIDEO_CAPTURE);
  } else if (request.video_type != blink::mojom::MediaStreamType::NO_SERVICE) {
    std::move(callback).Run(
        blink::mojom::StreamDevicesSet(),
        blink::mojom::MediaStreamRequestResult::NOT_SUPPORTED, nullptr);
    return;
  }

  auto* render_frame_host = content::RenderFrameHost::FromID(
      request.render_process_id, request.render_frame_id);
  if (!render_frame_host) {
    std::move(callback).Run(
        blink::mojom::StreamDevicesSet(),
        blink::mojom::MediaStreamRequestResult::INVALID_STATE, nullptr);
    return;
  }

  if (url::Origin::Create(request.security_origin) !=
      render_frame_host->GetLastCommittedOrigin()) {
    std::move(callback).Run(
        blink::mojom::StreamDevicesSet(),
        blink::mojom::MediaStreamRequestResult::INVALID_SECURITY_ORIGIN,
        nullptr);
    return;
  }

  content::PermissionController* permission_controller =
      web_contents_->GetBrowserContext()->GetPermissionController();
  DCHECK(permission_controller);

  permission_controller->RequestPermissionsFromCurrentDocument(
      render_frame_host,
      content::PermissionRequestDescription(permissions, request.user_gesture),
      base::BindOnce(&HandleMediaPermissionsRequestResult, request,
                     std::move(callback)));
}

bool FrameImpl::CheckMediaAccessPermission(
    content::RenderFrameHost* render_frame_host,
    const url::Origin& security_origin,
    blink::mojom::MediaStreamType type) {
  blink::PermissionType permission;
  switch (type) {
    case blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE:
      permission = blink::PermissionType::AUDIO_CAPTURE;
      break;
    case blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE:
      permission = blink::PermissionType::VIDEO_CAPTURE;
      break;
    default:
      NOTREACHED();
  }

  // TODO(crbug.com/40223767): Remove `security_origin`.
  if (security_origin != render_frame_host->GetLastCommittedOrigin()) {
    return false;
  }

  content::PermissionController* permission_controller =
      web_contents_->GetBrowserContext()->GetPermissionController();
  DCHECK(permission_controller);

  return permission_controller->GetPermissionStatusForCurrentDocument(
             permission, render_frame_host) ==
         blink::mojom::PermissionStatus::GRANTED;
}

std::unique_ptr<content::AudioStreamBrokerFactory>
FrameImpl::CreateAudioStreamBrokerFactory(content::WebContents* web_contents) {
  DCHECK_EQ(web_contents, web_contents_.get());

  auto result = std::make_unique<AudioStreamBrokerFactory>();

  // Save callback to use to pass renderer usage to the factory in the future.
  set_audio_output_usage_callback_ = result->GetSetOutputUsagerCallback();
  if (media_settings_.has_renderer_usage())
    set_audio_output_usage_callback_.Run(media_settings_.renderer_usage());

  return result;
}

bool FrameImpl::CanOverscrollContent() {
  // Don't process "overscroll" events (e.g. pull-to-refresh, swipe back,
  // swipe forward).
  // TODO(crbug.com/40748448): Add overscroll toggle to Frame API.
  return false;
}

void FrameImpl::ReadyToCommitNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      navigation_handle->IsSameDocument() || navigation_handle->IsErrorPage()) {
    return;
  }

  script_injector_.InjectScriptsForURL(navigation_handle->GetURL(),
                                       navigation_handle->GetRenderFrameHost());

#if BUILDFLAG(ENABLE_CAST_RECEIVER)
  MaybeStartCastStreaming(navigation_handle);
#endif
}

void FrameImpl::DidFinishLoad(content::RenderFrameHost* render_frame_host,
                              const GURL& validated_url) {
  context_->devtools_controller()->OnFrameLoaded(web_contents_.get());
}

void FrameImpl::RenderFrameCreated(content::RenderFrameHost* frame_host) {
  // The top-level frame is given a transparent background color.
  // GetView() is guaranteed to be non-null until |frame_host| teardown.
  if (!frame_host->GetParentOrOuterDocument()) {
    frame_host->GetView()->SetBackgroundColor(SK_AlphaTRANSPARENT);
  }
}

void FrameImpl::RenderFrameHostChanged(content::RenderFrameHost* old_host,
                                       content::RenderFrameHost* new_host) {
  // UpdateRenderFrameZoomLevel() sets temporary zoom level for the current
  // RenderFrame. It needs to be called again whenever main RenderFrame is
  // changed.
  if (new_host->IsInPrimaryMainFrame())
    UpdateRenderFrameZoomLevel(new_host);
}

void FrameImpl::DidFirstVisuallyNonEmptyPaint() {
  base::RecordComputedAction("AppFirstPaint");
}

void FrameImpl::ResourceLoadComplete(
    content::RenderFrameHost* render_frame_host,
    const content::GlobalRequestID& request_id,
    const blink::mojom::ResourceLoadInfo& resource_load_info) {
  int net_error = resource_load_info.net_error;
  if (net_error != net::OK) {
    base::RecordComputedAction(
        base::StringPrintf("WebEngine.ResourceRequestError:%d", net_error));
  }
}

void FrameImpl::MediaStartedPlaying(const MediaPlayerInfo& video_type,
                                    const content::MediaPlayerId& id) {
  base::RecordComputedAction("MediaPlay");
}

void FrameImpl::MediaStoppedPlaying(
    const MediaPlayerInfo& video_type,
    const content::MediaPlayerId& id,
    WebContentsObserver::MediaStoppedReason reason) {
  base::RecordComputedAction("MediaPause");
}

void FrameImpl::OnPixelScaleUpdate(float pixel_scale) {
  if (accessibility_bridge_) {
    accessibility_bridge_->SetPixelScale(pixel_scale);
  }
}

void FrameImpl::SetAccessibilityEnabled(bool enabled) {
  if (!enabled) {
    scoped_accessibility_mode_.reset();
  } else if (!scoped_accessibility_mode_) {
    scoped_accessibility_mode_ =
        content::BrowserAccessibilityState::GetInstance()
            ->CreateScopedModeForProcess(ui::kAXModeComplete);
  }
}

void FrameImpl::OnThemeManagerError() {
  // TODO(crbug.com/40731307): Destroy the frame once a fake Display service is
  // implemented.
  // this->CloseAndDestroyFrame(ZX_ERR_INVALID_ARGS);
}
