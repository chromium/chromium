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
#include "base/process/kill.h"
#include "base/process/process.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "cc/base/switches.h"
#include "cc/paint/skia_paint_canvas.h"
#include "cc/trees/layer_tree_host.h"
#include "content/common/content_constants_internal.h"
#include "content/common/drag_messages.h"
#include "content/common/frame_messages.h"
#include "content/common/frame_replication_state.h"
#include "content/common/input_messages.h"
#include "content/common/page_messages.h"
#include "content/common/render_message_filter.mojom.h"
#include "content/common/view_messages.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/page_importance_signals.h"
#include "content/public/common/page_state.h"
#include "content/public/common/referrer_type_converters.h"
#include "content/public/common/three_d_api_types.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/web_preferences.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/public/renderer/document_state.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/render_view_observer.h"
#include "content/public/renderer/render_view_visitor.h"
#include "content/public/renderer/window_features_converter.h"
#include "content/renderer/browser_plugin/browser_plugin.h"
#include "content/renderer/compositor/layer_tree_view.h"
#include "content/renderer/drop_data_builder.h"
#include "content/renderer/history_serialization.h"
#include "content/renderer/ime_event_guard.h"
#include "content/renderer/internal_document_state_data.h"
#include "content/renderer/loader/request_extra_data.h"
#include "content/renderer/media/audio/audio_device_factory.h"
#include "content/renderer/render_frame_impl.h"
#include "content/renderer/render_frame_proxy.h"
#include "content/renderer/render_process.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/render_widget_fullscreen_pepper.h"
#include "content/renderer/renderer_blink_platform_impl.h"
#include "content/renderer/savable_resources.h"
#include "content/renderer/v8_value_converter_impl.h"
#include "content/renderer/web_ui_extension_data.h"
#include "media/audio/audio_output_device.h"
#include "media/base/media_switches.h"
#include "media/media_buildflags.h"
#include "media/renderers/audio_renderer_impl.h"
#include "media/video/gpu_video_accelerator_factories.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
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
#include "third_party/blink/public/common/plugin/plugin_action.h"
#include "third_party/blink/public/platform/file_path_conversion.h"
#include "third_party/blink/public/platform/modules/video_capture/web_video_capture_impl_manager.h"
#include "third_party/blink/public/platform/url_conversion.h"
#include "third_party/blink/public/platform/web_connection_type.h"
#include "third_party/blink/public/platform/web_effective_connection_type.h"
#include "third_party/blink/public/platform/web_http_body.h"
#include "third_party/blink/public/platform/web_input_event.h"
#include "third_party/blink/public/platform/web_input_event_result.h"
#include "third_party/blink/public/platform/web_network_state_notifier.h"
#include "third_party/blink/public/platform/web_point.h"
#include "third_party/blink/public/platform/web_rect.h"
#include "third_party/blink/public/platform/web_runtime_features.h"
#include "third_party/blink/public/platform/web_size.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_text_autosizer_page_info.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_url_error.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/platform/web_url_response.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/public/public_buildflags.h"
#include "third_party/blink/public/web/modules/mediastream/web_media_stream_device_observer.h"
#include "third_party/blink/public/web/web_autofill_client.h"
#include "third_party/blink/public/web/web_ax_object.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_dom_event.h"
#include "third_party/blink/public/web/web_dom_message_event.h"
#include "third_party/blink/public/web/web_element.h"
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

using blink::PluginAction;
using blink::WebAXObject;
using blink::WebConsoleMessage;
using blink::WebData;
using blink::WebDocument;
using blink::WebDragOperation;
using blink::WebElement;
using blink::WebFormControlElement;
using blink::WebFormElement;
using blink::WebFrame;
using blink::WebFrameContentDumper;
using blink::WebGestureEvent;
using blink::WebHistoryItem;
using blink::WebHitTestResult;
using blink::WebHTTPBody;
using blink::WebInputElement;
using blink::WebInputEvent;
using blink::WebLocalFrame;
using blink::WebMouseEvent;
using blink::WebNavigationPolicy;
using blink::WebNavigationType;
using blink::WebNode;
using blink::WebPoint;
using blink::WebRect;
using blink::WebRuntimeFeatures;
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

