// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/engine/browser/frame_impl.h"

#include <fuchsia/ui/gfx/cpp/fidl.h>
#include <lib/fpromise/result.h>
#include <lib/sys/cpp/component_context.h>
#include <lib/ui/scenic/cpp/view_ref_pair.h>
#include <limits>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/mem_buffer_util.h"
#include "base/fuchsia/process_context.h"
#include "base/json/json_writer.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/strcat.h"
#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/host_zoom_map.h"
#include "content/public/browser/media_session.h"
#include "content/public/browser/message_port_provider.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/permission_controller_delegate.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/renderer_preferences_util.h"
#include "content/public/browser/web_contents.h"
#include "fuchsia/base/message_port.h"
#include "fuchsia/engine/browser/accessibility_bridge.h"
#include "fuchsia/engine/browser/context_impl.h"
#include "fuchsia/engine/browser/event_filter.h"
#include "fuchsia/engine/browser/frame_layout_manager.h"
#include "fuchsia/engine/browser/frame_window_tree_host.h"
#include "fuchsia/engine/browser/media_player_impl.h"
#include "fuchsia/engine/browser/navigation_policy_handler.h"
#include "fuchsia/engine/browser/receiver_session_client.h"
#include "fuchsia/engine/browser/url_request_rewrite_type_converters.h"
#include "fuchsia/engine/browser/web_engine_devtools_controller.h"
#include "fuchsia/engine/common/cast_streaming.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "net/base/net_errors.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/logging/logging_utils.h"
#include "third_party/blink/public/common/messaging/web_message_port.h"
#include "third_party/blink/public/common/page/page_zoom.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom.h"
#include "third_party/blink/public/mojom/navigation/was_activated_option.mojom.h"
#include "ui/accessibility/platform/fuchsia/semantic_provider_impl.h"
#include "ui/aura/window.h"
#include "ui/compositor/compositor.h"
#include "ui/gfx/switches.h"
#include "ui/ozone/public/ozone_switches.h"
#include "ui/wm/core/base_focus_rules.h"
#include "url/gurl.h"

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
  // TODO(crbug.com/878439): Return a result based on window properties such as
  // visibility.
  return true;
}

// TODO(crbug.com/1113289): Use OnLoadScriptInjectorHost's origin matching code.
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

FuchsiaLogSeverity FuchsiaWebConsoleLogLevelToFxLogSeverity(
    fuchsia::web::ConsoleLogLevel level) {
  switch (level) {
    case fuchsia::web::ConsoleLogLevel::DEBUG:
      return FUCHSIA_LOG_DEBUG;
    case fuchsia::web::ConsoleLogLevel::INFO:
      return FUCHSIA_LOG_INFO;
    case fuchsia::web::ConsoleLogLevel::WARN:
      return FUCHSIA_LOG_WARNING;
    case fuchsia::web::ConsoleLogLevel::ERROR:
      return FUCHSIA_LOG_ERROR;
    case fuchsia::web::ConsoleLogLevel::NONE:
      return FUCHSIA_LOG_NONE;
    default:
      // Cope gracefully with callers setting undefined levels.
      DLOG(ERROR) << "Unknown log level:"
                  << static_cast<std::underlying_type<decltype(level)>::type>(
                         level);
      return FUCHSIA_LOG_NONE;
  }
}

FuchsiaLogSeverity BlinkConsoleMessageLevelToFxLogSeverity(
    blink::mojom::ConsoleMessageLevel level) {
  switch (level) {
    case blink::mojom::ConsoleMessageLevel::kVerbose:
      return FUCHSIA_LOG_DEBUG;
    case blink::mojom::ConsoleMessageLevel::kInfo:
      return FUCHSIA_LOG_INFO;
    case blink::mojom::ConsoleMessageLevel::kWarning:
      return FUCHSIA_LOG_WARNING;
    case blink::mojom::ConsoleMessageLevel::kError:
      return FUCHSIA_LOG_ERROR;
  }

  // Cope gracefully with callers setting undefined levels.
  DLOG(ERROR) << "Unknown log level:"
              << static_cast<std::underlying_type<decltype(level)>::type>(
                     level);
  return FUCHSIA_LOG_NONE;
}

