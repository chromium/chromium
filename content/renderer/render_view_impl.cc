// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/render_view_impl.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <utility>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/debug/alias.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/i18n/rtl.h"
#include "base/json/json_writer.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_macros.h"
#include "base/process/kill.h"
#include "base/process/process.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/sys_info.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "cc/base/switches.h"
#include "cc/paint/skia_paint_canvas.h"
#include "cc/trees/layer_tree_host.h"
#include "content/common/content_constants_internal.h"
#include "content/common/dom_storage/dom_storage_types.h"
#include "content/common/drag_messages.h"
#include "content/common/frame_messages.h"
#include "content/common/frame_replication_state.h"
#include "content/common/input_messages.h"
#include "content/common/page_messages.h"
#include "content/common/render_message_filter.mojom.h"
#include "content/common/view_messages.h"
#include "content/public/common/browser_side_navigation_policy.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/page_importance_signals.h"
#include "content/public/common/page_state.h"
#include "content/public/common/page_zoom.h"
#include "content/public/common/three_d_api_types.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/use_zoom_for_dsf_policy.h"
#include "content/public/common/web_preferences.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/public/renderer/document_state.h"
#include "content/public/renderer/render_view_observer.h"
#include "content/public/renderer/render_view_visitor.h"
#include "content/public/renderer/window_features_converter.h"
#include "content/renderer/appcache/appcache_dispatcher.h"
#include "content/renderer/appcache/web_application_cache_host_impl.h"
#include "content/renderer/browser_plugin/browser_plugin.h"
#include "content/renderer/browser_plugin/browser_plugin_manager.h"
#include "content/renderer/drop_data_builder.h"
#include "content/renderer/gpu/layer_tree_view.h"
#include "content/renderer/history_serialization.h"
#include "content/renderer/ime_event_guard.h"
#include "content/renderer/internal_document_state_data.h"
#include "content/renderer/loader/request_extra_data.h"
#include "content/renderer/media/audio/audio_device_factory.h"
#include "content/renderer/media/stream/media_stream_device_observer.h"
#include "content/renderer/media/video_capture_impl_manager.h"
#include "content/renderer/media/webrtc/peer_connection_dependency_factory.h"
#include "content/renderer/media/webrtc/rtc_peer_connection_handler.h"
#include "content/renderer/render_frame_impl.h"
#include "content/renderer/render_frame_proxy.h"
#include "content/renderer/render_process.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/render_widget_fullscreen_pepper.h"
#include "content/renderer/renderer_blink_platform_impl.h"
#include "content/renderer/renderer_webapplicationcachehost_impl.h"
#include "content/renderer/resizing_mode_selector.h"
#include "content/renderer/savable_resources.h"
#include "content/renderer/v8_value_converter_impl.h"
#include "content/renderer/web_ui_extension_data.h"
#include "media/audio/audio_output_device.h"
#include "media/base/media_switches.h"
#include "media/media_buildflags.h"
#include "media/renderers/audio_renderer_impl.h"
#include "media/video/gpu_video_accelerator_factories.h"
#include "net/base/data_url.h"
#include "net/base/escape.h"
#include "net/base/net_errors.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/http/http_util.h"
#include "net/nqe/effective_connection_type.h"
#include "ppapi/buildflags/buildflags.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/dom_storage/session_storage_namespace_id.h"
#include "third_party/blink/public/common/frame/user_activation_update_source.h"
#include "third_party/blink/public/mojom/page/page_visibility_state.mojom.h"
#include "third_party/blink/public/platform/file_path_conversion.h"
#include "third_party/blink/public/platform/url_conversion.h"
#include "third_party/blink/public/platform/web_connection_type.h"
#include "third_party/blink/public/platform/web_effective_connection_type.h"
#include "third_party/blink/public/platform/web_http_body.h"
#include "third_party/blink/public/platform/web_image.h"
#include "third_party/blink/public/platform/web_input_event.h"
#include "third_party/blink/public/platform/web_input_event_result.h"
#include "third_party/blink/public/platform/web_network_state_notifier.h"
#include "third_party/blink/public/platform/web_point.h"
#include "third_party/blink/public/platform/web_rect.h"
#include "third_party/blink/public/platform/web_runtime_features.h"
#include "third_party/blink/public/platform/web_size.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_url_error.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/platform/web_url_response.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/public/public_buildflags.h"
#include "third_party/blink/public/web/web_autofill_client.h"
#include "third_party/blink/public/web/web_ax_object.h"
#include "third_party/blink/public/web/web_date_time_chooser_completion.h"
#include "third_party/blink/public/web/web_date_time_chooser_params.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_dom_event.h"
#include "third_party/blink/public/web/web_dom_message_event.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_file_chooser_completion.h"
#include "third_party/blink/public/web/web_file_chooser_params.h"
#include "third_party/blink/public/web/web_form_control_element.h"
#include "third_party/blink/public/web/web_form_element.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/public/web/web_frame_content_dumper.h"
#include "third_party/blink/public/web/web_frame_widget.h"
#include "third_party/blink/public/web/web_history_item.h"
#include "third_party/blink/public/web/web_hit_test_result.h"
#include "third_party/blink/public/web/web_input_element.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_navigation_policy.h"
#include "third_party/blink/public/web/web_page_importance_signals.h"
#include "third_party/blink/public/web/web_page_popup.h"
#include "third_party/blink/public/web/web_plugin.h"
#include "third_party/blink/public/web/web_plugin_action.h"
#include "third_party/blink/public/web/web_range.h"
#include "third_party/blink/public/web/web_render_theme.h"
#include "third_party/blink/public/web/web_script_source.h"
#include "third_party/blink/public/web/web_searchable_form_data.h"
#include "third_party/blink/public/web/web_security_policy.h"
#include "third_party/blink/public/web/web_settings.h"
#include "third_party/blink/public/web/web_user_gesture_indicator.h"
#include "third_party/blink/public/web/web_view.h"
#include "third_party/blink/public/web/web_window_features.h"
#include "third_party/icu/source/common/unicode/uchar.h"
#include "third_party/icu/source/common/unicode/uscript.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/ui_base_switches_util.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/switches.h"
#include "ui/latency/latency_info.h"
#include "url/origin.h"
#include "url/url_constants.h"
#include "v8/include/v8.h"

#if defined(OS_ANDROID)
#include <cpu-features.h>

#include "base/android/build_info.h"
#include "base/memory/shared_memory.h"
#include "content/child/child_thread_impl.h"
#include "ui/gfx/geometry/rect_f.h"

#elif defined(OS_MACOSX)
#include "skia/ext/skia_utils_mac.h"
#endif

#if BUILDFLAG(ENABLE_PLUGINS)
#include "content/renderer/pepper/pepper_plugin_instance_impl.h"
#include "content/renderer/pepper/pepper_plugin_registry.h"
#endif

using blink::WebAXObject;
using blink::WebApplicationCacheHost;
using blink::WebApplicationCacheHostClient;
using blink::WebConsoleMessage;
using blink::WebData;
using blink::WebDocument;
using blink::WebDragOperation;
using blink::WebElement;
using blink::WebFileChooserCompletion;
using blink::WebFormControlElement;
using blink::WebFormElement;
using blink::WebFrame;
using blink::WebFrameContentDumper;
using blink::WebGestureEvent;
using blink::WebHistoryItem;
using blink::WebHTTPBody;
using blink::WebHitTestResult;
using blink::WebImage;
using blink::WebInputElement;
using blink::WebInputEvent;
using blink::WebLocalFrame;
using blink::WebMouseEvent;
using blink::WebNavigationPolicy;
using blink::WebNavigationType;
using blink::WebNode;
using blink::WebPluginAction;
using blink::WebPoint;
using blink::WebRect;
using blink::WebSandboxFlags;
using blink::WebScriptSource;
using blink::WebSearchableFormData;
using blink::WebSecurityOrigin;
using blink::WebSecurityPolicy;
using blink::WebSettings;
using blink::WebSize;
using blink::WebString;
using blink::WebTextDirection;
using blink::WebTouchEvent;
using blink::WebURL;
using blink::WebURLError;
using blink::WebURLRequest;
using blink::WebURLResponse;
using blink::WebUserGestureIndicator;
using blink::WebVector;
using blink::WebView;
using blink::WebWidget;
using blink::WebWindowFeatures;
using blink::WebRuntimeFeatures;
using base::TimeDelta;