void ApplyCommandLineToSettings(WebSettings* settings) {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

  settings->SetThreadedScrollingEnabled(
      !command_line.HasSwitch(switches::kDisableThreadedScrolling));

  if (switches::IsTouchDragDropEnabled())
    settings->SetTouchDragDropEnabled(true);

  WebSettings::SelectionStrategyType selection_strategy;
  if (command_line.GetSwitchValueASCII(switches::kTouchTextSelectionStrategy) ==
      "direction")
    selection_strategy = WebSettings::SelectionStrategyType::kDirection;
  else
    selection_strategy = WebSettings::SelectionStrategyType::kCharacter;
  settings->SetSelectionStrategy(selection_strategy);

  std::string passive_listeners_default =
      command_line.GetSwitchValueASCII(switches::kPassiveListenersDefault);
  if (!passive_listeners_default.empty()) {
    WebSettings::PassiveEventListenerDefault passive_default =
        WebSettings::PassiveEventListenerDefault::kFalse;
    if (passive_listeners_default == "true")
      passive_default = WebSettings::PassiveEventListenerDefault::kTrue;
    else if (passive_listeners_default == "forcealltrue")
      passive_default = WebSettings::PassiveEventListenerDefault::kForceAllTrue;
    settings->SetPassiveEventListenerDefault(passive_default);
  }

  std::string network_quiet_timeout =
      command_line.GetSwitchValueASCII(switches::kNetworkQuietTimeout);
  if (!network_quiet_timeout.empty()) {
    double network_quiet_timeout_seconds = 0.0;
    if (base::StringToDouble(network_quiet_timeout,
                             &network_quiet_timeout_seconds))
      settings->SetNetworkQuietTimeout(network_quiet_timeout_seconds);
  }

  if (command_line.HasSwitch(switches::kBlinkSettings)) {
    std::vector<std::string> blink_settings = base::SplitString(
        command_line.GetSwitchValueASCII(switches::kBlinkSettings), ",",
        base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
    for (const std::string& setting : blink_settings) {
      size_t pos = setting.find('=');
      settings->SetFromStrings(
          blink::WebString::FromLatin1(setting.substr(0, pos)),
          blink::WebString::FromLatin1(
              pos == std::string::npos ? "" : setting.substr(pos + 1)));
    }
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
    : routing_id_(params.view_id),
      renderer_wide_named_frame_lookup_(
          params.renderer_wide_named_frame_lookup),
      compositor_deps_(compositor_deps),
      webkit_preferences_(params.web_preferences),
      session_storage_namespace_id_(params.session_storage_namespace_id) {
  DCHECK(!session_storage_namespace_id_.empty())
      << "Session storage namespace must be populated.";
  // Please put all logic in RenderViewImpl::Initialize().
}

void RenderViewImpl::Initialize(
    CompositorDependencies* compositor_deps,
    mojom::CreateViewParamsPtr params,
    RenderWidget::ShowCallback show_callback,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  DCHECK(RenderThread::IsMainThread());
  // We have either a main frame or a proxy routing id.
  DCHECK_NE(params->main_frame_routing_id != MSG_ROUTING_NONE,
            params->proxy_routing_id != MSG_ROUTING_NONE);

  RenderThread::Get()->AddRoute(routing_id_, this);

#if defined(OS_ANDROID)
  bool has_show_callback = !!show_callback;
#endif

  WebFrame* opener_frame =
      RenderFrameImpl::ResolveOpener(params->opener_frame_route_id);

  // The newly created webview_ is owned by this instance.
  webview_ = WebView::Create(this, params->hidden,
                             /*compositing_enabled=*/true,
                             opener_frame ? opener_frame->View() : nullptr);

  g_view_map.Get().insert(std::make_pair(webview(), this));
  g_routing_id_view_map.Get().insert(std::make_pair(GetRoutingID(), this));

  bool local_main_frame = params->main_frame_routing_id != MSG_ROUTING_NONE;

  // TODO(danakj): Put this in with making the RenderFrame? Does order matter?
  if (local_main_frame)
    webview()->SetDisplayMode(params->visual_properties.display_mode);

  ApplyWebPreferences(webkit_preferences_, webview());
  ApplyCommandLineToSettings(webview()->GetSettings());

  if (local_main_frame) {
    main_render_frame_ = RenderFrameImpl::CreateMainFrame(
        this, compositor_deps, opener_frame, &params, std::move(show_callback));
  } else {
    // TODO(https://crbug.com/995981): We should not need to create a
    // RenderWidget for a remote main frame.
    undead_render_widget_ = RenderWidget::CreateForFrame(
        params->main_frame_widget_routing_id, compositor_deps,
        params->visual_properties.display_mode,
        /*is_undead=*/true, params->never_visible);
    undead_render_widget_->set_delegate(this);
    // We intentionally pass in a null webwidget since it is not needed
    // for remote frames, and we don't have one or a ScreenInfo until we have
    // a local main frame.
    undead_render_widget_->InitForMainFrame(std::move(show_callback),
                                            /*web_frame_widget=*/nullptr,
                                            /*screen_info=*/nullptr);

    RenderFrameProxy::CreateFrameProxy(params->proxy_routing_id, GetRoutingID(),
                                       opener_frame, MSG_ROUTING_NONE,
                                       params->replicated_frame_state,
                                       params->devtools_main_frame_token);
  }

  // TODO(davidben): Move this state from Blink into content.
  if (params->window_was_created_with_opener)
    webview()->SetOpenedByDOM();

  OnSetRendererPrefs(*params->renderer_preferences);

  GetContentClient()->renderer()->RenderViewCreated(this);

  nav_state_sync_timer_.SetTaskRunner(task_runner);

  // We pass this state to Page, but it's only used by the main frame in the
  // page.
  if (params->inside_portal)
    webview()->SetInsidePortal(true);

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
  DCHECK(destroying_);  // Always deleted through Destroy().

  g_routing_id_view_map.Get().erase(routing_id_);
  RenderThread::Get()->RemoveRoute(routing_id_);

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
  DCHECK(RenderThread::IsMainThread());
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
  DCHECK(RenderThread::IsMainThread());
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
  DCHECK(RenderThread::IsMainThread());
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
  settings->SetDNSPrefetchingEnabled(prefs.dns_prefetching_enabled);
  blink::WebNetworkStateNotifier::SetSaveDataEnabled(prefs.data_saver_enabled);
  settings->SetLocalStorageEnabled(prefs.local_storage_enabled);
  settings->SetSyncXHRInDocumentsEnabled(prefs.sync_xhr_in_documents_enabled);
  WebRuntimeFeatures::EnableDatabase(prefs.databases_enabled);
  settings->SetOfflineWebApplicationCacheEnabled(
      prefs.application_cache_enabled);
  settings->SetShouldProtectAgainstIpcFlooding(
      !prefs.disable_ipc_flooding_protection);
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

  settings->SetHideScrollbars(prefs.hide_scrollbars);

  // Enable gpu-accelerated 2d canvas if requested on the command line.
  WebRuntimeFeatures::EnableAccelerated2dCanvas(
      prefs.accelerated_2d_canvas_enabled);

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
  settings->SetPrefersReducedMotion(prefs.prefers_reduced_motion);

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
  // Spatnav depends on KeyboardFocusableScrollers. The WebUI team has
  // disabled KFS because they need more time to update their custom elements,
  // crbug.com/907284. Meanwhile, we pre-ship KFS to spatnav users.
  if (prefs.spatial_navigation_enabled)
    WebRuntimeFeatures::EnableKeyboardFocusableScrollers(true);

  settings->SetCaretBrowsingEnabled(prefs.caret_browsing_enabled);

  settings->SetSelectionIncludesAltImageText(true);

  settings->SetV8CacheOptions(
      static_cast<WebSettings::V8CacheOptions>(prefs.v8_cache_options));

  settings->SetImageAnimationPolicy(
      static_cast<WebSettings::ImageAnimationPolicy>(prefs.animation_policy));

  settings->SetPresentationRequiresUserGesture(
      prefs.user_gesture_required_for_presentation);

  settings->SetTextTrackBackgroundColor(
      WebString::FromASCII(prefs.text_track_background_color));
  settings->SetTextTrackTextColor(
      WebString::FromASCII(prefs.text_track_text_color));
  settings->SetTextTrackTextSize(
      WebString::FromASCII(prefs.text_track_text_size));
  settings->SetTextTrackTextShadow(
      WebString::FromASCII(prefs.text_track_text_shadow));
  settings->SetTextTrackFontFamily(
      WebString::FromASCII(prefs.text_track_font_family));
  settings->SetTextTrackFontVariant(
      WebString::FromASCII(prefs.text_track_font_variant));
  settings->SetTextTrackMarginPercentage(prefs.text_track_margin_percentage);
  settings->SetTextTrackWindowColor(
      WebString::FromASCII(prefs.text_track_window_color));
  settings->SetTextTrackWindowPadding(
      WebString::FromASCII(prefs.text_track_window_padding));
  settings->SetTextTrackWindowRadius(
      WebString::FromASCII(prefs.text_track_window_radius));

  // Needs to happen before SetDefaultPageScaleLimits below since that'll
  // recalculate the final page scale limits and that depends on this setting.
  settings->SetShrinksViewportContentToFit(
      prefs.shrinks_viewport_contents_to_fit);

  // Needs to happen before SetIgnoreViewportTagScaleLimits below.
  web_view->SetDefaultPageScaleLimits(prefs.default_minimum_page_scale_factor,
                                      prefs.default_maximum_page_scale_factor);

  settings->SetFullscreenSupported(prefs.fullscreen_supported);
  settings->SetTextAutosizingEnabled(prefs.text_autosizing_enabled);
  settings->SetDoubleTapToZoomEnabled(prefs.double_tap_to_zoom_enabled);
  blink::WebNetworkStateNotifier::SetNetworkQualityWebHoldback(
      static_cast<blink::WebEffectiveConnectionType>(
          prefs.network_quality_estimator_web_holdback));

  settings->SetDontSendKeyEventsToJavascript(
      prefs.dont_send_key_events_to_javascript);
  settings->SetWebAppScope(WebString::FromASCII(prefs.web_app_scope.spec()));

#if defined(OS_ANDROID)
  settings->SetAllowCustomScrollbarInMainFrame(false);
  settings->SetAccessibilityFontScaleFactor(prefs.font_scale_factor);
  settings->SetDeviceScaleAdjustment(prefs.device_scale_adjustment);
  web_view->SetIgnoreViewportTagScaleLimits(prefs.force_enable_zoom);
  settings->SetAutoZoomFocusedNodeToLegibleScale(true);
  settings->SetDefaultVideoPosterURL(
      WebString::FromASCII(prefs.default_video_poster_url.spec()));
  settings->SetSupportDeprecatedTargetDensityDPI(
      prefs.support_deprecated_target_density_dpi);
  settings->SetUseLegacyBackgroundSizeShorthandBehavior(
      prefs.use_legacy_background_size_shorthand_behavior);
  settings->SetWideViewportQuirkEnabled(prefs.wide_viewport_quirk);
  settings->SetUseWideViewport(prefs.use_wide_viewport);
  settings->SetForceZeroLayoutHeight(prefs.force_zero_layout_height);
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

  WebRuntimeFeatures::EnableVideoFullscreenOrientationLock(
      prefs.video_fullscreen_orientation_lock_enabled);
  WebRuntimeFeatures::EnableVideoRotateToFullscreen(
      prefs.video_rotate_to_fullscreen_enabled);
  WebRuntimeFeatures::EnableVideoFullscreenDetection(
      prefs.video_fullscreen_detection_enabled);
  settings->SetEmbeddedMediaExperienceEnabled(
      prefs.embedded_media_experience_enabled);
  settings->SetImmersiveModeEnabled(prefs.immersive_mode_enabled);
  settings->SetDoNotUpdateSelectionOnMutatingSelectionRange(
      prefs.do_not_update_selection_on_mutating_selection_range);
  WebRuntimeFeatures::EnableCSSHexAlphaColor(prefs.css_hex_alpha_color_enabled);
  WebRuntimeFeatures::EnableScrollTopLeftInterop(
      prefs.scroll_top_left_interop_enabled);
  WebRuntimeFeatures::EnableSurfaceEmbeddingFeatures(
      !prefs.disable_features_depending_on_viz);
  WebRuntimeFeatures::EnableAcceleratedSmallCanvases(
      !prefs.disable_accelerated_small_canvases);
  if (prefs.reenable_web_components_v0) {
    WebRuntimeFeatures::EnableShadowDOMV0(true);
    WebRuntimeFeatures::EnableCustomElementsV0(true);
    WebRuntimeFeatures::EnableHTMLImports(true);
  }
#endif  // defined(OS_ANDROID)

  settings->SetForceDarkModeEnabled(prefs.force_dark_mode_enabled);

  switch (prefs.autoplay_policy) {
    case AutoplayPolicy::kNoUserGestureRequired:
      settings->SetAutoplayPolicy(
          WebSettings::AutoplayPolicy::kNoUserGestureRequired);
      break;
    case AutoplayPolicy::kUserGestureRequired:
      settings->SetAutoplayPolicy(
          WebSettings::AutoplayPolicy::kUserGestureRequired);
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

  settings->SetShowContextMenuOnMouseUp(prefs.context_menu_on_mouse_up);
  settings->SetAlwaysShowContextMenuOnTouch(
      prefs.always_show_context_menu_on_touch);
  settings->SetSmoothScrollForFindEnabled(prefs.smooth_scroll_for_find_enabled);

  settings->SetHideDownloadUI(prefs.hide_download_ui);

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

  settings->SetLazyLoadEnabled(prefs.lazy_load_enabled);

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

  for (const auto& fully_load_k_pair : prefs.lazy_image_first_k_fully_load) {
    switch (fully_load_k_pair.first) {
      case net::EFFECTIVE_CONNECTION_TYPE_OFFLINE:
        continue;
      case net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN:
        settings->SetLazyImageFirstKFullyLoadUnknown(fully_load_k_pair.second);
        continue;
      case net::EFFECTIVE_CONNECTION_TYPE_SLOW_2G:
        settings->SetLazyImageFirstKFullyLoadSlow2G(fully_load_k_pair.second);
        continue;
      case net::EFFECTIVE_CONNECTION_TYPE_2G:
        settings->SetLazyImageFirstKFullyLoad2G(fully_load_k_pair.second);
        continue;
      case net::EFFECTIVE_CONNECTION_TYPE_3G:
        settings->SetLazyImageFirstKFullyLoad3G(fully_load_k_pair.second);
        continue;
      case net::EFFECTIVE_CONNECTION_TYPE_4G:
        settings->SetLazyImageFirstKFullyLoad4G(fully_load_k_pair.second);
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

  WebRuntimeFeatures::EnableTranslateService(prefs.translate_service_available);
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

  if (g_create_render_view_impl) {
    render_view = g_create_render_view_impl(compositor_deps, *params);
  } else {
    render_view = new RenderViewImpl(compositor_deps, *params);
  }

  render_view->Initialize(compositor_deps, std::move(params),
                          std::move(show_callback), std::move(task_runner));
  return render_view;
}

void RenderViewImpl::Destroy() {
  destroying_ = true;

  // If there is no local main frame, then destroying the WebView will not
  // detach anything, and the RenderWidget will not be destroyed. So we have
  // to do it here.
  bool close_render_widget_here = !main_render_frame_;

  webview_->Close();
  // The webview_ is already destroyed by the time we get here, remove any
  // references to it.
  g_view_map.Get().erase(webview_);
  webview_ = nullptr;

  // We do this after WebView has closed, though it should not matter. WebView
  // only uses the RenderWidget through WebWidgetClient that it accesses through
  // a main frame. So it should not be able to see this happening when there is
  // no local main frame.
  if (close_render_widget_here) {
    RenderWidget* closing_widget = undead_render_widget_.get();
    closing_widget->CloseForFrame(std::move(undead_render_widget_));
  }

  delete this;
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

void RenderViewImpl::SetActiveForWidget(bool active) {
  if (webview())
    webview()->SetIsActive(active);
}

bool RenderViewImpl::SupportsMultipleWindowsForWidget() {
  return webkit_preferences_.supports_multiple_windows;
}

bool RenderViewImpl::ShouldAckSyntheticInputImmediately() {
  // TODO(bokan): The RequestPresentation API appears not to function in VR. As
  // a short term workaround for https://crbug.com/940063, ACK input
  // immediately rather than using RequestPresentation.
  if (webkit_preferences_.immersive_mode_enabled)
    return true;
  return false;
}

void RenderViewImpl::CancelPagePopupForWidget() {
  webview()->CancelPagePopup();
}

void RenderViewImpl::ApplyNewDisplayModeForWidget(
    blink::mojom::DisplayMode new_display_mode) {
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
    blink::WebFrameWidget* frame_widget =
        focused_frame->LocalRoot()->FrameWidget();
    frame_widget->ScrollFocusedEditableElementIntoView();
  }
}

void RenderViewImpl::DidReceiveSetFocusEventForWidget() {
  // This message must always be received when the main frame is a
  // WebLocalFrame.
  // TODO(ajwong): Can this be removed and just check |delegate_| in
  // RenderWidget instead?
  CHECK(webview()->MainFrame()->IsWebLocalFrame());
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
    const gfx::Size& widget_size,
    float top_controls_height,
    float bottom_controls_height,
    bool browser_controls_shrink_blink_size) {
  webview()->ResizeWithBrowserControls(widget_size, top_controls_height,
                                       bottom_controls_height,
                                       browser_controls_shrink_blink_size);
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

void RenderViewImpl::OnAudioStateChanged(bool is_audio_playing) {
  webview()->AudioStateChanged(is_audio_playing);
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
    IPC_MESSAGE_HANDLER(ViewMsg_ClosePage, OnClosePage)
    IPC_MESSAGE_HANDLER(ViewMsg_MoveOrResizeStarted, OnMoveOrResizeStarted)
    IPC_MESSAGE_HANDLER(ViewMsg_EnablePreferredSizeChangedMode,
                        OnEnablePreferredSizeChangedMode)
    IPC_MESSAGE_HANDLER(ViewMsg_PluginActionAt, OnPluginActionAt)
    IPC_MESSAGE_HANDLER(ViewMsg_AnimateDoubleTapZoom,
                        OnAnimateDoubleTapZoomInMainFrame)
    IPC_MESSAGE_HANDLER(ViewMsg_ZoomToFindInPageRect, OnZoomToFindInPageRect)
    IPC_MESSAGE_HANDLER(ViewMsg_SetBackgroundOpaque, OnSetBackgroundOpaque)

    // Page messages.
    IPC_MESSAGE_HANDLER(PageMsg_VisibilityChanged, OnPageVisibilityChanged)
    IPC_MESSAGE_HANDLER(PageMsg_SetHistoryOffsetAndLength,
                        OnSetHistoryOffsetAndLength)
    IPC_MESSAGE_HANDLER(PageMsg_AudioStateChanged, OnAudioStateChanged)
    IPC_MESSAGE_HANDLER(PageMsg_SetPageFrozen, SetPageFrozen)
    IPC_MESSAGE_HANDLER(PageMsg_PutPageIntoBackForwardCache,
                        PutPageIntoBackForwardCache)
    IPC_MESSAGE_HANDLER(PageMsg_RestorePageFromBackForwardCache,
                        RestorePageFromBackForwardCache)
    IPC_MESSAGE_HANDLER(PageMsg_UpdateTextAutosizerPageInfoForRemoteMainFrames,
                        OnTextAutosizerPageInfoChanged)
    IPC_MESSAGE_HANDLER(PageMsg_SetRendererPrefs, OnSetRendererPrefs)

    // Adding a new message? Add platform independent ones first, then put the
    // platform specific ones at the end.
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
    WebSandboxFlags sandbox_flags,
    const blink::FeaturePolicy::FeatureState& opener_feature_state,
    const blink::SessionStorageNamespaceId& session_storage_namespace_id) {
  RenderFrameImpl* creator_frame = RenderFrameImpl::FromWebFrame(creator);
  mojom::CreateNewWindowParamsPtr params = mojom::CreateNewWindowParams::New();

  // The user activation check is done at the browser process through
  // |frame_host->CreateNewWindow()| call below.  But the extensions case
  // handled through the following |if| is an exception.
  //
  // TODO(mustaq): Investigate if mimic_user_gesture can wrongly expose presence
  // of user activation w/o any user interaction, e.g. through
  // |WebChromeClient#onCreateWindow|. One case to deep-dive: disabling popup
  // blocker then calling window.open at onload event. crbug.com/929729
  params->mimic_user_gesture = false;
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
  params->opener_suppressed = features.noopener;
  params->disposition = NavigationPolicyToDisposition(policy);
  if (!request.IsNull()) {
    params->target_url = request.Url();
    params->referrer =
        blink::mojom::Referrer::From(GetReferrerFromRequest(creator, request));
  }
  params->features = ConvertWebWindowFeaturesToMojoWindowFeatures(features);

  // We preserve this information before sending the message since |params| is
  // moved on send.
  bool is_background_tab =
      params->disposition == WindowOpenDisposition::NEW_BACKGROUND_TAB;

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
  // used much in Blink, except to enable web testing... perhaps this should
  // be checked directly in the browser side.
  if (status == mojom::CreateNewWindowStatus::kReuse)
    return webview();

  DCHECK(reply);
  DCHECK_NE(MSG_ROUTING_NONE, reply->route_id);
  DCHECK_NE(MSG_ROUTING_NONE, reply->main_frame_route_id);
  DCHECK_NE(MSG_ROUTING_NONE, reply->main_frame_widget_route_id);

  // The browser allowed creation of a new window and consumed the user
  // activation.
  bool was_consumed = WebUserGestureIndicator::ConsumeUserGesture(
      creator, blink::UserActivationUpdateSource::kBrowser);

  // While this view may be a background extension page, it can spawn a visible
  // render view. So we just assume that the new one is not another background
  // page instead of passing on our own value.
  // TODO(vangelis): Can we tell if the new view will be a background page?
  bool never_visible = false;

  // The initial hidden state for the RenderViewImpl here has to match what the
  // browser will eventually decide for the given disposition. Since we have to
  // return from this call synchronously, we just have to make our best guess
  // and rely on the browser sending a WasHidden / WasShown message if it
  // disagrees.
  mojom::CreateViewParamsPtr view_params = mojom::CreateViewParams::New();

  view_params->opener_frame_route_id = creator_frame->GetRoutingID();
  DCHECK_EQ(GetRoutingID(), creator_frame->render_view()->GetRoutingID());

  view_params->window_was_created_with_opener = true;
  view_params->renderer_preferences = renderer_preferences_.Clone();
  view_params->web_preferences = webkit_preferences_;
  view_params->view_id = reply->route_id;
  view_params->main_frame_routing_id = reply->main_frame_route_id;
  view_params
      ->main_frame_interface_bundle = mojom::DocumentScopedInterfaceBundle::New(
      std::move(reply->main_frame_interface_bundle->interface_provider),
      std::move(reply->main_frame_interface_bundle->browser_interface_broker));
  view_params->main_frame_widget_routing_id = reply->main_frame_widget_route_id;
  view_params->session_storage_namespace_id =
      reply->cloned_session_storage_namespace_id;
  DCHECK(!view_params->session_storage_namespace_id.empty())
      << "Session storage namespace must be populated.";
  view_params->replicated_frame_state.frame_policy.sandbox_flags =
      sandbox_flags;
  view_params->replicated_frame_state.opener_feature_state =
      opener_feature_state;
  view_params->replicated_frame_state.name = frame_name_utf8;
  view_params->devtools_main_frame_token = reply->devtools_main_frame_token;
  view_params->hidden = is_background_tab;
  view_params->never_visible = never_visible;
  view_params->visual_properties = reply->visual_properties;

  // Unretained() is safe here because our calling function will also call
  // show().
  RenderWidget::ShowCallback show_callback =
      base::BindOnce(&RenderFrameImpl::ShowCreatedWindow,
                     base::Unretained(creator_frame), was_consumed);

  RenderViewImpl* view = RenderViewImpl::Create(
      compositor_deps_, std::move(view_params), std::move(show_callback),
      creator->GetTaskRunner(blink::TaskType::kInternalDefault));

  return view->webview();
}

blink::WebPagePopup* RenderViewImpl::CreatePopup(
    blink::WebLocalFrame* creator) {
  mojo::PendingRemote<mojom::Widget> widget_channel;
  mojo::PendingReceiver<mojom::Widget> widget_channel_receiver =
      widget_channel.InitWithNewPipeAndPassReceiver();

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

  RenderWidget* opener_render_widget =
      RenderFrameImpl::FromWebFrame(creator)->GetLocalRootRenderWidget();

  RenderWidget* popup_widget = RenderWidget::CreateForPopup(
      widget_routing_id, opener_render_widget->compositor_deps(),
      blink::mojom::DisplayMode::kUndefined,
      /*hidden=*/false,
      /*never_visible=*/false, std::move(widget_channel_receiver));

  // The returned WebPagePopup is self-referencing, so the pointer here is not
  // an owning pointer. It is de-referenced by calling Close().
  blink::WebPagePopup* popup_web_widget =
      blink::WebPagePopup::Create(popup_widget);

  // Adds a self-reference on the |popup_widget| so it will not be destroyed
  // when leaving scope. The WebPagePopup takes responsibility for Close()ing
  // and thus destroying the RenderWidget.
  popup_widget->InitForPopup(std::move(opener_callback), opener_render_widget,
                             popup_web_widget,
                             opener_render_widget->GetOriginalScreenInfo());
  // TODO(crbug.com/419087): RenderWidget has some weird logic for picking a
  // WebWidget which doesn't apply to this case. So we verify. This can go away
  // when RenderWidget::GetWebWidget() is just a simple accessor.
  DCHECK_EQ(popup_widget->GetWebWidget(), popup_web_widget);

  return popup_web_widget;
}

void RenderViewImpl::DoDeferredClose() {
  // The main widget is currently not active. The active main frame widget is
  // in a different process.  Have the browser route the close request to the
  // active widget instead, so that the correct unload handlers are run.
  Send(new ViewHostMsg_RouteCloseEvent(GetRoutingID()));
}

void RenderViewImpl::CloseWindowSoon() {
  DCHECK(RenderThread::IsMainThread());
  if (!render_widget_ || render_widget_->IsUndeadOrProvisional()) {
    // Ask the RenderViewHost with a local main frame to initiate close.  We
    // could be called from deep in Javascript.  If we ask the RenderViewHost to
    // close now, the window could be closed before the JS finishes executing,
    // thanks to nested message loops running and handling the resulting Close
    // IPC. So instead, post a message back to the message loop, which won't run
    // until the JS is complete, and then the Close request can be sent.
    GetCleanupTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(&RenderViewImpl::DoDeferredClose,
                                  weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  // If the main widget is not undead then the Close request goes directly
  // through it, because the RenderWidget ultimately owns the RenderViewImpl.
  render_widget_->CloseWidgetSoon();
}

base::StringPiece RenderViewImpl::GetSessionStorageNamespaceId() {
  CHECK(!session_storage_namespace_id_.empty());
  return session_storage_namespace_id_;
}

void RenderViewImpl::PrintPage(WebLocalFrame* frame) {
  RenderFrameImpl* render_frame = RenderFrameImpl::FromWebFrame(frame);
  RenderWidget* render_widget = render_frame->GetLocalRootRenderWidget();

  render_frame->ScriptedPrint(
      render_widget->input_handler().handling_input_event());
}

bool RenderViewImpl::SetZoomLevel(double zoom_level) {
  if (zoom_level == page_zoom_level_)
    return false;

  // If we change the zoom level for the view, make sure any subsequent subframe
  // loads reflect the current zoom level.
  page_zoom_level_ = zoom_level;
  webview()->SetZoomLevel(zoom_level);
  for (auto& observer : observers_)
    observer.OnZoomLevelChanged();
  return true;
}

void RenderViewImpl::SetPreferCompositingToLCDTextEnabled(bool prefer) {
  webview()->GetSettings()->SetPreferCompositingToLCDTextEnabled(prefer);
}

void RenderViewImpl::SetDeviceScaleFactor(bool use_zoom_for_dsf,
                                          float device_scale_factor) {
  if (use_zoom_for_dsf)
    webview()->SetZoomFactorForDeviceScaleFactor(device_scale_factor);
  else
    webview()->SetDeviceScaleFactor(device_scale_factor);
}

void RenderViewImpl::SetVisibleViewportSize(
    const gfx::Size& visible_viewport_size) {
  if (main_render_frame_) {
    // A local main frame controls the size of the WebView through
    // ResizeWebWidgetForWidget(). The VisualViewport is updated independently
    // here.
    webview()->ResizeVisualViewport(visible_viewport_size);
  } else {
    // RenderWidgets in a RenderView's frame tree without a local main frame
    // set the size of the WebView to be the |visible_viewport_size|, in order
    // to limit compositing in (out of process) child frames to what is visible.
    //
    // Note that child frames in the same process/RenderView frame tree as the
    // main frame do not benefit from this.
    webview()->Resize(visible_viewport_size);
  }
}

void RenderViewImpl::PropagatePageZoomToNewlyAttachedFrame(
    bool use_zoom_for_dsf,
    float device_scale_factor) {
  if (use_zoom_for_dsf)
    webview()->SetZoomFactorForDeviceScaleFactor(device_scale_factor);
  else
    webview()->SetZoomLevel(page_zoom_level_);
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
  else if (GetWebView()->GetVisibilityState() != PageVisibilityState::kVisible)
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
  nav_state_sync_timer_.Start(FROM_HERE, base::TimeDelta::FromSeconds(delay),
                              this, &RenderViewImpl::SendFrameStateUpdates);
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

void RenderViewImpl::FocusedElementChanged(const WebElement& from_element,
                                           const WebElement& to_element) {
  RenderFrameImpl* previous_frame = nullptr;
  if (!from_element.IsNull())
    previous_frame =
        RenderFrameImpl::FromWebFrame(from_element.GetDocument().GetFrame());
  RenderFrameImpl* new_frame = nullptr;
  if (!to_element.IsNull())
    new_frame =
        RenderFrameImpl::FromWebFrame(to_element.GetDocument().GetFrame());

  if (previous_frame && previous_frame != new_frame)
    previous_frame->FocusedElementChanged(WebElement());
  if (new_frame)
    new_frame->FocusedElementChanged(to_element);

  // TODO(dmazzoni): remove once there's a separate a11y tree per frame.
  if (main_render_frame_)
    main_render_frame_->FocusedElementChangedForAccessibility(to_element);
}

void RenderViewImpl::DidUpdateMainFrameLayout() {
  for (auto& observer : observers_)
    observer.DidUpdateMainFrameLayout();

  // The main frame may have changed size.
  needs_preferred_size_update_ = true;
}

void RenderViewImpl::UpdateBrowserControlsState(
    BrowserControlsState constraints,
    BrowserControlsState current,
    bool animate) {
  TRACE_EVENT2("renderer", "RenderViewImpl::UpdateBrowserControlsState",
               "Constraint", static_cast<int>(constraints), "Current",
               static_cast<int>(current));
  TRACE_EVENT_INSTANT1("renderer", "is_animated", TRACE_EVENT_SCOPE_THREAD,
                       "animated", animate);

  if (render_widget_ && render_widget_->layer_tree_view()) {
    render_widget_->layer_tree_view()
        ->layer_tree_host()
        ->UpdateBrowserControlsState(ContentToCc(constraints),
                                     ContentToCc(current), animate);
  }

  top_controls_constraints_ = constraints;
}

void RenderViewImpl::RegisterRendererPreferenceWatcher(
    mojo::PendingRemote<blink::mojom::RendererPreferenceWatcher> watcher) {
  renderer_preference_watchers_.Add(std::move(watcher));
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

// TODO(https://crbug.com/937569): Remove this in Chrome 82.
bool RenderViewImpl::AllowPopupsDuringPageUnload() {
  // The switch version is for enabling via enterprise policy. The feature
  // version is for enabling via about:flags and Finch policy.
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  return command_line.HasSwitch(switches::kAllowPopupsDuringPageUnload) ||
         base::FeatureList::IsEnabled(features::kAllowPopupsDuringPageUnload);
}

bool RenderViewImpl::CanUpdateLayout() {
  return true;
}

void RenderViewImpl::SetEditCommandForNextKeyEvent(const std::string& name,
                                                   const std::string& value) {
  // This is test-only code. Only propagate the command if there is a main
  // render frame.
  if (main_render_frame_)
    render_widget_->SetEditCommandForNextKeyEvent(name, value);
}

void RenderViewImpl::ClearEditCommands() {
  // This is test-only code. Only propagate the command if there is a main
  // render frame.
  if (main_render_frame_)
    render_widget_->ClearEditCommands();
}

const std::string& RenderViewImpl::GetAcceptLanguages() {
  return renderer_preferences_.accept_languages;
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

void RenderViewImpl::UpdatePreferredSize() {
  // We don't always want to send the change messages over IPC, only if we've
  // been put in that mode by getting a |ViewMsg_EnablePreferredSizeChangedMode|
  // message.
  if (!send_preferred_size_changes_ || !webview() || !main_render_frame_)
    return;

  if (!needs_preferred_size_update_)
    return;
  needs_preferred_size_update_ = false;

  blink::WebSize web_size = webview()->ContentsPreferredMinimumSize();
  blink::WebRect web_rect(0, 0, web_size.width, web_size.height);
  render_widget_->ConvertViewportToWindow(&web_rect);
  gfx::Size size(web_rect.width, web_rect.height);

  if (size != preferred_size_) {
    preferred_size_ = size;
    Send(new ViewHostMsg_DidContentsPreferredSizeChange(GetRoutingID(), size));
  }
}

blink::WebString RenderViewImpl::AcceptLanguages() {
  return WebString::FromUTF8(renderer_preferences_.accept_languages);
}

// RenderView implementation ---------------------------------------------------

bool RenderViewImpl::Send(IPC::Message* message) {
  // No messages sent through RenderView come without a routing id, yay. Let's
  // keep that up.
  CHECK_NE(message->routing_id(), MSG_ROUTING_NONE);
  return RenderThread::Get()->Send(message);
}

RenderWidget* RenderViewImpl::GetWidget() {
  return render_widget_.get();
}

RenderFrameImpl* RenderViewImpl::GetMainRenderFrame() {
  return main_render_frame_;
}

int RenderViewImpl::GetRoutingID() {
  return routing_id_;
}

float RenderViewImpl::GetZoomLevel() {
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

bool RenderViewImpl::GetContentStateImmediately() {
  return send_content_state_immediately_;
}

void RenderViewImpl::OnSetPageScale(float page_scale_factor) {
  if (!webview())
    return;
  webview()->SetPageScaleFactor(page_scale_factor);
}

void RenderViewImpl::ApplyPageVisibilityState(
    PageVisibilityState visibility_state,
    bool initial_setting) {
  webview()->SetVisibilityState(visibility_state, initial_setting);
  for (auto& observer : observers_)
    observer.OnPageVisibilityChanged(visibility_state);
  // Note: RenderWidget visibility is separately set from the IPC handlers, and
  // does not change when tests override the visibility of the Page.
}

RenderWidget* RenderViewImpl::ReviveUndeadMainFrameRenderWidget() {
  render_widget_ = std::move(undead_render_widget_);
  render_widget_->SetIsUndead(false);
  return render_widget_.get();
}

void RenderViewImpl::CloseMainFrameRenderWidget() {
  // There is a WebFrameWidget previously attached by AttachWebFrameWidget().
  DCHECK(render_widget_->GetWebWidget());

  if (destroying_) {
    // We are inside RenderViewImpl::Destroy() and the main frame is being
    // detached as part of shutdown. So we can destroy the RenderWidget.

    // We pass ownership of |render_widget_| to itself. Grab a raw pointer to
    // call the Close() method on so we don't have to be a C++ expert to know
    // whether we will end up with a nullptr where we didn't intend due to order
    // of execution.
    RenderWidget* closing_widget = render_widget_.get();
    closing_widget->CloseForFrame(std::move(render_widget_));
  } else {
    // We are not inside RenderViewImpl::Destroy(), the main frame is being
    // detached and replaced with a remote frame proxy. We can't close the
    // RenderWidget, and it is marked undead instead.
    render_widget_->SetIsUndead(true);

    undead_render_widget_ = std::move(render_widget_);
  }
}

void RenderViewImpl::OnUpdateWebPreferences(const WebPreferences& prefs) {
  webkit_preferences_ = prefs;
  ApplyWebPreferences(webkit_preferences_, webview());
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
  if (webview()->MainFrameWidget()) {
    webview()->MainFrameWidget()->UpdateLifecycle(
        WebWidget::LifecycleUpdate::kLayout,
        WebWidget::LifecycleUpdateReason::kOther);
  }

  // If a layout was not needed, |DidUpdateMainFrameLayout| will not be called.
  // We explicitly update the preferred size here to ensure the preferred size
  // notification is sent.
  UpdatePreferredSize();
}

void RenderViewImpl::OnSetRendererPrefs(
    const blink::mojom::RendererPreferences& renderer_prefs) {
  std::string old_accept_languages = renderer_preferences_.accept_languages;

  renderer_preferences_ = renderer_prefs;

  for (auto& watcher : renderer_preference_watchers_)
    watcher->NotifyUpdate(renderer_prefs.Clone());

  UpdateFontRenderingFromRendererPrefs();
  UpdateThemePrefs();
  blink::SetCaretBlinkInterval(
      renderer_prefs.caret_blink_interval.has_value()
          ? renderer_prefs.caret_blink_interval.value()
          : base::TimeDelta::FromMilliseconds(
                blink::mojom::kDefaultCaretBlinkIntervalInMilliseconds));

#if defined(USE_AURA)
  if (renderer_prefs.use_custom_colors) {
    blink::SetFocusRingColor(renderer_prefs.focus_ring_color);
    blink::SetSelectionColors(renderer_prefs.active_selection_bg_color,
                              renderer_prefs.active_selection_fg_color,
                              renderer_prefs.inactive_selection_bg_color,
                              renderer_prefs.inactive_selection_fg_color);
    if (webview() && webview()->MainFrameWidget())
      webview()->MainFrameWidget()->ThemeChanged();
  }
#endif

  if (webview() &&
      old_accept_languages != renderer_preferences_.accept_languages) {
    webview()->AcceptLanguagesChanged();
  }
}

void RenderViewImpl::OnPluginActionAt(const gfx::Point& location,
                                      const PluginAction& action) {
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
    webview()->CancelPagePopup();
}

void RenderViewImpl::OnPageVisibilityChanged(
    PageVisibilityState visibility_state) {
#if defined(OS_ANDROID)
  SuspendVideoCaptureDevices(visibility_state != PageVisibilityState::kVisible);
#endif

  ApplyPageVisibilityState(visibility_state,
                           /*initial_setting=*/false);
}

void RenderViewImpl::SetPageFrozen(bool frozen) {
  if (webview())
    webview()->SetPageFrozen(frozen);
}

void RenderViewImpl::PutPageIntoBackForwardCache() {
  if (webview())
    webview()->PutPageIntoBackForwardCache();
}

void RenderViewImpl::RestorePageFromBackForwardCache(
    base::TimeTicks navigation_start) {
  if (webview())
    webview()->RestorePageFromBackForwardCache(navigation_start);
}

// This function receives TextAutosizerPageInfo from the main frame's renderer
// and makes it available to other renderers with frames on the same page.
void RenderViewImpl::OnTextAutosizerPageInfoChanged(
    const blink::WebTextAutosizerPageInfo& page_info) {
  // Only propagate the remote page info if our main frame is remote. It's
  // possible a main frame renderer may receive this message, as SendPageMessage
  // in RenderFrameHostManager may send to a speculative RenderFrameHost that
  // corresponds to a local main frame. Since a local main frame will generate
  // these values for itself, we shouldn't override them with values from
  // another renderer.
  if (!webview()->MainFrame()->IsWebLocalFrame())
    webview()->SetTextAutosizePageInfo(page_info);
}

void RenderViewImpl::SetFocus(bool enable) {
  // This is only called from RenderFrameProxy.
  CHECK(!webview()->MainFrame()->IsWebLocalFrame());
  webview()->SetFocus(enable);
}

void RenderViewImpl::PageScaleFactorChanged(float page_scale_factor) {
  if (!webview())
    return;

  Send(new ViewHostMsg_PageScaleFactorChanged(GetRoutingID(),
                                              page_scale_factor));
}

void RenderViewImpl::DidUpdateTextAutosizerPageInfo(
    const blink::WebTextAutosizerPageInfo& page_info) {
  DCHECK(webview()->MainFrame()->IsWebLocalFrame());
  Send(new ViewHostMsg_NotifyTextAutosizerPageInfoChangedInLocalMainFrame(
      GetRoutingID(), page_info));
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
  // Auto resize should only happen on local main frames.
  DCHECK(render_widget_);
  render_widget_->DidAutoResize(newSize);
}

void RenderViewImpl::DidFocus(blink::WebLocalFrame* calling_frame) {
  // TODO(jcivelli): when https://bugs.webkit.org/show_bug.cgi?id=33389 is fixed
  //                 we won't have to test for user gesture anymore and we can
  //                 move that code back to render_widget.cc
  if (WebUserGestureIndicator::IsProcessingUserGesture(calling_frame) &&
      !RenderThreadImpl::current()->web_test_mode()) {
    Send(new ViewHostMsg_Focus(GetRoutingID()));

    // Tattle on the frame that called |window.focus()|.
    RenderFrameImpl* calling_render_frame =
        RenderFrameImpl::FromWebFrame(calling_frame);
    if (calling_render_frame)
      calling_render_frame->FrameDidCallFocus();
  }
}

#if defined(OS_ANDROID)
void RenderViewImpl::SuspendVideoCaptureDevices(bool suspend) {
  if (!main_render_frame_)
    return;

  blink::WebMediaStreamDeviceObserver* media_stream_device_observer =
      main_render_frame_->MediaStreamDeviceObserver();
  if (!media_stream_device_observer)
    return;

  blink::MediaStreamDevices video_devices =
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

  if (enable == render_widget_->has_focus())
    return;

  if (enable) {
    SetActiveForWidget(true);
    // Fake an IPC message so go through the IPC handler.
    render_widget_->OnSetFocus(true);
  } else {
    // Fake an IPC message so go through the IPC handler.
    render_widget_->OnSetFocus(false);
    SetActiveForWidget(false);
  }
}

void RenderViewImpl::OnAnimateDoubleTapZoomInMainFrame(
    const blink::WebPoint& point,
    const blink::WebRect& bound) {
  webview()->AnimateDoubleTapZoom(point, bound);
}

void RenderViewImpl::OnZoomToFindInPageRect(
    const blink::WebRect& rect_to_zoom) {
  webview()->ZoomToFindInPageRect(rect_to_zoom);
}

void RenderViewImpl::OnSetBackgroundOpaque(bool opaque) {
  if (!webview())
    return;

  if (opaque) {
    webview()->ClearBaseBackgroundColorOverride();
    webview()->ClearBackgroundColorOverride();
  } else {
    webview()->SetBaseBackgroundColorOverride(SK_ColorTRANSPARENT);
    webview()->SetBackgroundColorOverride(SK_ColorTRANSPARENT);
  }
}

// static
scoped_refptr<base::SingleThreadTaskRunner>
RenderViewImpl::GetCleanupTaskRunner() {
  return RenderThreadImpl::current_blink_platform_impl()
      ->main_thread_scheduler()
      ->CleanupTaskRunner();
}

}  // namespace content