bool IsHeadless() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kHeadless);
}

using FrameImplMap =
    base::small_map<std::map<content::WebContents*, FrameImpl*>>;

FrameImplMap& WebContentsToFrameImplMap() {
  static FrameImplMap frame_impl_map;
  return frame_impl_map;
}

content::PermissionType FidlPermissionTypeToContentPermissionType(
    fuchsia::web::PermissionType fidl_type) {
  switch (fidl_type) {
    case fuchsia::web::PermissionType::MICROPHONE:
      return content::PermissionType::AUDIO_CAPTURE;
    case fuchsia::web::PermissionType::CAMERA:
      return content::PermissionType::VIDEO_CAPTURE;
    case fuchsia::web::PermissionType::PROTECTED_MEDIA_IDENTIFIER:
      return content::PermissionType::PROTECTED_MEDIA_IDENTIFIER;
    case fuchsia::web::PermissionType::PERSISTENT_STORAGE:
      return content::PermissionType::DURABLE_STORAGE;
  }
}

// Permission request callback for FrameImpl::RequestMediaAccessPermission.
void HandleMediaPermissionsRequestResult(
    const content::MediaStreamRequest& request,
    content::MediaResponseCallback callback,
    const std::vector<blink::mojom::PermissionStatus>& result) {
  blink::MediaStreamDevices devices;

  int result_pos = 0;

  if (request.audio_type ==
      blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE) {
    if (result[result_pos] == blink::mojom::PermissionStatus::GRANTED) {
      devices.push_back(blink::MediaStreamDevice(
          request.audio_type, request.requested_audio_device_id,
          /*name=*/""));
    }
    result_pos++;
  }

  if (request.video_type ==
      blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE) {
    if (result[result_pos] == blink::mojom::PermissionStatus::GRANTED) {
      devices.push_back(blink::MediaStreamDevice(
          request.video_type, request.requested_video_device_id,
          /*name=*/""));
    }
  }

  std::move(callback).Run(
      devices,
      devices.empty() ? blink::mojom::MediaStreamRequestResult::NO_HARDWARE
                      : blink::mojom::MediaStreamRequestResult::OK,
      nullptr);
}

absl::optional<url::Origin> ParseAndValidateWebOrigin(
    const std::string& origin_str) {
  GURL origin_url(origin_str);
  if (!origin_url.username().empty() || !origin_url.password().empty() ||
      !origin_url.query().empty() || !origin_url.ref().empty()) {
    return absl::nullopt;
  }

  if (!origin_url.path().empty() && origin_url.path() != "/")
    return absl::nullopt;

  auto origin = url::Origin::Create(origin_url);
  if (origin.opaque())
    return absl::nullopt;

  return origin;
}

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
      navigation_controller_(web_contents_.get()),
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

  // TODO(http://crbug.com/1254073): Deprecate autoplay_policy in
  // CreateFrameParams.
  if (params_for_popups_.has_autoplay_policy()) {
    content_area_settings_.set_autoplay_policy(
        params_for_popups_.autoplay_policy());
  }
}