namespace content {

//-----------------------------------------------------------------------------

typedef std::map<blink::WebView*, RenderViewImpl*> ViewMap;
static base::LazyInstance<ViewMap>::Leaky g_view_map =
    LAZY_INSTANCE_INITIALIZER;
typedef std::map<int32_t, RenderViewImpl*> RoutingIDViewMap;
static base::LazyInstance<RoutingIDViewMap>::Leaky g_routing_id_view_map =
    LAZY_INSTANCE_INITIALIZER;

// Time, in seconds, we delay before sending content state changes (such as form
// state and scroll position) to the browser. We delay sending changes to avoid
// spamming the browser.
// To avoid having tab/session restore require sending a message to get the
// current content state during tab closing we use a shorter timeout for the
// foreground renderer. This means there is a small window of time from which
// content state is modified and not sent to session restore, but this is
// better than having to wake up all renderers during shutdown.
const int kDelaySecondsForContentStateSyncHidden = 5;
const int kDelaySecondsForContentStateSync = 1;

static RenderViewImpl* (*g_create_render_view_impl)(
    CompositorDependencies* compositor_deps,
    const mojom::CreateViewParams&) = nullptr;

// static
Referrer RenderViewImpl::GetReferrerFromRequest(
    WebFrame* frame,
    const WebURLRequest& request) {
  return Referrer(blink::WebStringToGURL(
                      request.HttpHeaderField(WebString::FromUTF8("Referer"))),
                  request.GetReferrerPolicy());
}

// static
WindowOpenDisposition RenderViewImpl::NavigationPolicyToDisposition(
    WebNavigationPolicy policy) {
  switch (policy) {
    case blink::kWebNavigationPolicyIgnore:
      return WindowOpenDisposition::IGNORE_ACTION;
    case blink::kWebNavigationPolicyDownload:
      return WindowOpenDisposition::SAVE_TO_DISK;
    case blink::kWebNavigationPolicyCurrentTab:
      return WindowOpenDisposition::CURRENT_TAB;
    case blink::kWebNavigationPolicyNewBackgroundTab:
      return WindowOpenDisposition::NEW_BACKGROUND_TAB;
    case blink::kWebNavigationPolicyNewForegroundTab:
      return WindowOpenDisposition::NEW_FOREGROUND_TAB;
    case blink::kWebNavigationPolicyNewWindow:
      return WindowOpenDisposition::NEW_WINDOW;
    case blink::kWebNavigationPolicyNewPopup:
      return WindowOpenDisposition::NEW_POPUP;
  default:
    NOTREACHED() << "Unexpected WebNavigationPolicy";
    return WindowOpenDisposition::IGNORE_ACTION;
  }
}

///////////////////////////////////////////////////////////////////////////////

namespace {

typedef void (*SetFontFamilyWrapper)(blink::WebSettings*,
                                     const base::string16&,
                                     UScriptCode);

void SetStandardFontFamilyWrapper(WebSettings* settings,
                                  const base::string16& font,
                                  UScriptCode script) {
  settings->SetStandardFontFamily(WebString::FromUTF16(font), script);
}

void SetFixedFontFamilyWrapper(WebSettings* settings,
                               const base::string16& font,
                               UScriptCode script) {
  settings->SetFixedFontFamily(WebString::FromUTF16(font), script);
}

void SetSerifFontFamilyWrapper(WebSettings* settings,
                               const base::string16& font,
                               UScriptCode script) {
  settings->SetSerifFontFamily(WebString::FromUTF16(font), script);
}

void SetSansSerifFontFamilyWrapper(WebSettings* settings,
                                   const base::string16& font,
                                   UScriptCode script) {
  settings->SetSansSerifFontFamily(WebString::FromUTF16(font), script);
}

void SetCursiveFontFamilyWrapper(WebSettings* settings,
                                 const base::string16& font,
                                 UScriptCode script) {
  settings->SetCursiveFontFamily(WebString::FromUTF16(font), script);
}

void SetFantasyFontFamilyWrapper(WebSettings* settings,
                                 const base::string16& font,
                                 UScriptCode script) {
  settings->SetFantasyFontFamily(WebString::FromUTF16(font), script);
}

void SetPictographFontFamilyWrapper(WebSettings* settings,
                                    const base::string16& font,
                                    UScriptCode script) {
  settings->SetPictographFontFamily(WebString::FromUTF16(font), script);
}

// If |scriptCode| is a member of a family of "similar" script codes, returns
// the script code in that family that is used by WebKit for font selection
// purposes.  For example, USCRIPT_KATAKANA_OR_HIRAGANA and USCRIPT_JAPANESE are
// considered equivalent for the purposes of font selection.  WebKit uses the
// script code USCRIPT_KATAKANA_OR_HIRAGANA.  So, if |scriptCode| is
// USCRIPT_JAPANESE, the function returns USCRIPT_KATAKANA_OR_HIRAGANA.  WebKit
// uses different scripts than the ones in Chrome pref names because the version
// of ICU included on certain ports does not have some of the newer scripts.  If
// |scriptCode| is not a member of such a family, returns |scriptCode|.
UScriptCode GetScriptForWebSettings(UScriptCode scriptCode) {
  switch (scriptCode) {
    case USCRIPT_HIRAGANA:
    case USCRIPT_KATAKANA:
    case USCRIPT_JAPANESE:
      return USCRIPT_KATAKANA_OR_HIRAGANA;
    case USCRIPT_KOREAN:
      return USCRIPT_HANGUL;
    default:
      return scriptCode;
  }
}

void ApplyFontsFromMap(const ScriptFontFamilyMap& map,
                       SetFontFamilyWrapper setter,
                       WebSettings* settings) {
  for (auto it = map.begin(); it != map.end(); ++it) {
    int32_t script = u_getPropertyValueEnum(UCHAR_SCRIPT, (it->first).c_str());
    if (script >= 0 && script < USCRIPT_CODE_LIMIT) {
      UScriptCode code = static_cast<UScriptCode>(script);
      (*setter)(settings, it->second, GetScriptForWebSettings(code));
    }
  }
}

void ApplyBlinkSettings(const base::CommandLine& command_line,
                        WebSettings* settings) {
  if (!command_line.HasSwitch(switches::kBlinkSettings))
    return;

  std::vector<std::string> blink_settings = base::SplitString(
      command_line.GetSwitchValueASCII(switches::kBlinkSettings),
      ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  for (const std::string& setting : blink_settings) {
    size_t pos = setting.find('=');
    settings->SetFromStrings(
        blink::WebString::FromLatin1(setting.substr(0, pos)),
        blink::WebString::FromLatin1(
            pos == std::string::npos ? "" : setting.substr(pos + 1)));
  }
}

content::mojom::WindowContainerType WindowFeaturesToContainerType(
    const blink::WebWindowFeatures& window_features) {
  if (window_features.background) {
    if (window_features.persistent)
      return content::mojom::WindowContainerType::PERSISTENT;
    else
      return content::mojom::WindowContainerType::BACKGROUND;
  } else {
    return content::mojom::WindowContainerType::NORMAL;
  }
}

// Check content::BrowserControlsState, and cc::BrowserControlsState
// are kept in sync.
static_assert(int(BROWSER_CONTROLS_STATE_SHOWN) ==
                  int(cc::BrowserControlsState::kShown),
              "mismatching enums: SHOWN");
static_assert(int(BROWSER_CONTROLS_STATE_HIDDEN) ==
                  int(cc::BrowserControlsState::kHidden),
              "mismatching enums: HIDDEN");
static_assert(int(BROWSER_CONTROLS_STATE_BOTH) ==
                  int(cc::BrowserControlsState::kBoth),
              "mismatching enums: BOTH");

cc::BrowserControlsState ContentToCc(BrowserControlsState state) {
  return static_cast<cc::BrowserControlsState>(state);
}

}  // namespace

RenderViewImpl::RenderViewImpl(CompositorDependencies* compositor_deps,
                               const mojom::CreateViewParams& params)
    : RenderWidget(params.main_frame_widget_routing_id,
                   compositor_deps,
                   WidgetType::kFrame,
                   params.visual_properties.screen_info,
                   params.visual_properties.display_mode,
                   params.swapped_out,
                   params.hidden,
                   params.never_visible),
      routing_id_(params.view_id),
      renderer_wide_named_frame_lookup_(
          params.renderer_wide_named_frame_lookup),
      webkit_preferences_(params.web_preferences),
      session_storage_namespace_id_(params.session_storage_namespace_id),
      weak_ptr_factory_(this) {
  DCHECK(!session_storage_namespace_id_.empty())
      << "Session storage namespace must be populated.";
  GetWidget()->set_owner_delegate(this);
  RenderThread::Get()->AddRoute(routing_id_, this);
}

void RenderViewImpl::Initialize(
    mojom::CreateViewParamsPtr params,
    RenderWidget::ShowCallback show_callback,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
#if defined(OS_ANDROID)
  bool has_show_callback = !!show_callback;
#endif

  WebFrame* opener_frame =
      RenderFrameImpl::ResolveOpener(params->opener_frame_route_id);

  // Pass WidgetClient(), not |this|, as the WebWidgetClient. The method may
  // be overridden in layout tests to inject a test-only WebWidgetClient.
  webview_ =
      WebView::Create(this, WidgetClient(),
                      is_hidden() ? blink::mojom::PageVisibilityState::kHidden
                                  : blink::mojom::PageVisibilityState::kVisible,
                      opener_frame ? opener_frame->View() : nullptr);
  RenderWidget::Init(std::move(show_callback), webview_->GetWidget());

  g_view_map.Get().insert(std::make_pair(webview(), this));
  g_routing_id_view_map.Get().insert(std::make_pair(GetRoutingID(), this));

  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

  webview()->SetDisplayMode(GetWidget()->display_mode());
  webview()->GetSettings()->SetThreadedScrollingEnabled(
      !command_line.HasSwitch(switches::kDisableThreadedScrolling));
  webview()->SetShowFPSCounter(
      command_line.HasSwitch(cc::switches::kShowFPSCounter));

  ApplyWebPreferences(webkit_preferences_, webview());

  if (switches::IsTouchDragDropEnabled())
    webview()->GetSettings()->SetTouchDragDropEnabled(true);

  WebSettings::SelectionStrategyType selection_strategy =
      WebSettings::SelectionStrategyType::kCharacter;
  const std::string selection_strategy_str =
      command_line.GetSwitchValueASCII(switches::kTouchTextSelectionStrategy);
  if (selection_strategy_str == "direction")
    selection_strategy = WebSettings::SelectionStrategyType::kDirection;
  webview()->GetSettings()->SetSelectionStrategy(selection_strategy);

  std::string passive_listeners_default =
      command_line.GetSwitchValueASCII(switches::kPassiveListenersDefault);
  if (!passive_listeners_default.empty()) {
    WebSettings::PassiveEventListenerDefault passiveDefault =
        WebSettings::PassiveEventListenerDefault::kFalse;
    if (passive_listeners_default == "true")
      passiveDefault = WebSettings::PassiveEventListenerDefault::kTrue;
    else if (passive_listeners_default == "forcealltrue")
      passiveDefault = WebSettings::PassiveEventListenerDefault::kForceAllTrue;
    webview()->GetSettings()->SetPassiveEventListenerDefault(passiveDefault);
  }

  std::string network_quiet_timeout =
      command_line.GetSwitchValueASCII(switches::kNetworkQuietTimeout);
  if (!network_quiet_timeout.empty()) {
    double network_quiet_timeout_seconds = 0.0;
    if (base::StringToDouble(network_quiet_timeout,
                             &network_quiet_timeout_seconds)) {
      webview()->GetSettings()->SetNetworkQuietTimeout(
          network_quiet_timeout_seconds);
    }
  }

  ApplyBlinkSettings(command_line, webview()->GetSettings());

  // We have either a main frame or a proxy routing id.
  DCHECK_NE(params->main_frame_routing_id != MSG_ROUTING_NONE,
            params->proxy_routing_id != MSG_ROUTING_NONE);

  if (params->main_frame_routing_id != MSG_ROUTING_NONE) {
    CHECK(params->main_frame_interface_provider.is_valid());
    service_manager::mojom::InterfaceProviderPtr main_frame_interface_provider(
        std::move(params->main_frame_interface_provider));
    main_render_frame_ = RenderFrameImpl::CreateMainFrame(
        this, params->main_frame_routing_id,
        std::move(main_frame_interface_provider),
        params->main_frame_widget_routing_id, params->hidden,
        GetWidget()->GetWebScreenInfo(), GetWidget()->compositor_deps(),
        opener_frame, params->devtools_main_frame_token,
        params->replicated_frame_state, params->has_committed_real_load);

    main_render_frame_->Initialize();
  } else {
    CHECK(params->swapped_out);
    RenderFrameProxy::CreateFrameProxy(params->proxy_routing_id, GetRoutingID(),
                                       opener_frame, MSG_ROUTING_NONE,
                                       params->replicated_frame_state,
                                       params->devtools_main_frame_token);
  }

  // TODO(davidben): Move this state from Blink into content.
  if (params->window_was_created_with_opener)
    webview()->SetOpenedByDOM();

  UpdateWebViewWithDeviceScaleFactor();
  OnSetRendererPrefs(params->renderer_preferences);
  OnSynchronizeVisualProperties(params->visual_properties);

  GetContentClient()->renderer()->RenderViewCreated(this);
  page_zoom_level_ = 0;

  nav_state_sync_timer_.SetTaskRunner(task_runner);

#if defined(OS_ANDROID)
  // TODO(sgurun): crbug.com/325351 Needed only for android webview's deprecated
  // HandleNavigation codepath.
  // Renderer-created RenderViews have a ShowCallback because they send a Show
  // request (ViewHostMsg_ShowWidget, ViewHostMsg_ShowFullscreenWidget, or
  // FrameHostMsg_ShowCreatedWindow) to the browser to attach them to the UI
  // there. Browser-created RenderViews do not send a Show request to the
  // browser, so have no such callback.
  was_created_by_renderer_ = has_show_callback;
#endif
}

RenderViewImpl::~RenderViewImpl() {
  DCHECK(!frame_widget_);
  RenderThread::Get()->RemoveRoute(routing_id_);

#if defined(OS_ANDROID)
  // The date/time picker client is both a std::unique_ptr member of this class
  // and a RenderViewObserver. Reset it to prevent double deletion.
  date_time_picker_client_.reset();
#endif

#ifndef NDEBUG
  // Make sure we are no longer referenced by the ViewMap or RoutingIDViewMap.
  ViewMap* views = g_view_map.Pointer();
  for (ViewMap::iterator it = views->begin(); it != views->end(); ++it)
    DCHECK_NE(this, it->second) << "Failed to call Close?";
  RoutingIDViewMap* routing_id_views = g_routing_id_view_map.Pointer();
  for (RoutingIDViewMap::iterator it = routing_id_views->begin();
       it != routing_id_views->end(); ++it)
    DCHECK_NE(this, it->second) << "Failed to call Close?";
#endif

  for (auto& observer : observers_)
    observer.RenderViewGone();
  for (auto& observer : observers_)
    observer.OnDestruct();
}

/*static*/
RenderViewImpl* RenderViewImpl::FromWebView(WebView* webview) {
  ViewMap* views = g_view_map.Pointer();
  auto it = views->find(webview);
  return it == views->end() ? NULL : it->second;
}

/*static*/
RenderView* RenderView::FromWebView(blink::WebView* webview) {
  return RenderViewImpl::FromWebView(webview);
}

/*static*/
RenderViewImpl* RenderViewImpl::FromRoutingID(int32_t routing_id) {
  RoutingIDViewMap* views = g_routing_id_view_map.Pointer();
  auto it = views->find(routing_id);
  return it == views->end() ? NULL : it->second;
}

/*static*/
RenderView* RenderView::FromRoutingID(int routing_id) {
  return RenderViewImpl::FromRoutingID(routing_id);
}

/* static */
size_t RenderView::GetRenderViewCount() {
  return g_view_map.Get().size();
}

/*static*/
void RenderView::ForEach(RenderViewVisitor* visitor) {
  ViewMap* views = g_view_map.Pointer();
  for (auto it = views->begin(); it != views->end(); ++it) {
    if (!visitor->Visit(it->second))
      return;
  }
}

/*static*/
void RenderView::ApplyWebPreferences(const WebPreferences& prefs,
                                     WebView* web_view) {
  WebSettings* settings = web_view->GetSettings();
  ApplyFontsFromMap(prefs.standard_font_family_map,
                    SetStandardFontFamilyWrapper, settings);
  ApplyFontsFromMap(prefs.fixed_font_family_map,
                    SetFixedFontFamilyWrapper, settings);
  ApplyFontsFromMap(prefs.serif_font_family_map,
                    SetSerifFontFamilyWrapper, settings);
  ApplyFontsFromMap(prefs.sans_serif_font_family_map,
                    SetSansSerifFontFamilyWrapper, settings);
  ApplyFontsFromMap(prefs.cursive_font_family_map,
                    SetCursiveFontFamilyWrapper, settings);
  ApplyFontsFromMap(prefs.fantasy_font_family_map,
                    SetFantasyFontFamilyWrapper, settings);
  ApplyFontsFromMap(prefs.pictograph_font_family_map,
                    SetPictographFontFamilyWrapper, settings);
  settings->SetDefaultFontSize(prefs.default_font_size);
  settings->SetDefaultFixedFontSize(prefs.default_fixed_font_size);
  settings->SetMinimumFontSize(prefs.minimum_font_size);
  settings->SetMinimumLogicalFontSize(prefs.minimum_logical_font_size);
  settings->SetDefaultTextEncodingName(
      WebString::FromASCII(prefs.default_encoding));
  settings->SetJavaScriptEnabled(prefs.javascript_enabled);
  settings->SetWebSecurityEnabled(prefs.web_security_enabled);
  settings->SetLoadsImagesAutomatically(prefs.loads_images_automatically);
  settings->SetImagesEnabled(prefs.images_enabled);
  settings->SetPluginsEnabled(prefs.plugins_enabled);
  settings->SetDOMPasteAllowed(prefs.dom_paste_enabled);
  settings->SetTextAreasAreResizable(prefs.text_areas_are_resizable);
  settings->SetAllowScriptsToCloseWindows(prefs.allow_scripts_to_close_windows);
  settings->SetDownloadableBinaryFontsEnabled(prefs.remote_fonts_enabled);
  settings->SetJavaScriptCanAccessClipboard(
      prefs.javascript_can_access_clipboard);
  WebRuntimeFeatures::EnableXSLT(prefs.xslt_enabled);
  settings->SetXSSAuditorEnabled(prefs.xss_auditor_enabled);
  settings->SetDNSPrefetchingEnabled(prefs.dns_prefetching_enabled);
  blink::WebNetworkStateNotifier::SetSaveDataEnabled(prefs.data_saver_enabled);
  settings->SetLocalStorageEnabled(prefs.local_storage_enabled);
  settings->SetSyncXHRInDocumentsEnabled(prefs.sync_xhr_in_documents_enabled);
  WebRuntimeFeatures::EnableDatabase(prefs.databases_enabled);
  settings->SetOfflineWebApplicationCacheEnabled(
      prefs.application_cache_enabled);
  settings->SetHistoryEntryRequiresUserGesture(
      prefs.history_entry_requires_user_gesture);
  settings->SetShouldThrottlePushState(!prefs.disable_pushstate_throttle);
  settings->SetHyperlinkAuditingEnabled(prefs.hyperlink_auditing_enabled);
  settings->SetCookieEnabled(prefs.cookie_enabled);
  settings->SetNavigateOnDragDrop(prefs.navigate_on_drag_drop);

  // By default, allow_universal_access_from_file_urls is set to false and thus
  // we mitigate attacks from local HTML files by not granting file:// URLs
  // universal access. Only test shell will enable this.
  settings->SetAllowUniversalAccessFromFileURLs(
      prefs.allow_universal_access_from_file_urls);
  settings->SetAllowFileAccessFromFileURLs(
      prefs.allow_file_access_from_file_urls);

  settings->SetWebGL1Enabled(prefs.webgl1_enabled);
  settings->SetWebGL2Enabled(prefs.webgl2_enabled);

  // Enable WebGL errors to the JS console if requested.
  settings->SetWebGLErrorsToConsoleEnabled(
      prefs.webgl_errors_to_console_enabled);

  // Uses the mock theme engine for scrollbars.
  settings->SetMockScrollbarsEnabled(prefs.mock_scrollbars_enabled);

  settings->SetHideScrollbars(prefs.hide_scrollbars);

  // Enable gpu-accelerated 2d canvas if requested on the command line.
  WebRuntimeFeatures::EnableAccelerated2dCanvas(
      prefs.accelerated_2d_canvas_enabled);

  settings->SetMinimumAccelerated2dCanvasSize(
      prefs.minimum_accelerated_2d_canvas_size);

  // Disable antialiasing for 2d canvas if requested on the command line.
  settings->SetAntialiased2dCanvasEnabled(
      !prefs.antialiased_2d_canvas_disabled);

  // Disable antialiasing of clips for 2d canvas if requested on the command
  // line.
  settings->SetAntialiasedClips2dCanvasEnabled(
      prefs.antialiased_clips_2d_canvas_enabled);

  // Set MSAA sample count for 2d canvas if requested on the command line (or
  // default value if not).
  settings->SetAccelerated2dCanvasMSAASampleCount(
      prefs.accelerated_2d_canvas_msaa_sample_count);

  // Tabs to link is not part of the settings. WebCore calls
  // ChromeClient::tabsToLinks which is part of the glue code.
  web_view->SetTabsToLinks(prefs.tabs_to_links);

  settings->SetAllowRunningOfInsecureContent(
      prefs.allow_running_insecure_content);
  settings->SetDisableReadingFromCanvas(prefs.disable_reading_from_canvas);
  settings->SetStrictMixedContentChecking(prefs.strict_mixed_content_checking);

  settings->SetStrictlyBlockBlockableMixedContent(
      prefs.strictly_block_blockable_mixed_content);

  settings->SetStrictMixedContentCheckingForPlugin(
      prefs.block_mixed_plugin_content);

  settings->SetStrictPowerfulFeatureRestrictions(
      prefs.strict_powerful_feature_restrictions);
  settings->SetAllowGeolocationOnInsecureOrigins(
      prefs.allow_geolocation_on_insecure_origins);
  settings->SetPasswordEchoEnabled(prefs.password_echo_enabled);
  settings->SetShouldPrintBackgrounds(prefs.should_print_backgrounds);
  settings->SetShouldClearDocumentBackground(
      prefs.should_clear_document_background);
  settings->SetEnableScrollAnimator(prefs.enable_scroll_animator);

  WebRuntimeFeatures::EnableTouchEventFeatureDetection(
      prefs.touch_event_feature_detection_enabled);
  settings->SetMaxTouchPoints(prefs.pointer_events_max_touch_points);
  settings->SetAvailablePointerTypes(prefs.available_pointer_types);
  settings->SetPrimaryPointerType(
      static_cast<blink::PointerType>(prefs.primary_pointer_type));
  settings->SetAvailableHoverTypes(prefs.available_hover_types);
  settings->SetPrimaryHoverType(
      static_cast<blink::HoverType>(prefs.primary_hover_type));
  settings->SetEnableTouchAdjustment(prefs.touch_adjustment_enabled);
  settings->SetBarrelButtonForDragEnabled(prefs.barrel_button_for_drag_enabled);

  settings->SetShouldRespectImageOrientation(
      prefs.should_respect_image_orientation);

  settings->SetEditingBehavior(
      static_cast<WebSettings::EditingBehavior>(prefs.editing_behavior));

  settings->SetSupportsMultipleWindows(prefs.supports_multiple_windows);

  settings->SetMainFrameClipsContent(!prefs.record_whole_document);

  settings->SetSmartInsertDeleteEnabled(prefs.smart_insert_delete_enabled);

  settings->SetSpatialNavigationEnabled(prefs.spatial_navigation_enabled);

  settings->SetSelectionIncludesAltImageText(true);

  settings->SetV8CacheOptions(
      static_cast<WebSettings::V8CacheOptions>(prefs.v8_cache_options));

  settings->SetImageAnimationPolicy(
      static_cast<WebSettings::ImageAnimationPolicy>(prefs.animation_policy));

  settings->SetPresentationRequiresUserGesture(
      prefs.user_gesture_required_for_presentation);

  settings->SetTextTrackMarginPercentage(prefs.text_track_margin_percentage);

  // Needs to happen before SetDefaultPageScaleLimits below since that'll
  // recalculate the final page scale limits and that depends on this setting.
  settings->SetShrinksViewportContentToFit(
      prefs.shrinks_viewport_contents_to_fit);

  // Needs to happen before SetIgnoreViewportTagScaleLimits below.
  web_view->SetDefaultPageScaleLimits(prefs.default_minimum_page_scale_factor,
                                      prefs.default_maximum_page_scale_factor);

  settings->SetSavePreviousDocumentResources(
      static_cast<WebSettings::SavePreviousDocumentResources>(
          prefs.save_previous_document_resources));

  settings->SetTextAutosizingEnabled(prefs.text_autosizing_enabled);
  settings->SetDoubleTapToZoomEnabled(prefs.double_tap_to_zoom_enabled);
  blink::WebNetworkStateNotifier::SetNetworkQualityWebHoldback(
      static_cast<blink::WebEffectiveConnectionType>(
          prefs.network_quality_estimator_web_holdback));

#if defined(OS_ANDROID)
  settings->SetAllowCustomScrollbarInMainFrame(false);
  settings->SetAccessibilityFontScaleFactor(prefs.font_scale_factor);
  settings->SetDeviceScaleAdjustment(prefs.device_scale_adjustment);
  settings->SetFullscreenSupported(prefs.fullscreen_supported);
  web_view->SetIgnoreViewportTagScaleLimits(prefs.force_enable_zoom);
  settings->SetAutoZoomFocusedNodeToLegibleScale(true);
  settings->SetMediaPlaybackGestureWhitelistScope(blink::WebString::FromASCII(
      prefs.media_playback_gesture_whitelist_scope.spec()));
  settings->SetDefaultVideoPosterURL(
      WebString::FromASCII(prefs.default_video_poster_url.spec()));
  settings->SetSupportDeprecatedTargetDensityDPI(
      prefs.support_deprecated_target_density_dpi);
  settings->SetUseLegacyBackgroundSizeShorthandBehavior(
      prefs.use_legacy_background_size_shorthand_behavior);
  settings->SetWideViewportQuirkEnabled(prefs.wide_viewport_quirk);
  settings->SetUseWideViewport(prefs.use_wide_viewport);
  settings->SetForceZeroLayoutHeight(prefs.force_zero_layout_height);
  settings->SetViewportMetaLayoutSizeQuirk(
      prefs.viewport_meta_layout_size_quirk);
  settings->SetViewportMetaMergeContentQuirk(
      prefs.viewport_meta_merge_content_quirk);
  settings->SetViewportMetaNonUserScalableQuirk(
      prefs.viewport_meta_non_user_scalable_quirk);
  settings->SetViewportMetaZeroValuesQuirk(
      prefs.viewport_meta_zero_values_quirk);
  settings->SetClobberUserAgentInitialScaleQuirk(
      prefs.clobber_user_agent_initial_scale_quirk);
  settings->SetIgnoreMainFrameOverflowHiddenQuirk(
      prefs.ignore_main_frame_overflow_hidden_quirk);
  settings->SetReportScreenSizeInPhysicalPixelsQuirk(
      prefs.report_screen_size_in_physical_pixels_quirk);
  settings->SetShouldReuseGlobalForUnownedMainFrame(
      prefs.reuse_global_for_unowned_main_frame);
  settings->SetPreferHiddenVolumeControls(true);
  settings->SetSpellCheckEnabledByDefault(prefs.spellcheck_enabled_by_default);

  // Force preload=none and disable autoplay on older Android
  // platforms because their media pipelines are not stable enough to handle
  // concurrent elements. See http://crbug.com/612909, http://crbug.com/622826.
  const bool is_jelly_bean =
      base::android::BuildInfo::GetInstance()->sdk_int() <=
      base::android::SDK_VERSION_JELLY_BEAN_MR2;
  settings->SetForcePreloadNoneForMediaElements(is_jelly_bean);

  WebRuntimeFeatures::EnableVideoFullscreenOrientationLock(
      prefs.video_fullscreen_orientation_lock_enabled);
  WebRuntimeFeatures::EnableVideoRotateToFullscreen(
      prefs.video_rotate_to_fullscreen_enabled);
  WebRuntimeFeatures::EnableVideoFullscreenDetection(
      prefs.video_fullscreen_detection_enabled);
  settings->SetEmbeddedMediaExperienceEnabled(
      prefs.embedded_media_experience_enabled);
  settings->SetImmersiveModeEnabled(prefs.immersive_mode_enabled);
  settings->SetMediaDownloadInProductHelpEnabled(
      prefs.enable_media_download_in_product_help);
  settings->SetDoNotUpdateSelectionOnMutatingSelectionRange(
      prefs.do_not_update_selection_on_mutating_selection_range);
  WebRuntimeFeatures::EnableCSSHexAlphaColor(prefs.css_hex_alpha_color_enabled);
  WebRuntimeFeatures::EnableScrollTopLeftInterop(
      prefs.scroll_top_left_interop_enabled);
#endif  // defined(OS_ANDROID)

  switch (prefs.autoplay_policy) {
    case AutoplayPolicy::kNoUserGestureRequired:
      settings->SetAutoplayPolicy(
          WebSettings::AutoplayPolicy::kNoUserGestureRequired);
      break;
    case AutoplayPolicy::kUserGestureRequired:
      settings->SetAutoplayPolicy(
          WebSettings::AutoplayPolicy::kUserGestureRequired);
      break;
    case AutoplayPolicy::kUserGestureRequiredForCrossOrigin:
      settings->SetAutoplayPolicy(
          WebSettings::AutoplayPolicy::kUserGestureRequiredForCrossOrigin);
      break;
    case AutoplayPolicy::kDocumentUserActivationRequired:
      settings->SetAutoplayPolicy(
          WebSettings::AutoplayPolicy::kDocumentUserActivationRequired);
      break;
  }

  settings->SetViewportEnabled(prefs.viewport_enabled);
  settings->SetViewportMetaEnabled(prefs.viewport_meta_enabled);
  settings->SetViewportStyle(
      static_cast<blink::WebViewportStyle>(prefs.viewport_style));

  settings->SetLoadWithOverviewMode(prefs.initialize_at_minimum_page_scale);
  settings->SetMainFrameResizesAreOrientationChanges(
      prefs.main_frame_resizes_are_orientation_changes);

  settings->SetUseSolidColorScrollbars(prefs.use_solid_color_scrollbars);

  settings->SetShowContextMenuOnMouseUp(prefs.context_menu_on_mouse_up);
  settings->SetAlwaysShowContextMenuOnTouch(
      prefs.always_show_context_menu_on_touch);
  settings->SetSmoothScrollForFindEnabled(prefs.smooth_scroll_for_find_enabled);

  settings->SetHideDownloadUI(prefs.hide_download_ui);
  WebRuntimeFeatures::EnableNewRemotePlaybackPipeline(
      base::FeatureList::IsEnabled(media::kNewRemotePlaybackPipeline));

  settings->SetPresentationReceiver(prefs.presentation_receiver);

  settings->SetMediaControlsEnabled(prefs.media_controls_enabled);

  settings->SetLowPriorityIframesThreshold(
      static_cast<blink::WebEffectiveConnectionType>(
          prefs.low_priority_iframes_threshold));

  settings->SetPictureInPictureEnabled(
      prefs.picture_in_picture_enabled &&
      MediaFactory::GetVideoSurfaceLayerMode() !=
          blink::WebMediaPlayer::SurfaceLayerMode::kNever);

  settings->SetDataSaverHoldbackWebApi(
      prefs.data_saver_holdback_web_api_enabled);
  settings->SetDataSaverHoldbackMediaApi(
      prefs.data_saver_holdback_media_api_enabled);

  for (const auto& ect_distance_pair :
       prefs.lazy_frame_loading_distance_thresholds_px) {
    switch (ect_distance_pair.first) {
      case net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN:
        settings->SetLazyFrameLoadingDistanceThresholdPxUnknown(
            ect_distance_pair.second);
        continue;
      case net::EFFECTIVE_CONNECTION_TYPE_OFFLINE:
        settings->SetLazyFrameLoadingDistanceThresholdPxOffline(
            ect_distance_pair.second);
        continue;
      case net::EFFECTIVE_CONNECTION_TYPE_SLOW_2G:
        settings->SetLazyFrameLoadingDistanceThresholdPxSlow2G(
            ect_distance_pair.second);
        continue;
      case net::EFFECTIVE_CONNECTION_TYPE_2G:
        settings->SetLazyFrameLoadingDistanceThresholdPx2G(
            ect_distance_pair.second);
        continue;
      case net::EFFECTIVE_CONNECTION_TYPE_3G:
        settings->SetLazyFrameLoadingDistanceThresholdPx3G(
            ect_distance_pair.second);
        continue;
      case net::EFFECTIVE_CONNECTION_TYPE_4G:
        settings->SetLazyFrameLoadingDistanceThresholdPx4G(
            ect_distance_pair.second);
        continue;
      case net::EFFECTIVE_CONNECTION_TYPE_LAST:
        continue;
    }
    NOTREACHED();
  }

  for (const auto& ect_distance_pair :
       prefs.lazy_image_loading_distance_thresholds_px) {
    switch (ect_distance_pair.first) {
      case net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN:
        settings->SetLazyImageLoadingDistanceThresholdPxUnknown(
            ect_distance_pair.second);
        continue;
      case net::EFFECTIVE_CONNECTION_TYPE_OFFLINE:
        settings->SetLazyImageLoadingDistanceThresholdPxOffline(
            ect_distance_pair.second);
        continue;
      case net::EFFECTIVE_CONNECTION_TYPE_SLOW_2G:
        settings->SetLazyImageLoadingDistanceThresholdPxSlow2G(
            ect_distance_pair.second);
        continue;
      case net::EFFECTIVE_CONNECTION_TYPE_2G:
        settings->SetLazyImageLoadingDistanceThresholdPx2G(
            ect_distance_pair.second);
        continue;
      case net::EFFECTIVE_CONNECTION_TYPE_3G:
        settings->SetLazyImageLoadingDistanceThresholdPx3G(
            ect_distance_pair.second);
        continue;
      case net::EFFECTIVE_CONNECTION_TYPE_4G:
        settings->SetLazyImageLoadingDistanceThresholdPx4G(
            ect_distance_pair.second);
        continue;
      case net::EFFECTIVE_CONNECTION_TYPE_LAST:
        continue;
    }
    NOTREACHED();
  }

#if defined(OS_MACOSX)
  web_view->SetMaximumLegibleScale(prefs.default_maximum_page_scale_factor);
#endif

#if defined(OS_WIN)
  WebRuntimeFeatures::EnableMiddleClickAutoscroll(true);
#endif

  // Only enable href translate if we have the experimental web platform
  // flag on and the translation service is available.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableExperimentalWebPlatformFeatures)) {
    WebRuntimeFeatures::EnableHrefTranslate(prefs.translate_service_available);
  }
}

/*static*/
RenderViewImpl* RenderViewImpl::Create(
    CompositorDependencies* compositor_deps,
    mojom::CreateViewParamsPtr params,
    RenderWidget::ShowCallback show_callback,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  DCHECK(params->view_id != MSG_ROUTING_NONE);
  DCHECK(params->main_frame_widget_routing_id != MSG_ROUTING_NONE);
  RenderViewImpl* render_view;
  if (g_create_render_view_impl)
    render_view = g_create_render_view_impl(compositor_deps, *params);
  else
    render_view = new RenderViewImpl(compositor_deps, *params);

  render_view->Initialize(std::move(params), std::move(show_callback),
                          std::move(task_runner));
  return render_view;
}

// static
void RenderViewImpl::InstallCreateHook(RenderViewImpl* (
    *create_render_view_impl)(CompositorDependencies* compositor_deps,
                              const mojom::CreateViewParams&)) {
  CHECK(!g_create_render_view_impl);
  g_create_render_view_impl = create_render_view_impl;
}

void RenderViewImpl::AddObserver(RenderViewObserver* observer) {
  observers_.AddObserver(observer);
}

void RenderViewImpl::RemoveObserver(RenderViewObserver* observer) {
  observer->RenderViewGone();
  observers_.RemoveObserver(observer);
}

blink::WebView* RenderViewImpl::webview() {
  return webview_;
}

const blink::WebView* RenderViewImpl::webview() const {
  return webview_;
}

// RenderWidgetOwnerDelegate -----------------------------------------

blink::WebWidget* RenderViewImpl::GetWebWidgetForWidget() const {
  return frame_widget_;
}

bool RenderViewImpl::RenderWidgetWillHandleMouseEventForWidget(
    const blink::WebMouseEvent& event) {
  // If the mouse is locked, only the current owner of the mouse lock can
  // process mouse events.
  return GetWidget()->mouse_lock_dispatcher()->WillHandleMouseEvent(event);
}

void RenderViewImpl::SetActiveForWidget(bool active) {
  if (webview())
    webview()->SetIsActive(active);
}

bool RenderViewImpl::SupportsMultipleWindowsForWidget() {
  return webkit_preferences_.supports_multiple_windows;
}

void RenderViewImpl::DidHandleGestureEventForWidget(
    const WebGestureEvent& event) {
  for (auto& observer : observers_)
    observer.DidHandleGestureEvent(event);
}

void RenderViewImpl::OverrideCloseForWidget() {
  DCHECK(frame_widget_);
  frame_widget_->Close();
  frame_widget_ = nullptr;
}

void RenderViewImpl::DidCloseWidget() {
  // The webview_ is already destroyed by the time we get here, remove any
  // references to it.
  g_view_map.Get().erase(webview_);
  webview_ = nullptr;
  g_routing_id_view_map.Get().erase(GetRoutingID());
}

void RenderViewImpl::ApplyNewSizeForWidget(const gfx::Size& old_size,
                                           const gfx::Size& new_size) {
  if (new_size != old_size) {
    // Only hide popups when the size changes. There are situations (e.g. hiding
    // the ChromeOS virtual keyboard) where we send a resize message with no
    // change in size, but we don't want to close popups.
    // See https://crbug.com/761908.
    webview()->HidePopups();
  }
}

void RenderViewImpl::ApplyNewDisplayModeForWidget(
    const blink::WebDisplayMode& new_display_mode) {
  webview()->SetDisplayMode(new_display_mode);
}

void RenderViewImpl::ApplyAutoResizeLimitsForWidget(const gfx::Size& min_size,
                                                    const gfx::Size& max_size) {
  webview()->EnableAutoResizeMode(min_size, max_size);
}

void RenderViewImpl::DisableAutoResizeForWidget() {
  webview()->DisableAutoResizeMode();
}

void RenderViewImpl::ScrollFocusedNodeIntoViewForWidget() {
  if (WebLocalFrame* focused_frame = GetWebView()->FocusedFrame()) {
    auto* frame_widget = focused_frame->LocalRoot()->FrameWidget();
    frame_widget->ScrollFocusedEditableElementIntoView();
  }
}

void RenderViewImpl::DidReceiveSetFocusEventForWidget() {
  // This message must always be received when the main frame is a
  // WebLocalFrame.
  // TODO(ajwong): Can this be removed and just check |owner_delegate_| in
  // RenderWidget instead?
  CHECK(webview()->MainFrame()->IsWebLocalFrame());
}

void RenderViewImpl::DidChangeFocusForWidget() {
  // Notify all BrowserPlugins of the RenderView's focus state.
  if (BrowserPluginManager::Get())
    BrowserPluginManager::Get()->UpdateFocusState();
}

void RenderViewImpl::DidCommitCompositorFrameForWidget() {
  for (auto& observer : observers_)
    observer.DidCommitCompositorFrame();
  UpdatePreferredSize();
}

void RenderViewImpl::DidCompletePageScaleAnimationForWidget() {
  if (auto* focused_frame = GetWebView()->FocusedFrame()) {
    if (focused_frame->AutofillClient())
      focused_frame->AutofillClient()->DidCompleteFocusChangeInFrame();
  }
}

void RenderViewImpl::ResizeWebWidgetForWidget(
    const gfx::Size& size,
    float top_controls_height,
    float bottom_controls_height,
    bool browser_controls_shrink_blink_size) {
  webview()->ResizeWithBrowserControls(size, top_controls_height,
                                       bottom_controls_height,
                                       browser_controls_shrink_blink_size);
}

void RenderViewImpl::RequestScheduleAnimationForWidget() {
  // Schedule the animation on the WidgetClient() which may not be the
  // RenderWidget directly in layout tests.
  WidgetClient()->ScheduleAnimation();
}

void RenderViewImpl::SetScreenMetricsEmulationParametersForWidget(
    bool enabled,
    const blink::WebDeviceEmulationParams& params) {
  if (enabled)
    webview()->EnableDeviceEmulation(params);
  else
    webview()->DisableDeviceEmulation();
}

// IPC message handlers -----------------------------------------

void RenderViewImpl::OnUpdateTargetURLAck() {
  // Check if there is a targeturl waiting to be sent.
  if (target_url_status_ == TARGET_PENDING)
    Send(new ViewHostMsg_UpdateTargetURL(GetRoutingID(), pending_target_url_));

  target_url_status_ = TARGET_NONE;
}

void RenderViewImpl::OnSetHistoryOffsetAndLength(int history_offset,
                                                 int history_length) {
  // -1 <= history_offset < history_length <= kMaxSessionHistoryEntries(50).
  DCHECK_LE(-1, history_offset);
  DCHECK_LT(history_offset, history_length);
  DCHECK_LE(history_length, kMaxSessionHistoryEntries);

  history_list_offset_ = history_offset;
  history_list_length_ = history_length;
}

void RenderViewImpl::OnSetInitialFocus(bool reverse) {
  if (!webview())
    return;
  webview()->SetInitialFocus(reverse);
}

void RenderViewImpl::OnUpdateWindowScreenRect(gfx::Rect window_screen_rect) {
  // Defers to the RenderWidget message handler.
  GetWidget()->SetWindowScreenRect(window_screen_rect);
}

void RenderViewImpl::OnAudioStateChanged(bool is_audio_playing) {
  webview()->AudioStateChanged(is_audio_playing);
}

void RenderViewImpl::OnPausePageScheduledTasks(bool paused) {
  webview()->PausePageScheduledTasks(paused);
}

///////////////////////////////////////////////////////////////////////////////

void RenderViewImpl::ShowCreatedPopupWidget(RenderWidget* popup_widget,
                                            WebNavigationPolicy policy,
                                            const gfx::Rect& initial_rect) {
  Send(new ViewHostMsg_ShowWidget(GetRoutingID(), popup_widget->routing_id(),
                                  initial_rect));
}

void RenderViewImpl::ShowCreatedFullscreenWidget(
    RenderWidget* fullscreen_widget,
    WebNavigationPolicy policy,
    const gfx::Rect& initial_rect) {
  Send(new ViewHostMsg_ShowFullscreenWidget(GetRoutingID(),
                                            fullscreen_widget->routing_id()));
}

void RenderViewImpl::SendFrameStateUpdates() {
  // Tell each frame with pending state to send its UpdateState message.
  for (int render_frame_routing_id : frames_with_pending_state_) {
    RenderFrameImpl* frame =
        RenderFrameImpl::FromRoutingID(render_frame_routing_id);
    if (frame)
      frame->SendUpdateState();
  }
  frames_with_pending_state_.clear();
}

// IPC::Listener -------------------------------------------------------------

bool RenderViewImpl::OnMessageReceived(const IPC::Message& message) {
  WebFrame* main_frame = webview() ? webview()->MainFrame() : nullptr;
  if (main_frame) {
    GURL active_url;
    if (main_frame->IsWebLocalFrame())
      active_url = main_frame->ToWebLocalFrame()->GetDocument().Url();
    GetContentClient()->SetActiveURL(
        active_url, main_frame->Top()->GetSecurityOrigin().ToString().Utf8());
  }

  for (auto& observer : observers_) {
    if (observer.OnMessageReceived(message))
      return true;
  }

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(RenderViewImpl, message)
    IPC_MESSAGE_HANDLER(ViewMsg_SetPageScale, OnSetPageScale)
    IPC_MESSAGE_HANDLER(ViewMsg_SetInitialFocus, OnSetInitialFocus)
    IPC_MESSAGE_HANDLER(ViewMsg_UpdateTargetURL_ACK, OnUpdateTargetURLAck)
    IPC_MESSAGE_HANDLER(ViewMsg_UpdateWebPreferences, OnUpdateWebPreferences)
    IPC_MESSAGE_HANDLER(ViewMsg_EnumerateDirectoryResponse,
                        OnEnumerateDirectoryResponse)
    IPC_MESSAGE_HANDLER(ViewMsg_ClosePage, OnClosePage)
    IPC_MESSAGE_HANDLER(ViewMsg_MoveOrResizeStarted, OnMoveOrResizeStarted)
    IPC_MESSAGE_HANDLER(ViewMsg_EnablePreferredSizeChangedMode,
                        OnEnablePreferredSizeChangedMode)
    IPC_MESSAGE_HANDLER(ViewMsg_SetRendererPrefs, OnSetRendererPrefs)
    IPC_MESSAGE_HANDLER(ViewMsg_PluginActionAt, OnPluginActionAt)

    // Page messages.
    IPC_MESSAGE_HANDLER(PageMsg_UpdateWindowScreenRect,
                        OnUpdateWindowScreenRect)
    IPC_MESSAGE_HANDLER(PageMsg_WasHidden, OnPageWasHidden)
    IPC_MESSAGE_HANDLER(PageMsg_WasShown, OnPageWasShown)
    IPC_MESSAGE_HANDLER(PageMsg_SetHistoryOffsetAndLength,
                        OnSetHistoryOffsetAndLength)
    IPC_MESSAGE_HANDLER(PageMsg_AudioStateChanged, OnAudioStateChanged)
    IPC_MESSAGE_HANDLER(PageMsg_PausePageScheduledTasks,
                        OnPausePageScheduledTasks)
    IPC_MESSAGE_HANDLER(PageMsg_UpdateScreenInfo, OnUpdateScreenInfo)
    IPC_MESSAGE_HANDLER(PageMsg_SetPageFrozen, SetPageFrozen)

    // Adding a new message? Add platform independent ones first, then put the
    // platform specific ones at the end.

    // Have the super handle all other messages.
    IPC_MESSAGE_UNHANDLED(handled = RenderWidget::OnMessageReceived(message))
  IPC_END_MESSAGE_MAP()

  return handled;
}

// blink::WebViewClient ------------------------------------------------------

// TODO(csharrison): Migrate this method to WebLocalFrameClient /
// RenderFrameImpl, as it is now serviced by a mojo interface scoped to the
// opener frame.
WebView* RenderViewImpl::CreateView(
    WebLocalFrame* creator,
    const WebURLRequest& request,
    const WebWindowFeatures& features,
    const WebString& frame_name,
    WebNavigationPolicy policy,
    bool suppress_opener,
    WebSandboxFlags sandbox_flags,
    const blink::SessionStorageNamespaceId& session_storage_namespace_id) {
  RenderFrameImpl* creator_frame = RenderFrameImpl::FromWebFrame(creator);
  mojom::CreateNewWindowParamsPtr params = mojom::CreateNewWindowParams::New();

  // User Activation v2 moves user gesture checks to the browser process, with
  // the exception of the extensions case handled through the following |if|.
  params->mimic_user_gesture =
      base::FeatureList::IsEnabled(features::kUserActivationV2)
          ? false
          : WebUserGestureIndicator::IsProcessingUserGesture(creator);
  if (GetContentClient()->renderer()->AllowPopup())
    params->mimic_user_gesture = true;

  params->window_container_type = WindowFeaturesToContainerType(features);

  params->session_storage_namespace_id = session_storage_namespace_id;
  // TODO(dmurph): Don't copy session storage when features.noopener is true:
  // https://html.spec.whatwg.org/multipage/browsers.html#copy-session-storage
  // https://crbug.com/771959
  params->clone_from_session_storage_namespace_id =
      session_storage_namespace_id_;

  const std::string& frame_name_utf8 = frame_name.Utf8(
      WebString::UTF8ConversionMode::kStrictReplacingErrorsWithFFFD);
  params->frame_name = frame_name_utf8;
  params->opener_suppressed = suppress_opener;
  params->disposition = NavigationPolicyToDisposition(policy);
  if (!request.IsNull()) {
    params->target_url = request.Url();
    params->referrer = GetReferrerFromRequest(creator, request);
  }
  params->features = ConvertWebWindowFeaturesToMojoWindowFeatures(features);

  // We preserve this information before sending the message since |params| is
  // moved on send.
  bool is_background_tab =
      params->disposition == WindowOpenDisposition::NEW_BACKGROUND_TAB;
  bool opened_by_user_gesture = params->mimic_user_gesture;

  mojom::CreateNewWindowStatus status;
  mojom::CreateNewWindowReplyPtr reply;
  auto* frame_host = creator_frame->GetFrameHost();
  bool err = !frame_host->CreateNewWindow(std::move(params), &status, &reply);
  if (err || status == mojom::CreateNewWindowStatus::kIgnore)
    return nullptr;

  // For Android WebView, we support a pop-up like behavior for window.open()
  // even if the embedding app doesn't support multiple windows. In this case,
  // window.open() will return "window" and navigate it to whatever URL was
  // passed. We also don't need to consume user gestures to protect against
  // multiple windows being opened, because, well, the app doesn't support
  // multiple windows.
  // TODO(dcheng): It's awkward that this is plumbed into Blink but not really
  // used much in Blink, except to enable layout testing... perhaps this should
  // be checked directly in the browser side.
  if (status == mojom::CreateNewWindowStatus::kReuse)
    return webview();

  DCHECK(reply);
  DCHECK_NE(MSG_ROUTING_NONE, reply->route_id);
  DCHECK_NE(MSG_ROUTING_NONE, reply->main_frame_route_id);
  DCHECK_NE(MSG_ROUTING_NONE, reply->main_frame_widget_route_id);

  // The browser allowed creation of a new window and consumed the user
  // activation (UAv2 only).
  WebUserGestureIndicator::ConsumeUserGesture(
      creator, blink::UserActivationUpdateSource::kBrowser);

  // While this view may be a background extension page, it can spawn a visible
  // render view. So we just assume that the new one is not another background
  // page instead of passing on our own value.
  // TODO(vangelis): Can we tell if the new view will be a background page?
  bool never_visible = false;

  VisualProperties visual_properties = VisualProperties();
  visual_properties.screen_info = GetWidget()->screen_info();

  // The initial hidden state for the RenderViewImpl here has to match what the
  // browser will eventually decide for the given disposition. Since we have to
  // return from this call synchronously, we just have to make our best guess
  // and rely on the browser sending a WasHidden / WasShown message if it
  // disagrees.
  mojom::CreateViewParamsPtr view_params = mojom::CreateViewParams::New();

  view_params->opener_frame_route_id = creator_frame->GetRoutingID();
  DCHECK_EQ(GetRoutingID(), creator_frame->render_view()->GetRoutingID());

  view_params->window_was_created_with_opener = true;
  view_params->renderer_preferences = renderer_preferences_;
  view_params->web_preferences = webkit_preferences_;
  view_params->view_id = reply->route_id;
  view_params->main_frame_routing_id = reply->main_frame_route_id;
  view_params->main_frame_interface_provider =
      std::move(reply->main_frame_interface_provider);
  view_params->main_frame_widget_routing_id = reply->main_frame_widget_route_id;
  view_params->session_storage_namespace_id =
      reply->cloned_session_storage_namespace_id;
  DCHECK(!view_params->session_storage_namespace_id.empty())
      << "Session storage namespace must be populated.";
  view_params->swapped_out = false;
  view_params->replicated_frame_state.frame_policy.sandbox_flags =
      sandbox_flags;
  view_params->replicated_frame_state.name = frame_name_utf8;
  view_params->devtools_main_frame_token = reply->devtools_main_frame_token;
  // Even if the main frame has a name, the main frame's unique name is always
  // the empty string.
  view_params->hidden = is_background_tab;
  view_params->never_visible = never_visible;
  view_params->visual_properties = visual_properties;

  // Unretained() is safe here because our calling function will also call
  // show().
  RenderWidget::ShowCallback show_callback =
      base::BindOnce(&RenderFrameImpl::ShowCreatedWindow,
                     base::Unretained(creator_frame), opened_by_user_gesture);

  RenderViewImpl* view = RenderViewImpl::Create(
      GetWidget()->compositor_deps(), std::move(view_params),
      std::move(show_callback),
      creator->GetTaskRunner(blink::TaskType::kInternalDefault));

  return view->webview();
}

WebWidget* RenderViewImpl::CreatePopup(blink::WebLocalFrame* creator) {
  mojom::WidgetPtr widget_channel;
  mojom::WidgetRequest widget_channel_request =
      mojo::MakeRequest(&widget_channel);

  // Do a synchronous IPC to obtain a routing ID.
  int32_t widget_routing_id = MSG_ROUTING_NONE;
  bool success =
      RenderThreadImpl::current_render_message_filter()->CreateNewWidget(
          GetRoutingID(), std::move(widget_channel), &widget_routing_id);
  if (!success) {
    // When the renderer is being killed the mojo message will fail.
    return nullptr;
  }

  RenderWidget::ShowCallback opener_callback = base::BindOnce(
      &RenderViewImpl::ShowCreatedPopupWidget, weak_ptr_factory_.GetWeakPtr());

  // The RenderWidget associated with the RenderView. This should be the
  // RenderWidget for the main frame, but may be a zombie RenderWidget when
  // the main frame is remote (we don't need a RenderWidget for it then).
  // However for now (https://crbug.com/419087) we know it exists and grab
  // state off it for the popup.
  // TODO(crbug.com/419087): This should probably be using the local root's
  // RenderWidget for the frame making the popup.
  RenderWidget* view_render_widget = GetWidget();

  auto popup_widget = base::MakeRefCounted<RenderWidget>(
      widget_routing_id, view_render_widget->compositor_deps(),
      WidgetType::kPopup, view_render_widget->screen_info(),
      blink::kWebDisplayModeUndefined,
      /*swapped_out=*/false,
      /*hidden=*/false,
      /*never_visible=*/false, std::move(widget_channel_request));

  // The returned WebPagePopup is self-referencing, so the pointer here is not
  // an owning pointer.
  blink::WebPagePopup* popup_web_widget =
      blink::WebPagePopup::Create(popup_widget.get());

  // Adds a self-reference on the |popup_widget| so it will not be destroyed
  // when leaving scope. The WebPagePopup takes responsibility for Close()ing
  // and thus destroying the RenderWidget.
  popup_widget->InitForPopup(std::move(opener_callback), popup_web_widget);
  // TODO(crbug.com/419087): RenderWidget has some weird logic for picking a
  // WebWidget which doesn't apply to this case. So we verify. This can go away
  // when RenderWidget::GetWebWidget() is just a simple accessor.
  DCHECK_EQ(popup_widget->GetWebWidget(), popup_web_widget);

  // Devtools emulation, which may be currently applied to the
  // |view_render_widget|, should also apply to the new popup. This doesn't
  // happen automatically.
  popup_widget->ApplyEmulatedScreenMetricsForPopupWidget(view_render_widget);

  return popup_web_widget;
}

base::StringPiece RenderViewImpl::GetSessionStorageNamespaceId() {
  CHECK(!session_storage_namespace_id_.empty());
  return session_storage_namespace_id_;
}

void RenderViewImpl::PrintPage(WebLocalFrame* frame) {
  UMA_HISTOGRAM_BOOLEAN("PrintPreview.InitiatedByScript",
                        frame->Top() == frame);

  // Logging whether the top frame is remote is sufficient in this case. If
  // the top frame is local, the printing code will function correctly and
  // the frame itself will be printed, so the cases this histogram tracks is
  // where printing of a subframe will fail as of now.
  UMA_HISTOGRAM_BOOLEAN("PrintPreview.OutOfProcessSubframe",
                        frame->Top()->IsWebRemoteFrame());

  RenderFrameImpl::FromWebFrame(frame)->ScriptedPrint(
      GetWidget()->input_handler().handling_input_event());
}

bool RenderViewImpl::EnumerateChosenDirectory(
    const WebString& path,
    WebFileChooserCompletion* chooser_completion) {
  int id = enumeration_completion_id_++;
  enumeration_completions_[id] = chooser_completion;
  return Send(new ViewHostMsg_EnumerateDirectory(
      GetRoutingID(), id, blink::WebStringToFilePath(path)));
}

void RenderViewImpl::AttachWebFrameWidget(blink::WebFrameWidget* frame_widget) {
  // The previous WebFrameWidget must already be detached by CloseForFrame().
  DCHECK(!frame_widget_);
  frame_widget_ = frame_widget;
}

void RenderViewImpl::SetZoomLevel(double zoom_level) {
  // If we change the zoom level for the view, make sure any subsequent subframe
  // loads reflect the current zoom level.
  page_zoom_level_ = zoom_level;

  webview()->SetZoomLevel(zoom_level);
  for (auto& observer : observers_)
    observer.OnZoomLevelChanged();
}

void RenderViewImpl::SetValidationMessageDirection(
    base::string16* wrapped_main_text,
    blink::WebTextDirection main_text_hint,
    base::string16* wrapped_sub_text,
    blink::WebTextDirection sub_text_hint) {
  if (main_text_hint == blink::kWebTextDirectionLeftToRight) {
    *wrapped_main_text =
        base::i18n::GetDisplayStringInLTRDirectionality(*wrapped_main_text);
  } else if (main_text_hint == blink::kWebTextDirectionRightToLeft &&
             !base::i18n::IsRTL()) {
    base::i18n::WrapStringWithRTLFormatting(wrapped_main_text);
  }

  if (!wrapped_sub_text->empty()) {
    if (sub_text_hint == blink::kWebTextDirectionLeftToRight) {
      *wrapped_sub_text =
          base::i18n::GetDisplayStringInLTRDirectionality(*wrapped_sub_text);
    } else if (sub_text_hint == blink::kWebTextDirectionRightToLeft) {
      base::i18n::WrapStringWithRTLFormatting(wrapped_sub_text);
    }
  }
}

void RenderViewImpl::UpdateTargetURL(const GURL& url,
                                     const GURL& fallback_url) {
  GURL latest_url = url.is_empty() ? fallback_url : url;
  if (latest_url == target_url_)
    return;

  // Tell the browser to display a destination link.
  if (target_url_status_ == TARGET_INFLIGHT ||
      target_url_status_ == TARGET_PENDING) {
    // If we have a request in-flight, save the URL to be sent when we
    // receive an ACK to the in-flight request. We can happily overwrite
    // any existing pending sends.
    pending_target_url_ = latest_url;
    target_url_status_ = TARGET_PENDING;
  } else {
    // URLs larger than |kMaxURLChars| cannot be sent through IPC -
    // see |ParamTraits<GURL>|.
    if (latest_url.possibly_invalid_spec().size() > url::kMaxURLChars)
      latest_url = GURL();
    Send(new ViewHostMsg_UpdateTargetURL(GetRoutingID(), latest_url));
    target_url_ = latest_url;
    target_url_status_ = TARGET_INFLIGHT;
  }
}

void RenderViewImpl::StartNavStateSyncTimerIfNecessary(RenderFrameImpl* frame) {
  // Keep track of which frames have pending updates.
  frames_with_pending_state_.insert(frame->GetRoutingID());

  int delay;
  if (send_content_state_immediately_)
    delay = 0;
  else if (is_hidden())
    delay = kDelaySecondsForContentStateSyncHidden;
  else
    delay = kDelaySecondsForContentStateSync;

  if (nav_state_sync_timer_.IsRunning()) {
    // The timer is already running. If the delay of the timer maches the amount
    // we want to delay by, then return. Otherwise stop the timer so that it
    // gets started with the right delay.
    if (nav_state_sync_timer_.GetCurrentDelay().InSeconds() == delay)
      return;
    nav_state_sync_timer_.Stop();
  }

  // Tell each frame with pending state to inform the browser.
  nav_state_sync_timer_.Start(FROM_HERE, TimeDelta::FromSeconds(delay), this,
                              &RenderViewImpl::SendFrameStateUpdates);
}

void RenderViewImpl::SetMouseOverURL(const WebURL& url) {
  mouse_over_url_ = GURL(url);
  UpdateTargetURL(mouse_over_url_, focus_url_);
}

void RenderViewImpl::SetKeyboardFocusURL(const WebURL& url) {
  focus_url_ = GURL(url);
  UpdateTargetURL(focus_url_, mouse_over_url_);
}

bool RenderViewImpl::AcceptsLoadDrops() {
  return renderer_preferences_.can_accept_load_drops;
}

void RenderViewImpl::FocusNext() {
  Send(new ViewHostMsg_TakeFocus(GetRoutingID(), false));
}

void RenderViewImpl::FocusPrevious() {
  Send(new ViewHostMsg_TakeFocus(GetRoutingID(), true));
}

// TODO(esprehn): Blink only ever passes Elements, this should take WebElement.
void RenderViewImpl::FocusedNodeChanged(const WebNode& fromNode,
                                        const WebNode& toNode) {
  RenderFrameImpl* previous_frame = nullptr;
  if (!fromNode.IsNull())
    previous_frame =
        RenderFrameImpl::FromWebFrame(fromNode.GetDocument().GetFrame());
  RenderFrameImpl* new_frame = nullptr;
  if (!toNode.IsNull())
    new_frame = RenderFrameImpl::FromWebFrame(toNode.GetDocument().GetFrame());

  if (previous_frame && previous_frame != new_frame)
    previous_frame->FocusedNodeChanged(WebNode());
  if (new_frame)
    new_frame->FocusedNodeChanged(toNode);

  // TODO(dmazzoni): remove once there's a separate a11y tree per frame.
  if (main_render_frame_)
    main_render_frame_->FocusedNodeChangedForAccessibility(toNode);
}

void RenderViewImpl::DidUpdateMainFrameLayout() {
  for (auto& observer : observers_)
    observer.DidUpdateMainFrameLayout();

  // The main frame may have changed size.
  needs_preferred_size_update_ = true;
}

void RenderViewImpl::NavigateBackForwardSoon(int offset,
                                             bool has_user_gesture) {
  history_navigation_virtual_time_pauser_ =
      RenderThreadImpl::current()
          ->GetWebMainThreadScheduler()
          ->CreateWebScopedVirtualTimePauser(
              "NavigateBackForwardSoon",
              blink::WebScopedVirtualTimePauser::VirtualTaskDuration::kInstant);
  history_navigation_virtual_time_pauser_.PauseVirtualTime();
  Send(new ViewHostMsg_GoToEntryAtOffset(GetRoutingID(), offset,
                                         has_user_gesture));
}

void RenderViewImpl::DidCommitProvisionalHistoryLoad() {
  history_navigation_virtual_time_pauser_.UnpauseVirtualTime();
}

void RenderViewImpl::RegisterRendererPreferenceWatcherForWorker(
    mojom::RendererPreferenceWatcherPtr watcher) {
  renderer_preference_watchers_.AddPtr(std::move(watcher));
}

int RenderViewImpl::HistoryBackListCount() {
  return history_list_offset_ < 0 ? 0 : history_list_offset_;
}

int RenderViewImpl::HistoryForwardListCount() {
  return history_list_length_ - HistoryBackListCount() - 1;
}

// blink::WebWidgetClient ----------------------------------------------------

bool RenderViewImpl::CanHandleGestureEvent() {
  return true;
}

bool RenderViewImpl::CanUpdateLayout() {
  return true;
}

blink::WebWidgetClient* RenderViewImpl::WidgetClient() {
  return this;
}

// blink::WebLocalFrameClient
// -----------------------------------------------------

void RenderViewImpl::SetEditCommandForNextKeyEvent(const std::string& name,
                                                   const std::string& value) {
  GetWidget()->SetEditCommandForNextKeyEvent(name, value);
}

void RenderViewImpl::ClearEditCommands() {
  GetWidget()->ClearEditCommands();
}

const std::string& RenderViewImpl::GetAcceptLanguages() const {
  return renderer_preferences_.accept_languages;
}

void RenderViewImpl::UpdateBrowserControlsState(
    BrowserControlsState constraints,
    BrowserControlsState current,
    bool animate) {
  if (GetWidget() && GetWidget()->layer_tree_view()) {
    GetWidget()->layer_tree_view()->UpdateBrowserControlsState(
        ContentToCc(constraints), ContentToCc(current), animate);
  }

  top_controls_constraints_ = constraints;
}

#if defined(OS_ANDROID) || defined(OS_CHROMEOS)

void RenderViewImpl::didScrollWithKeyboard(const blink::WebSize& delta) {
  if (delta.height == 0)
    return;

  BrowserControlsState current = delta.height < 0
                                     ? BROWSER_CONTROLS_STATE_SHOWN
                                     : BROWSER_CONTROLS_STATE_HIDDEN;

  UpdateBrowserControlsState(top_controls_constraints_, current, true);
}

#endif

void RenderViewImpl::ConvertViewportToWindowViaWidget(blink::WebRect* rect) {
  WidgetClient()->ConvertViewportToWindow(rect);
}

gfx::RectF RenderViewImpl::ElementBoundsInWindow(
    const blink::WebElement& element) {
  blink::WebRect bounding_box_in_window = element.BoundsInViewport();
  WidgetClient()->ConvertViewportToWindow(&bounding_box_in_window);
  return gfx::RectF(bounding_box_in_window);
}

void RenderViewImpl::UpdatePreferredSize() {
  // We don't always want to send the change messages over IPC, only if we've
  // been put in that mode by getting a |ViewMsg_EnablePreferredSizeChangedMode|
  // message.
  if (!send_preferred_size_changes_ || !webview())
    return;

  if (!needs_preferred_size_update_)
    return;
  needs_preferred_size_update_ = false;

  blink::WebSize tmp_size = webview()->ContentsPreferredMinimumSize();
  blink::WebRect tmp_rect(0, 0, tmp_size.width, tmp_size.height);
  WidgetClient()->ConvertViewportToWindow(&tmp_rect);
  gfx::Size size(tmp_rect.width, tmp_rect.height);
  if (size == preferred_size_)
    return;

  preferred_size_ = size;
  Send(new ViewHostMsg_DidContentsPreferredSizeChange(GetRoutingID(),
                                                      preferred_size_));
}

blink::WebString RenderViewImpl::AcceptLanguages() {
  return WebString::FromUTF8(renderer_preferences_.accept_languages);
}

// RenderView implementation ---------------------------------------------------

bool RenderViewImpl::Send(IPC::Message* message) {
  // This method is an override of IPC::Sender, but RenderWidget also has an
  // override of IPC::Sender, so this method also overrides RenderWidget. Thus
  // we must call to the base class, not via an upcast or virtual dispatch would
  // go back here.
  return RenderWidget::Send(message);
}

RenderWidget* RenderViewImpl::GetWidget() {
  return this;
}

RenderFrameImpl* RenderViewImpl::GetMainRenderFrame() {
  return main_render_frame_;
}

int RenderViewImpl::GetRoutingID() const {
  return routing_id_;
}

gfx::Size RenderViewImpl::GetSize() const {
  return size();
}

float RenderViewImpl::GetDeviceScaleFactor() const {
  return GetWebScreenInfo().device_scale_factor;
}

float RenderViewImpl::GetZoomLevel() const {
  return page_zoom_level_;
}

const WebPreferences& RenderViewImpl::GetWebkitPreferences() {
  return webkit_preferences_;
}

void RenderViewImpl::SetWebkitPreferences(const WebPreferences& preferences) {
  OnUpdateWebPreferences(preferences);
}

blink::WebView* RenderViewImpl::GetWebView() {
  return webview();
}

blink::WebFrameWidget* RenderViewImpl::GetWebFrameWidget() {
  return frame_widget_;
}

bool RenderViewImpl::GetContentStateImmediately() const {
  return send_content_state_immediately_;
}

void RenderViewImpl::OnSetPageScale(float page_scale_factor) {
  if (!webview())
    return;
  webview()->SetPageScaleFactor(page_scale_factor);
}

void RenderViewImpl::UpdateZoomLevel(double zoom_level) {
  webview()->HidePopups();
  SetZoomLevel(zoom_level);
}

void RenderViewImpl::OnUpdateWebPreferences(const WebPreferences& prefs) {
  webkit_preferences_ = prefs;
  ApplyWebPreferences(webkit_preferences_, webview());
}

void RenderViewImpl::OnEnumerateDirectoryResponse(
    int id,
    const std::vector<base::FilePath>& paths) {
  if (!enumeration_completions_[id])
    return;

  WebVector<WebString> ws_file_names(paths.size());
  for (size_t i = 0; i < paths.size(); ++i)
    ws_file_names[i] = blink::FilePathToWebString(paths[i]);

  enumeration_completions_[id]->DidChooseFile(ws_file_names);
  enumeration_completions_.erase(id);
}

void RenderViewImpl::OnEnablePreferredSizeChangedMode() {
  if (send_preferred_size_changes_)
    return;
  send_preferred_size_changes_ = true;

  if (!webview())
    return;

  needs_preferred_size_update_ = true;

  // We need to ensure |UpdatePreferredSize| gets called. If a layout is needed,
  // force an update here which will call |DidUpdateMainFrameLayout|.
  webview()->UpdateLifecycle(WebWidget::LifecycleUpdate::kLayout);

  // If a layout was not needed, |DidUpdateMainFrameLayout| will not be called.
  // We explicitly update the preferred size here to ensure the preferred size
  // notification is sent.
  UpdatePreferredSize();
}

void RenderViewImpl::OnSetRendererPrefs(
    const RendererPreferences& renderer_prefs) {
  std::string old_accept_languages = renderer_preferences_.accept_languages;

  renderer_preferences_ = renderer_prefs;

  renderer_preference_watchers_.ForAllPtrs(
      [&renderer_prefs](mojom::RendererPreferenceWatcher* watcher) {
        watcher->NotifyUpdate(renderer_prefs);
      });

  UpdateFontRenderingFromRendererPrefs();
  UpdateThemePrefs();
  blink::SetCaretBlinkInterval(renderer_prefs.caret_blink_interval);

#if BUILDFLAG(USE_DEFAULT_RENDER_THEME)
  if (renderer_prefs.use_custom_colors) {
    blink::SetFocusRingColor(renderer_prefs.focus_ring_color);

    if (webview()) {
      webview()->SetSelectionColors(renderer_prefs.active_selection_bg_color,
                                    renderer_prefs.active_selection_fg_color,
                                    renderer_prefs.inactive_selection_bg_color,
                                    renderer_prefs.inactive_selection_fg_color);
      webview()->ThemeChanged();
    }
  }
#endif  // BUILDFLAG(USE_DEFAULT_RENDER_THEME)

  if (webview() &&
      old_accept_languages != renderer_preferences_.accept_languages) {
    webview()->AcceptLanguagesChanged();
  }
}

void RenderViewImpl::OnPluginActionAt(const gfx::Point& location,
                                      const WebPluginAction& action) {
  if (webview())
    webview()->PerformPluginAction(action, location);
}

void RenderViewImpl::OnClosePage() {
  // ViewMsg_ClosePage should only be sent to active, non-swapped-out views.
  DCHECK(webview()->MainFrame()->IsWebLocalFrame());

  // TODO(creis): We'd rather use webview()->Close() here, but that currently
  // sets the WebView's delegate_ to NULL, preventing any JavaScript dialogs
  // in the onunload handler from appearing.  For now, we're bypassing that and
  // calling the FrameLoader's CloseURL method directly.  This should be
  // revisited to avoid having two ways to close a page.  Having a single way
  // to close that can run onunload is also useful for fixing
  // http://b/issue?id=753080.
  webview()->MainFrame()->ToWebLocalFrame()->DispatchUnloadEvent();

  Send(new ViewHostMsg_ClosePage_ACK(GetRoutingID()));
}

void RenderViewImpl::OnMoveOrResizeStarted() {
  if (webview())
    webview()->HidePopups();
}

void RenderViewImpl::OnPageWasHidden() {
#if defined(OS_ANDROID)
  SuspendVideoCaptureDevices(true);
#endif

  if (webview()) {
    // TODO(lfg): It's not correct to defer the page visibility to the main
    // frame. Currently, this is done because the main frame may override the
    // visibility of the page when prerendering. In order to fix this,
    // prerendering must be made aware of OOPIFs. https://crbug.com/440544
    blink::mojom::PageVisibilityState visibilityState =
        GetMainRenderFrame() ? GetMainRenderFrame()->VisibilityState()
                             : blink::mojom::PageVisibilityState::kHidden;
    webview()->SetVisibilityState(visibilityState, false);
  }
}

void RenderViewImpl::OnPageWasShown() {
#if defined(OS_ANDROID)
  SuspendVideoCaptureDevices(false);
#endif

  if (webview()) {
    blink::mojom::PageVisibilityState visibilityState =
        GetMainRenderFrame() ? GetMainRenderFrame()->VisibilityState()
                             : blink::mojom::PageVisibilityState::kVisible;
    webview()->SetVisibilityState(visibilityState, false);
  }
}

void RenderViewImpl::OnUpdateScreenInfo(const ScreenInfo& screen_info) {
  // This IPC only updates the screen info on RenderViews that have a remote
  // main frame. For local main frames, the ScreenInfo is updated in
  // ViewMsg_Resize.
  // TODO(danakj): Move this message to RenderWidget?
  if (!main_render_frame_)
    GetWidget()->set_screen_info(screen_info);
}

void RenderViewImpl::SetPageFrozen(bool frozen) {
  if (webview())
    webview()->SetPageFrozen(frozen);
}

void RenderViewImpl::SetFocus(bool enable) {
  // This is not an IPC message, don't go through the IPC handler. This is used
  // in cases where the IPC message should not happen.
  GetWidget()->SetFocus(enable);
}

void RenderViewImpl::ZoomLimitsChanged(double minimum_level,
                                       double maximum_level) {
  // Round the double to avoid returning incorrect minimum/maximum zoom
  // percentages.
  int minimum_percent = round(
      ZoomLevelToZoomFactor(minimum_level) * 100);
  int maximum_percent = round(
      ZoomLevelToZoomFactor(maximum_level) * 100);

  Send(new ViewHostMsg_UpdateZoomLimits(GetRoutingID(), minimum_percent,
                                        maximum_percent));
}

void RenderViewImpl::PageScaleFactorChanged() {
  if (!webview())
    return;

  Send(new ViewHostMsg_PageScaleFactorChanged(GetRoutingID(),
                                              webview()->PageScaleFactor()));
}

double RenderViewImpl::zoomLevelToZoomFactor(double zoom_level) const {
  return ZoomLevelToZoomFactor(zoom_level);
}

double RenderViewImpl::zoomFactorToZoomLevel(double factor) const {
  return ZoomFactorToZoomLevel(factor);
}

void RenderViewImpl::PageImportanceSignalsChanged() {
  if (!webview() || !main_render_frame_)
    return;

  auto* web_signals = webview()->PageImportanceSignals();

  PageImportanceSignals signals;
  signals.had_form_interaction = web_signals->HadFormInteraction();

  main_render_frame_->Send(new FrameHostMsg_UpdatePageImportanceSignals(
      main_render_frame_->GetRoutingID(), signals));
}

void RenderViewImpl::DidAutoResize(const blink::WebSize& newSize) {
  GetWidget()->DidAutoResize(newSize);
}

blink::WebRect RenderViewImpl::RootWindowRect() {
  return WidgetClient()->WindowRect();
}

void RenderViewImpl::DidFocus(blink::WebLocalFrame* calling_frame) {
  // TODO(jcivelli): when https://bugs.webkit.org/show_bug.cgi?id=33389 is fixed
  //                 we won't have to test for user gesture anymore and we can
  //                 move that code back to render_widget.cc
  if (WebUserGestureIndicator::IsProcessingUserGesture(calling_frame) &&
      !RenderThreadImpl::current()->layout_test_mode()) {
    Send(new ViewHostMsg_Focus(GetRoutingID()));

    // Tattle on the frame that called |window.focus()|.
    RenderFrameImpl* calling_render_frame =
        RenderFrameImpl::FromWebFrame(calling_frame);
    if (calling_render_frame)
      calling_render_frame->FrameDidCallFocus();
  }
}

blink::WebScreenInfo RenderViewImpl::GetScreenInfo() {
  const ScreenInfo& info = GetWidget()->screen_info();

  blink::WebScreenInfo web_screen_info;
  web_screen_info.device_scale_factor = info.device_scale_factor;
  web_screen_info.color_space = info.color_space;
  web_screen_info.depth = info.depth;
  web_screen_info.depth_per_component = info.depth_per_component;
  web_screen_info.is_monochrome = info.is_monochrome;
  web_screen_info.rect = blink::WebRect(info.rect);
  web_screen_info.available_rect = blink::WebRect(info.available_rect);
  switch (info.orientation_type) {
    case SCREEN_ORIENTATION_VALUES_PORTRAIT_PRIMARY:
      web_screen_info.orientation_type =
          blink::kWebScreenOrientationPortraitPrimary;
      break;
    case SCREEN_ORIENTATION_VALUES_PORTRAIT_SECONDARY:
      web_screen_info.orientation_type =
          blink::kWebScreenOrientationPortraitSecondary;
      break;
    case SCREEN_ORIENTATION_VALUES_LANDSCAPE_PRIMARY:
      web_screen_info.orientation_type =
          blink::kWebScreenOrientationLandscapePrimary;
      break;
    case SCREEN_ORIENTATION_VALUES_LANDSCAPE_SECONDARY:
      web_screen_info.orientation_type =
          blink::kWebScreenOrientationLandscapeSecondary;
      break;
    default:
      web_screen_info.orientation_type = blink::kWebScreenOrientationUndefined;
      break;
  }
  web_screen_info.orientation_angle = info.orientation_angle;

  return web_screen_info;
}

#if defined(OS_ANDROID)
bool RenderViewImpl::OpenDateTimeChooser(
    const blink::WebDateTimeChooserParams& params,
    blink::WebDateTimeChooserCompletion* completion) {
  // JavaScript may try to open a date time chooser while one is already open.
  if (date_time_picker_client_)
    return false;
  date_time_picker_client_.reset(
      new RendererDateTimePicker(this, params, completion));
  return date_time_picker_client_->Open();
}

void RenderViewImpl::DismissDateTimeDialog() {
  DCHECK(date_time_picker_client_);
  date_time_picker_client_.reset();
}

void RenderViewImpl::SuspendVideoCaptureDevices(bool suspend) {
  if (!main_render_frame_)
    return;

  MediaStreamDeviceObserver* media_stream_device_observer =
      main_render_frame_->GetMediaStreamDeviceObserver();
  if (!media_stream_device_observer)
    return;

  MediaStreamDevices video_devices =
      media_stream_device_observer->GetNonScreenCaptureDevices();
  RenderThreadImpl::current()->video_capture_impl_manager()->SuspendDevices(
      video_devices, suspend);
}
#endif  // defined(OS_ANDROID)

unsigned RenderViewImpl::GetLocalSessionHistoryLengthForTesting() const {
  return history_list_length_;
}

void RenderViewImpl::SetFocusAndActivateForTesting(bool enable) {
  // If the main frame is remote, return immediately. Page level focus
  // should be set from the browser process, so if needed by tests it should
  // be properly supported.
  if (webview()->MainFrame()->IsWebRemoteFrame())
    return;

  if (enable == has_focus())
    return;

  if (enable) {
    SetActiveForWidget(true);
    // Fake an IPC message so go through the IPC handler.
    OnSetFocus(true);
  } else {
    // Fake an IPC message so go through the IPC handler.
    OnSetFocus(false);
    SetActiveForWidget(false);
  }
}

}  // namespace content