FrameImpl::~FrameImpl() {
  DestroyWindowTreeHost();
  context_->devtools_controller()->OnFrameDestroyed(web_contents_.get());

  auto& map = WebContentsToFrameImplMap();
  auto it = WebContentsToFrameImplMap().find(web_contents_.get());
  DCHECK(it != map.end() && it->second == this);
  map.erase(it);
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

  absl::optional<std::u16string> script_utf16 =
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

  web_contents_->GetMainFrame()->ExecuteJavaScript(*script_utf16,
                                                   std::move(result_callback));

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

void FrameImpl::AddNewContents(
    content::WebContents* source,
    std::unique_ptr<content::WebContents> new_contents,
    const GURL& target_url,
    WindowOpenDisposition disposition,
    const gfx::Rect& initial_rect,
    bool user_gesture,
    bool* was_blocked) {
  DCHECK_EQ(source, web_contents_.get());

  // TODO(crbug.com/995395): Add window disposition to the FIDL interface.
  switch (disposition) {
    case WindowOpenDisposition::NEW_FOREGROUND_TAB:
    case WindowOpenDisposition::NEW_BACKGROUND_TAB:
    case WindowOpenDisposition::NEW_PICTURE_IN_PICTURE:
    case WindowOpenDisposition::NEW_POPUP:
    case WindowOpenDisposition::NEW_WINDOW: {
      if (url_request_rewrite_rules_manager_.GetCachedRules()) {
        // There is no support for URL request rules rewriting with popups.
        *was_blocked = true;
        return;
      }

      auto* popup_creation_info =
          reinterpret_cast<PopupFrameCreationInfoUserData*>(
              new_contents->GetUserData(kPopupCreationInfo));
      fuchsia::web::PopupFrameCreationInfo popup_frame_creation_info =
          std::move(popup_creation_info->info);
      popup_frame_creation_info.set_initiated_by_user(user_gesture);
      // The PopupFrameCreationInfo won't be needed anymore, so clear it out.
      new_contents->SetUserData(kPopupCreationInfo, nullptr);

      // ContextImpl::CreateFrameInternal() verified that |params_for_popups_|
      // can be cloned, so it cannot fail here.
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
      return;
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
      return;
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
  v2_accessibility_bridge_.reset();

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
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&FrameImpl::CloseAndDestroyFrame,
                                weak_factory_.GetWeakPtr(), error));

  // The return value indicates to the accessibility bridge whether we should
  // attempt to reconnect. Since the frame has been destroyed, no reconnect
  // attempt should be made.
  return false;
}

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
  if (!context_->has_cast_streaming_enabled() || !receiver_session_client_)
    return;

  mojo::AssociatedRemote<cast_streaming::mojom::CastStreamingReceiver>
      cast_streaming_receiver;
  navigation_handle->GetRenderFrameHost()
      ->GetRemoteAssociatedInterfaces()
      ->GetInterface(&cast_streaming_receiver);
  receiver_session_client_->SetCastStreamingReceiver(
      std::move(cast_streaming_receiver));
}

void FrameImpl::UpdateRenderViewZoomLevel(
    content::RenderViewHost* render_view_host) {
  float page_scale = content_area_settings_.has_page_scale()
                         ? content_area_settings_.page_scale()
                         : 1.0;
  content::HostZoomMap* host_zoom_map =
      content::HostZoomMap::GetForWebContents(web_contents_.get());
  host_zoom_map->SetTemporaryZoomLevel(
      render_view_host->GetProcess()->GetID(), render_view_host->GetRoutingID(),
      blink::PageZoomFactorToZoomLevel(page_scale));
}

void FrameImpl::ConnectToAccessibilityBridge() {
  if (use_v2_accessibility_bridge_) {
    // TODO(crbug.com/1291613): Replace callbacks with an interface that
    // FrameImpl implements.
    v2_accessibility_bridge_ =
        std::make_unique<ui::AccessibilityBridgeFuchsiaImpl>(
            root_window(), window_tree_host_->CreateViewRef(),
            base::BindRepeating(&FrameImpl::GetDeviceScaleFactor,
                                base::Unretained(this)),
            base::BindRepeating(&FrameImpl::SetAccessibilityEnabled,
                                base::Unretained(this)),
            base::BindRepeating(&FrameImpl::OnAccessibilityError,
                                base::Unretained(this)),
            inspect_node_.CreateChild(kAccessibilityInspectNodeName));
  } else {
    fuchsia::accessibility::semantics::SemanticsManagerPtr semantics_manager;
    if (!semantics_manager_for_test_) {
      semantics_manager =
          base::ComponentContextForProcess()
              ->svc()
              ->Connect<fuchsia::accessibility::semantics::SemanticsManager>();
    }

    // If the SemanticTree owned by |accessibility_bridge_| is disconnected, it
    // will cause |this| to be closed.
    accessibility_bridge_ = std::make_unique<AccessibilityBridge>(
        semantics_manager_for_test_ ? semantics_manager_for_test_
                                    : semantics_manager.get(),
        window_tree_host_.get(), web_contents_.get(),
        base::BindOnce(&FrameImpl::OnAccessibilityError,
                       base::Unretained(this)),
        inspect_node_.CreateChild(kAccessibilityInspectNodeName));
  }
}

void FrameImpl::CreateView(fuchsia::ui::views::ViewToken view_token) {
  scenic::ViewRefPair view_ref_pair = scenic::ViewRefPair::New();
  CreateViewWithViewRef(std::move(view_token),
                        std::move(view_ref_pair.control_ref),
                        std::move(view_ref_pair.view_ref));
}

void FrameImpl::CreateViewWithViewRef(
    fuchsia::ui::views::ViewToken view_token,
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

  scenic::ViewRefPair view_ref_pair;
  view_ref_pair.control_ref = std::move(control_ref);
  view_ref_pair.view_ref = std::move(view_ref);
  SetupWindowTreeHost(std::move(view_token), std::move(view_ref_pair));

  ConnectToAccessibilityBridge();
}

void FrameImpl::CreateView2(fuchsia::web::CreateView2Args view_args) {
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

  scenic::ViewRefPair view_ref_pair = scenic::ViewRefPair::New();
  SetupWindowTreeHost(std::move(*view_args.mutable_view_creation_token()),
                      std::move(view_ref_pair));

  ConnectToAccessibilityBridge();
}

void FrameImpl::GetMediaPlayer(
    fidl::InterfaceRequest<fuchsia::media::sessions2::Player> player) {
  media_player_ = std::make_unique<MediaPlayerImpl>(
      content::MediaSession::Get(web_contents_.get()), std::move(player),
      base::BindOnce(&FrameImpl::OnMediaPlayerDisconnect,
                     base::Unretained(this)));
}

void FrameImpl::GetNavigationController(
    fidl::InterfaceRequest<fuchsia::web::NavigationController> controller) {
  navigation_controller_.AddBinding(std::move(controller));
}

void FrameImpl::ExecuteJavaScript(std::vector<std::string> origins,
                                  fuchsia::mem::Buffer script,
                                  ExecuteJavaScriptCallback callback) {
  ExecuteJavaScriptInternal(std::move(origins), std::move(script),
                            std::move(callback), true);
}

void FrameImpl::ExecuteJavaScriptNoResult(
    std::vector<std::string> origins,
    fuchsia::mem::Buffer script,
    ExecuteJavaScriptNoResultCallback callback) {
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
  constexpr char kWildcardOrigin[] = "*";

  if (!context_->IsJavaScriptInjectionAllowed()) {
    callback(fpromise::error(fuchsia::web::FrameError::INTERNAL_ERROR));
    return;
  }

  absl::optional<std::string> script_as_string =
      base::StringFromMemBuffer(script);
  if (!script_as_string) {
    LOG(ERROR) << "Couldn't read script from buffer.";
    callback(fpromise::error(fuchsia::web::FrameError::INTERNAL_ERROR));
    return;
  }

  // TODO(crbug.com/1108607): Only allow wildcards to be specified standalone.
  if (std::any_of(origins.begin(), origins.end(),
                  [kWildcardOrigin](base::StringPiece origin) {
                    return origin == kWildcardOrigin;
                  })) {
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
  script_injector_.RemoveScript(id);
}

void FrameImpl::PostMessage(std::string origin,
                            fuchsia::web::WebMessage message,
                            PostMessageCallback callback) {
  if (MaybeHandleCastStreamingMessage(&origin, &message, &callback))
    return;

  fuchsia::web::Frame_PostMessage_Result result;
  if (origin.empty()) {
    callback(fpromise::error(fuchsia::web::FrameError::INVALID_ORIGIN));
    return;
  }

  if (!message.has_data()) {
    callback(fpromise::error(fuchsia::web::FrameError::NO_DATA_IN_MESSAGE));
    return;
  }

  absl::optional<std::u16string> origin_utf16;
  if (origin != kWildcardOrigin)
    origin_utf16 = base::UTF8ToUTF16(origin);

  absl::optional<std::u16string> data_utf16 =
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
      blink::WebMessagePort blink_port = cr_fuchsia::BlinkMessagePortFromFidl(
          std::move(outgoing.message_port()));
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
  navigation_controller_.SetEventListener(std::move(listener), flags);
}

void FrameImpl::SetJavaScriptLogLevel(fuchsia::web::ConsoleLogLevel level) {
  log_level_ = FuchsiaWebConsoleLogLevelToFxLogSeverity(level);
}

void FrameImpl::SetConsoleLogSink(fuchsia::logger::LogSinkHandle sink) {
  if (sink) {
    console_logger_ = base::ScopedFxLogger::CreateFromLogSink(
        std::move(sink), {console_log_tag_});
  } else {
    console_logger_ = {};
  }
}

void FrameImpl::ConfigureInputTypes(fuchsia::web::InputTypes types,
                                    fuchsia::web::AllowInputState allow) {
  event_filter_.ConfigureInputTypes(types, allow);
}

void FrameImpl::SetPopupFrameCreationListener(
    fidl::InterfaceHandle<fuchsia::web::PopupFrameCreationListener> listener) {
  popup_listener_ = listener.Bind();
  popup_listener_.set_error_handler(
      fit::bind_member(this, &FrameImpl::OnPopupListenerDisconnected));
}

void FrameImpl::SetUrlRequestRewriteRules(
    std::vector<fuchsia::web::UrlRequestRewriteRule> rules,
    SetUrlRequestRewriteRulesCallback callback) {
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
  if (!IsHeadless()) {
    LOG(ERROR) << "EnableHeadlessRendering() on non-HEADLESS Context.";
    CloseAndDestroyFrame(ZX_ERR_INVALID_ARGS);
    return;
  }

  scenic::ViewRefPair view_ref_pair = scenic::ViewRefPair::New();
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
  if (!IsHeadless()) {
    LOG(ERROR)
        << "Attempted to disable headless rendering on non-HEADLESS Context.";
    CloseAndDestroyFrame(ZX_ERR_INVALID_ARGS);
    return;
  }

  DestroyWindowTreeHost();
}

void FrameImpl::SetupWindowTreeHost(fuchsia::ui::views::ViewToken view_token,
                                    scenic::ViewRefPair view_ref_pair) {
  DCHECK(!window_tree_host_);

  window_tree_host_ = std::make_unique<FrameWindowTreeHost>(
      std::move(view_token), std::move(view_ref_pair), web_contents_.get());

  InitWindowTreeHost();
}

void FrameImpl::SetupWindowTreeHost(
    fuchsia::ui::views::ViewCreationToken view_creation_token,
    scenic::ViewRefPair view_ref_pair) {
  DCHECK(!window_tree_host_);

  window_tree_host_ = std::make_unique<FrameWindowTreeHost>(
      std::move(view_creation_token), std::move(view_ref_pair),
      web_contents_.get());

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

  layout_manager_ = new FrameLayoutManager;
  root_window()->SetLayoutManager(layout_manager_);  // Transfers ownership.
  if (!render_size_override_.IsEmpty())
    layout_manager_->ForceContentDimensions(render_size_override_);

  root_window()->AddChild(web_contents_->GetNativeView());
  web_contents_->GetNativeView()->Show();

  // FrameWindowTreeHost will Show() itself when the View is actually attached
  // to the view-tree to be displayed. See https://crbug.com/1109270
}

void FrameImpl::SetMediaSessionId(uint64_t session_id) {
  media_session_id_ = session_id;
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

void FrameImpl::GetPrivateMemorySize(GetPrivateMemorySizeCallback callback) {
  if (!web_contents_->GetMainFrame()->GetProcess()->IsReady()) {
    // Renderer process is not yet started.
    callback(0);
    return;
  }

  zx_info_task_stats_t task_stats;
  zx_status_t status = zx_object_get_info(
      web_contents_->GetMainFrame()->GetProcess()->GetProcess().Handle(),
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
  navigation_policy_handler_ = std::make_unique<NavigationPolicyHandler>(
      std::move(params), std::move(provider));
}

void FrameImpl::OnThemeManagerError() {
  // TODO(crbug.com/1148454): Destroy the frame once a fake Display service is
  // implemented.
  // this->CloseAndDestroyFrame(ZX_ERR_INVALID_ARGS);
}

void FrameImpl::SetPreferredTheme(fuchsia::settings::ThemeType theme) {
  fuchsia::web::ContentAreaSettings settings;
  settings.set_theme(theme);
  SetContentAreaSettings(std::move(settings));
}

void FrameImpl::SetPageScale(float scale) {
  fuchsia::web::ContentAreaSettings settings;
  settings.set_page_scale(scale);
  SetContentAreaSettings(std::move(settings));
}

void FrameImpl::SetContentAreaSettings(
    fuchsia::web::ContentAreaSettings settings) {
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
      UpdateRenderViewZoomLevel(web_contents_->GetRenderViewHost());
    }
  }

  web_contents_->OnWebPreferencesChanged();
}

void FrameImpl::ResetContentAreaSettings() {
  content_area_settings_ = fuchsia::web::ContentAreaSettings();
  web_contents_->OnWebPreferencesChanged();
  UpdateRenderViewZoomLevel(web_contents_->GetRenderViewHost());
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

void FrameImpl::ForceContentDimensions(
    std::unique_ptr<fuchsia::ui::gfx::vec2> web_dips) {
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
  if (!fidl_permission.has_type()) {
    LOG(ERROR) << "PermissionDescriptor.type is not specified in "
                  "SetPermissionState().";
    CloseAndDestroyFrame(ZX_ERR_INVALID_ARGS);
    return;
  }

  content::PermissionType type =
      FidlPermissionTypeToContentPermissionType(fidl_permission.type());

  blink::mojom::PermissionStatus state =
      (fidl_state == fuchsia::web::PermissionState::GRANTED)
          ? blink::mojom::PermissionStatus::GRANTED
          : blink::mojom::PermissionStatus::DENIED;

  // TODO(crbug.com/1136994): Remove this once the PermissionManager API is
  // available.
  if (web_origin_string == "*" &&
      type == content::PermissionType::PROTECTED_MEDIA_IDENTIFIER) {
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

void FrameImpl::CloseContents(content::WebContents* source) {
  DCHECK_EQ(source, web_contents_.get());
  CloseAndDestroyFrame(ZX_OK);
}

void FrameImpl::SetBlockMediaLoading(bool blocked) {
  media_blocker_.BlockMediaLoading(blocked);
}

bool FrameImpl::DidAddMessageToConsole(
    content::WebContents* source,
    blink::mojom::ConsoleMessageLevel log_level,
    const std::u16string& message,
    int32_t line_no,
    const std::u16string& source_id) {
  // Prevent logging when log_level_ is 0. See crbug.com/1292187.
  // TODO(crbug.com/1292208): Convert to DCHECK when FUCHSIA_LOG_NONE
  // is defined to be greater than other log levels.
  if (log_level_ == 0) {
    // Prevent the default logging mechanism from logging the message.
    return true;
  }

  FuchsiaLogSeverity severity =
      BlinkConsoleMessageLevelToFxLogSeverity(log_level);
  if (severity < log_level_) {
    // Prevent the default logging mechanism from logging the message.
    return true;
  }

  if (!console_logger_.is_valid()) {
    // Log via the process' LogSink service if none was set on the Frame.
    // Connect on-demand, so that embedders need not provide a LogSink in the
    // CreateContextParams services, unless they actually enable logging.
    console_logger_ = base::ScopedFxLogger::CreateFromLogSink(
        base::ComponentContextForProcess()
            ->svc()
            ->Connect<fuchsia::logger::LogSink>(),
        {console_log_tag_});

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

  std::vector<content::PermissionType> permissions;

  if (request.audio_type ==
      blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE) {
    permissions.push_back(content::PermissionType::AUDIO_CAPTURE);
  } else if (request.audio_type != blink::mojom::MediaStreamType::NO_SERVICE) {
    std::move(callback).Run(
        blink::MediaStreamDevices(),
        blink::mojom::MediaStreamRequestResult::NOT_SUPPORTED, nullptr);
    return;
  }

  if (request.video_type ==
      blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE) {
    permissions.push_back(content::PermissionType::VIDEO_CAPTURE);
  } else if (request.video_type != blink::mojom::MediaStreamType::NO_SERVICE) {
    std::move(callback).Run(
        blink::MediaStreamDevices(),
        blink::mojom::MediaStreamRequestResult::NOT_SUPPORTED, nullptr);
    return;
  }

  auto* render_frame_host = content::RenderFrameHost::FromID(
      request.render_process_id, request.render_frame_id);
  if (!render_frame_host) {
    std::move(callback).Run(
        blink::MediaStreamDevices(),
        blink::mojom::MediaStreamRequestResult::INVALID_STATE, nullptr);
    return;
  }

  auto* permission_controller =
      web_contents_->GetBrowserContext()->GetPermissionControllerDelegate();
  permission_controller->RequestPermissions(
      permissions, render_frame_host, request.security_origin,
      request.user_gesture,
      base::BindOnce(&HandleMediaPermissionsRequestResult, request,
                     std::move(callback)));
}

bool FrameImpl::CheckMediaAccessPermission(
    content::RenderFrameHost* render_frame_host,
    const GURL& security_origin,
    blink::mojom::MediaStreamType type) {
  content::PermissionType permission;
  switch (type) {
    case blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE:
      permission = content::PermissionType::AUDIO_CAPTURE;
      break;
    case blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE:
      permission = content::PermissionType::VIDEO_CAPTURE;
      break;
    default:
      NOTREACHED();
      return false;
  }
  auto* permission_controller =
      web_contents_->GetBrowserContext()->GetPermissionControllerDelegate();
  return permission_controller->GetPermissionStatusForFrame(
             permission, render_frame_host, security_origin) ==
         blink::mojom::PermissionStatus::GRANTED;
}

bool FrameImpl::CanOverscrollContent() {
  // Don't process "overscroll" events (e.g. pull-to-refresh, swipe back,
  // swipe forward).
  // TODO(crbug/1177399): Add overscroll toggle to Frame API.
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

  MaybeStartCastStreaming(navigation_handle);
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

void FrameImpl::RenderViewHostChanged(content::RenderViewHost* old_host,
                                      content::RenderViewHost* new_host) {
  // UpdateRenderViewZoomLevel() sets temporary zoom level for the current
  // RenderView. It needs to be called again whenever main RenderView is
  // changed.
  UpdateRenderViewZoomLevel(new_host);
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

// TODO(crbug.com/1136681#c6): Move below GetBindingChannelForTest when fixed.
void FrameImpl::EnableExplicitSitesFilter(std::string error_page) {
  explicit_sites_filter_error_page_ = std::move(error_page);
}

float FrameImpl::GetDeviceScaleFactor() {
  if (device_scale_factor_for_test_)
    return *device_scale_factor_for_test_;

  return window_tree_host_->scenic_scale_factor();
}

void FrameImpl::SetAccessibilityEnabled(bool enabled) {
  auto* browser_accessibility_state =
      content::BrowserAccessibilityState::GetInstance();

  if (enabled) {
    browser_accessibility_state->AddAccessibilityModeFlags(ui::kAXModeComplete);
  } else {
    browser_accessibility_state->RemoveAccessibilityModeFlags(
        ui::kAXModeComplete);
  }
}
