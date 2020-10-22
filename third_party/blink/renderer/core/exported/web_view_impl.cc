/*
 * Copyright (C) 2011, 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "third_party/blink/renderer/core/exported/web_view_impl.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "cc/layers/picture_layer.h"
#include "media/base/media_switches.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/input/web_menu_source_type.h"
#include "third_party/blink/public/common/page/page_zoom.h"
#include "third_party/blink/public/common/switches.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "third_party/blink/public/mojom/input/focus_type.mojom-blink.h"
#include "third_party/blink/public/platform/interface_registry.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/scheduler/web_thread_scheduler.h"
#include "third_party/blink/public/platform/web_media_player.h"
#include "third_party/blink/public/platform/web_network_state_notifier.h"
#include "third_party/blink/public/platform/web_runtime_features.h"
#include "third_party/blink/public/platform/web_text_input_info.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/public/web/web_autofill_client.h"
#include "third_party/blink/public/web/web_console_message.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/public/web/web_hit_test_result.h"
#include "third_party/blink/public/web/web_input_element.h"
#include "third_party/blink/public/web/web_local_frame_client.h"
#include "third_party/blink/public/web/web_meaningful_layout.h"
#include "third_party/blink/public/web/web_node.h"
#include "third_party/blink/public/web/web_plugin.h"
#include "third_party/blink/public/web/web_range.h"
#include "third_party/blink/public/web/web_view_client.h"
#include "third_party/blink/public/web/web_widget_client.h"
#include "third_party/blink/public/web/web_window_features.h"
#include "third_party/blink/renderer/core/clipboard/data_object.h"
#include "third_party/blink/renderer/core/content_capture/content_capture_manager.h"
#include "third_party/blink/renderer/core/core_initializer.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/core/dom/context_features_client_impl.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"
#include "third_party/blink/renderer/core/dom/layout_tree_builder_traversal.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/editor.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/ime/input_method_controller.h"
#include "third_party/blink/renderer/core/editing/iterators/text_iterator.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/serializers/html_interchange.h"
#include "third_party/blink/renderer/core/editing/serializers/serialization.h"
#include "third_party/blink/renderer/core/events/current_input_event.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/events/ui_event_with_key_state.h"
#include "third_party/blink/renderer/core/events/web_input_event_conversion.h"
#include "third_party/blink/renderer/core/events/wheel_event.h"
#include "third_party/blink/renderer/core/exported/web_dev_tools_agent_impl.h"
#include "third_party/blink/renderer/core/exported/web_plugin_container_impl.h"
#include "third_party/blink/renderer/core/exported/web_remote_frame_impl.h"
#include "third_party/blink/renderer/core/exported/web_settings_impl.h"
#include "third_party/blink/renderer/core/frame/browser_controls.h"
#include "third_party/blink/renderer/core/frame/event_handler_registry.h"
#include "third_party/blink/renderer/core/frame/fullscreen_controller.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/local_frame_ukm_aggregator.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/page_scale_constraints_set.h"
#include "third_party/blink/renderer/core/frame/remote_frame.h"
#include "third_party/blink/renderer/core/frame/resize_viewport_anchor.h"
#include "third_party/blink/renderer/core/frame/rotation_viewport_anchor.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/viewport_data.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/frame/web_frame_widget_base.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/frame/web_view_frame_widget.h"
#include "third_party/blink/renderer/core/fullscreen/fullscreen.h"
#include "third_party/blink/renderer/core/html/forms/html_text_area_element.h"
#include "third_party/blink/renderer/core/html/html_plugin_element.h"
#include "third_party/blink/renderer/core/html/plugin_document.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/input/context_menu_allowed_scope.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/input/touch_action_util.h"
#include "third_party/blink/renderer/core/inspector/dev_tools_emulator.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_content.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/text_autosizer.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/frame_load_request.h"
#include "third_party/blink/renderer/core/loader/frame_loader.h"
#include "third_party/blink/renderer/core/loader/interactive_detector.h"
#include "third_party/blink/renderer/core/loader/prerenderer_client.h"
#include "third_party/blink/renderer/core/page/chrome_client_impl.h"
#include "third_party/blink/renderer/core/page/context_menu_controller.h"
#include "third_party/blink/renderer/core/page/context_menu_provider.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/page/frame_tree.h"
#include "third_party/blink/renderer/core/page/link_highlight.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/page_popup_client.h"
#include "third_party/blink/renderer/core/page/pointer_lock_controller.h"
#include "third_party/blink/renderer/core/page/scrolling/fragment_anchor.h"
#include "third_party/blink/renderer/core/page/scrolling/scrolling_coordinator.h"
#include "third_party/blink/renderer/core/page/scrolling/top_document_root_scroller_controller.h"
#include "third_party/blink/renderer/core/paint/compositing/paint_layer_compositor.h"
#include "third_party/blink/renderer/core/paint/first_meaningful_paint_detector.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/paint/paint_timing.h"
#include "third_party/blink/renderer/core/paint/paint_timing_detector.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/scroll/scrollbar_theme.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/core/timing/window_performance.h"
#include "third_party/blink/renderer/platform/fonts/font_cache.h"
#include "third_party/blink/renderer/platform/graphics/image.h"
#include "third_party/blink/renderer/platform/graphics/paint/cull_rect.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_record_builder.h"
#include "third_party/blink/renderer/platform/image-decoders/image_decoder.h"
#include "third_party/blink/renderer/platform/instrumentation/histogram.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/keyboard_codes.h"
#include "third_party/blink/renderer/platform/loader/fetch/unique_identifier.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/scheduler/public/page_lifecycle_state.h"
#include "third_party/blink/renderer/platform/scheduler/public/page_scheduler.h"
#include "third_party/blink/renderer/platform/widget/widget_base.h"
#include "third_party/icu/source/common/unicode/uscript.h"

#include "ui/gfx/skia_util.h"

#if defined(OS_ANDROID)
#include "components/viz/common/features.h"
#endif

// Get rid of WTF's pow define so we can use std::pow.
#undef pow
#include <cmath>  // for std::pow

// The following constants control parameters for automated scaling of webpages
// (such as due to a double tap gesture or find in page etc.). These are
// experimentally determined.
static const int touchPointPadding = 32;
static const int nonUserInitiatedPointPadding = 11;
static const float minScaleDifference = 0.01f;
static const float doubleTapZoomContentDefaultMargin = 5;
static const float doubleTapZoomContentMinimumMargin = 2;
static constexpr base::TimeDelta kDoubleTapZoomAnimationDuration =
    base::TimeDelta::FromMilliseconds(250);
static const float doubleTapZoomAlreadyLegibleRatio = 1.2f;

static constexpr base::TimeDelta kFindInPageAnimationDuration;

// Constants for viewport anchoring on resize.
static const float viewportAnchorCoordX = 0.5f;
static const float viewportAnchorCoordY = 0;

// Constants for zooming in on a focused text field.
static constexpr base::TimeDelta kScrollAndScaleAnimationDuration =
    base::TimeDelta::FromMicroseconds(200);
static const int minReadableCaretHeight = 16;
static const int minReadableCaretHeightForTextArea = 13;
static const float minScaleChangeToTriggerZoom = 1.5f;
static const float leftBoxRatio = 0.3f;
static const int caretPadding = 10;

namespace blink {

// Historically, these values came from Webkit in
// WebKitLegacy/mac/WebView/WebView.mm (named MinimumZoomMultiplier and
// MaximumZoomMultiplier there).
const double WebView::kMinTextSizeMultiplier = 0.5;
const double WebView::kMaxTextSizeMultiplier = 3.0;

// static
HashSet<WebViewImpl*>& WebViewImpl::AllInstances() {
  DEFINE_STATIC_LOCAL(HashSet<WebViewImpl*>, all_instances, ());
  return all_instances;
}

static bool g_should_use_external_popup_menus = false;

void WebView::SetUseExternalPopupMenus(bool use_external_popup_menus) {
  g_should_use_external_popup_menus = use_external_popup_menus;
}

bool WebViewImpl::UseExternalPopupMenus() {
  return g_should_use_external_popup_menus;
}

namespace {

class EmptyEventListener final : public NativeEventListener {
 public:
  void Invoke(ExecutionContext* execution_context, Event*) override {}
};

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

void ApplyFontsFromMap(const web_pref::ScriptFontFamilyMap& map,
                       SetFontFamilyWrapper setter,
                       WebSettings* settings) {
  for (auto& it : map) {
    int32_t script = u_getPropertyValueEnum(UCHAR_SCRIPT, (it.first).c_str());
    if (script >= 0 && script < USCRIPT_CODE_LIMIT) {
      UScriptCode code = static_cast<UScriptCode>(script);
      (*setter)(settings, it.second, GetScriptForWebSettings(code));
    }
  }
}

void ApplyCommandLineToSettings(WebSettings* settings) {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

  settings->SetThreadedScrollingEnabled(
      !command_line.HasSwitch(switches::kDisableThreadedScrolling));

  WebSettings::SelectionStrategyType selection_strategy;
  if (command_line.GetSwitchValueASCII(switches::kTouchTextSelectionStrategy) ==
      "direction")
    selection_strategy = WebSettings::SelectionStrategyType::kDirection;
  else
    selection_strategy = WebSettings::SelectionStrategyType::kCharacter;
  settings->SetSelectionStrategy(selection_strategy);

  WebString passive_listeners_default = WebString::FromUTF8(
      command_line.GetSwitchValueASCII(switches::kPassiveListenersDefault));
  if (!passive_listeners_default.IsEmpty()) {
    WebSettings::PassiveEventListenerDefault passive_default =
        WebSettings::PassiveEventListenerDefault::kFalse;
    if (passive_listeners_default == "true")
      passive_default = WebSettings::PassiveEventListenerDefault::kTrue;
    else if (passive_listeners_default == "forcealltrue")
      passive_default = WebSettings::PassiveEventListenerDefault::kForceAllTrue;
    settings->SetPassiveEventListenerDefault(passive_default);
  }

  WebString network_quiet_timeout = WebString::FromUTF8(
      command_line.GetSwitchValueASCII(switches::kNetworkQuietTimeout));
  if (!network_quiet_timeout.IsEmpty()) {
    bool ok;
    double network_quiet_timeout_seconds =
        String(network_quiet_timeout).ToDouble(&ok);
    if (ok)
      settings->SetNetworkQuietTimeout(network_quiet_timeout_seconds);
  }

  if (command_line.HasSwitch(switches::kBlinkSettings)) {
    Vector<String> blink_settings;
    String command_line_settings =
        command_line.GetSwitchValueASCII(switches::kBlinkSettings).c_str();
    command_line_settings.Split(",", blink_settings);
    for (const String& setting : blink_settings) {
      wtf_size_t pos = setting.find('=');
      settings->SetFromStrings(
          WebString(setting.Substring(0, pos)),
          WebString(pos == kNotFound ? "" : setting.Substring(pos + 1)));
    }
  }
}

WebMediaPlayer::SurfaceLayerMode GetVideoSurfaceLayerMode() {
#if defined(OS_ANDROID)
  if (base::FeatureList::IsEnabled(media::kDisableSurfaceLayerForVideo) &&
      !::features::IsUsingVizForWebView())
    return WebMediaPlayer::SurfaceLayerMode::kNever;
#endif  // OS_ANDROID

  return WebMediaPlayer::SurfaceLayerMode::kAlways;
}

}  // namespace

// WebView ----------------------------------------------------------------

WebView* WebView::Create(
    WebViewClient* client,
    bool is_hidden,
    bool is_inside_portal,
    bool compositing_enabled,
    WebView* opener,
    CrossVariantMojoAssociatedReceiver<mojom::PageBroadcastInterfaceBase>
        page_handle) {
  return WebViewImpl::Create(
      client,
      is_hidden ? mojom::blink::PageVisibilityState::kHidden
                : mojom::blink::PageVisibilityState::kVisible,
      is_inside_portal, compositing_enabled, static_cast<WebViewImpl*>(opener),
      std::move(page_handle));
}

WebViewImpl* WebViewImpl::Create(
    WebViewClient* client,
    mojom::blink::PageVisibilityState visibility,
    bool is_inside_portal,
    bool compositing_enabled,
    WebViewImpl* opener,
    mojo::PendingAssociatedReceiver<mojom::blink::PageBroadcast> page_handle) {
  // Take a self-reference for WebViewImpl that is released by calling Close(),
  // then return a raw pointer to the caller.
  auto web_view = base::AdoptRef(
      new WebViewImpl(client, visibility, is_inside_portal, compositing_enabled,
                      opener, std::move(page_handle)));
  web_view->AddRef();
  return web_view.get();
}

void WebView::UpdateVisitedLinkState(uint64_t link_hash) {
  Page::VisitedStateChanged(link_hash);
}

void WebView::ResetVisitedLinkState(bool invalidate_visited_link_hashes) {
  Page::AllVisitedStateChanged(invalidate_visited_link_hashes);
}

void WebViewImpl::SetPrerendererClient(
    WebPrerendererClient* prerenderer_client) {
  DCHECK(page_);
  ProvidePrerendererClientTo(*page_, MakeGarbageCollected<PrerendererClient>(
                                         *page_, prerenderer_client));
}

void WebViewImpl::CloseWindowSoon() {
  // Ask the RenderViewHost with a local main frame to initiate close.  We
  // could be called from deep in Javascript.  If we ask the RenderViewHost to
  // close now, the window could be closed before the JS finishes executing,
  // thanks to nested message loops running and handling the resulting
  // DestroyView IPC. So instead, post a message back to the message loop, which
  // won't run until the JS is complete, and then the
  // RouteCloseEvent/RequestClose request can be sent.
  Thread::MainThread()->GetTaskRunner()->PostTask(
      FROM_HERE, WTF::Bind(&WebViewImpl::DoDeferredCloseWindowSoon,
                           weak_ptr_factory_.GetWeakPtr()));
}

void WebViewImpl::DoDeferredCloseWindowSoon() {
  // Have the browser process a close request. We should have either a
  // |local_main_frame_host_remote_| or |remote_main_frame_host_remote_|.
  // This method will not execute if Close has been called as WeakPtrs
  // will be invalidated in Close.
  if (GetPage()->MainFrame()->IsLocalFrame()) {
    DCHECK(local_main_frame_host_remote_);
    local_main_frame_host_remote_->RequestClose();
  } else {
    DCHECK(remote_main_frame_host_remote_);
    remote_main_frame_host_remote_->RouteCloseEvent();
  }
}

WebViewImpl::WebViewImpl(
    WebViewClient* client,
    mojom::blink::PageVisibilityState visibility,
    bool is_inside_portal,
    bool does_composite,
    WebViewImpl* opener,
    mojo::PendingAssociatedReceiver<mojom::blink::PageBroadcast> page_handle)
    : web_view_client_(client),
      chrome_client_(MakeGarbageCollected<ChromeClientImpl>(this)),
      minimum_zoom_level_(PageZoomFactorToZoomLevel(kMinimumPageZoomFactor)),
      maximum_zoom_level_(PageZoomFactorToZoomLevel(kMaximumPageZoomFactor)),
      does_composite_(does_composite),
      fullscreen_controller_(std::make_unique<FullscreenController>(this)),
      receiver_(this, std::move(page_handle)) {
  if (!web_view_client_)
    DCHECK(!does_composite_);
  Page::PageClients page_clients;
  page_clients.chrome_client = chrome_client_.Get();
  page_ =
      Page::CreateOrdinary(page_clients, opener ? opener->GetPage() : nullptr);
  CoreInitializer::GetInstance().ProvideModulesToPage(*page_, web_view_client_);

  SetVisibilityState(visibility, /*is_initial_state=*/true);

  // We pass this state to Page, but it's only used by the main frame in the
  // page.
  SetInsidePortal(is_inside_portal);

  // When not compositing, keep the Page in the loop so that it will paint all
  // content into the root layer, as multiple layers can only be used when
  // compositing them together later.
  if (does_composite_)
    page_->GetSettings().SetAcceleratedCompositingEnabled(true);

  dev_tools_emulator_ = MakeGarbageCollected<DevToolsEmulator>(this);

  AllInstances().insert(this);

  resize_viewport_anchor_ = MakeGarbageCollected<ResizeViewportAnchor>(*page_);
}

WebViewImpl::~WebViewImpl() {
  DCHECK(!page_);
}

WebDevToolsAgentImpl* WebViewImpl::MainFrameDevToolsAgentImpl() {
  WebLocalFrameImpl* main_frame = MainFrameImpl();
  return main_frame ? main_frame->DevToolsAgentImpl() : nullptr;
}

void WebViewImpl::SetTabKeyCyclesThroughElements(bool value) {
  if (page_)
    page_->SetTabKeyCyclesThroughElements(value);
}

void WebViewImpl::HandleMouseLeave(LocalFrame& main_frame,
                                   const WebMouseEvent& event) {
  SetMouseOverURL(WebURL());
  PageWidgetEventHandler::HandleMouseLeave(main_frame, event);
}

void WebViewImpl::HandleMouseDown(LocalFrame& main_frame,
                                  const WebMouseEvent& event) {
  // If there is a popup open, close it as the user is clicking on the page
  // (outside of the popup). We also save it so we can prevent a click on an
  // element from immediately reopening the same popup.
  //
  // The popup would not be destroyed in this stack normally as it is owned by
  // closership from the RenderWidget, which is owned by the browser via the
  // Close IPC. However if a nested message loop were to happen then the browser
  // close of the RenderWidget and WebPagePopupImpl could feasibly occur inside
  // this method, so holding a reference here ensures we can use the
  // |page_popup| even if it is closed.
  scoped_refptr<WebPagePopupImpl> page_popup;
  if (event.button == WebMouseEvent::Button::kLeft) {
    page_popup = page_popup_;
    CancelPagePopup();
    DCHECK(!page_popup_);
  }

  // Take capture on a mouse down on a plugin so we can send it mouse events.
  // If the hit node is a plugin but a scrollbar is over it don't start mouse
  // capture because it will interfere with the scrollbar receiving events.
  if (event.button == WebMouseEvent::Button::kLeft) {
    HitTestLocation location(main_frame.View()->ConvertFromRootFrame(
        FloatPoint(event.PositionInWidget())));
    HitTestResult result(
        main_frame.GetEventHandler().HitTestResultAtLocation(location));
    result.SetToShadowHostIfInRestrictedShadowRoot();
    Node* hit_node = result.InnerNodeOrImageMapImage();
    auto* html_element = DynamicTo<HTMLElement>(hit_node);
    if (!result.GetScrollbar() && hit_node && hit_node->GetLayoutObject() &&
        hit_node->GetLayoutObject()->IsEmbeddedObject() && html_element &&
        html_element->IsPluginElement()) {
      mouse_capture_element_ = To<HTMLPlugInElement>(hit_node);
      main_frame.Client()->SetMouseCapture(true);
      TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("input", "capturing mouse",
                                        TRACE_ID_LOCAL(this));
    }
  }

  PageWidgetEventHandler::HandleMouseDown(main_frame, event);

  if (page_popup_ && page_popup &&
      page_popup_->HasSamePopupClient(page_popup.get())) {
    // That click triggered a page popup that is the same as the one we just
    // closed.  It needs to be closed.
    CancelPagePopup();
  }

  // Dispatch the contextmenu event regardless of if the click was swallowed.
  if (!GetPage()->GetSettings().GetShowContextMenuOnMouseUp()) {
#if defined(OS_MAC)
    if (event.button == WebMouseEvent::Button::kRight ||
        (event.button == WebMouseEvent::Button::kLeft &&
         event.GetModifiers() & WebMouseEvent::kControlKey))
      MouseContextMenu(event);
#else
    if (event.button == WebMouseEvent::Button::kRight)
      MouseContextMenu(event);
#endif
  }
}

void WebViewImpl::MouseContextMenu(const WebMouseEvent& event) {
  if (!MainFrameImpl() || !MainFrameImpl()->GetFrameView())
    return;

  page_->GetContextMenuController().ClearContextMenu();

  WebMouseEvent transformed_event =
      TransformWebMouseEvent(MainFrameImpl()->GetFrameView(), event);
  transformed_event.menu_source_type = kMenuSourceMouse;

  // Find the right target frame. See issue 1186900.
  HitTestResult result = HitTestResultForRootFramePos(
      FloatPoint(transformed_event.PositionInRootFrame()));
  Frame* target_frame;
  if (result.InnerNodeOrImageMapImage())
    target_frame = result.InnerNodeOrImageMapImage()->GetDocument().GetFrame();
  else
    target_frame = page_->GetFocusController().FocusedOrMainFrame();

  auto* target_local_frame = DynamicTo<LocalFrame>(target_frame);
  if (!target_local_frame)
    return;

  {
    ContextMenuAllowedScope scope;
    target_local_frame->GetEventHandler().SendContextMenuEvent(
        transformed_event);
  }
  // Actually showing the context menu is handled by the ContextMenuController
  // implementation...
}

WebInputEventResult WebViewImpl::HandleMouseUp(LocalFrame& main_frame,
                                               const WebMouseEvent& event) {
  WebInputEventResult result =
      PageWidgetEventHandler::HandleMouseUp(main_frame, event);

  if (GetPage()->GetSettings().GetShowContextMenuOnMouseUp()) {
    // Dispatch the contextmenu event regardless of if the click was swallowed.
    // On Mac/Linux, we handle it on mouse down, not up.
    if (event.button == WebMouseEvent::Button::kRight)
      MouseContextMenu(event);
  }
  return result;
}

WebInputEventResult WebViewImpl::HandleMouseWheel(
    LocalFrame& main_frame,
    const WebMouseWheelEvent& event) {
  CancelPagePopup();
  return PageWidgetEventHandler::HandleMouseWheel(main_frame, event);
}

WebInputEventResult WebViewImpl::HandleGestureEvent(
    const WebGestureEvent& event) {
  if (!web_view_client_ || !web_view_client_->CanHandleGestureEvent()) {
    return WebInputEventResult::kNotHandled;
  }

  WebInputEventResult event_result = WebInputEventResult::kNotHandled;
  bool event_cancelled = false;  // for disambiguation

  // Fling events are not sent to the renderer.
  CHECK(event.GetType() != WebInputEvent::Type::kGestureFlingStart);
  CHECK(event.GetType() != WebInputEvent::Type::kGestureFlingCancel);

  WebGestureEvent scaled_event =
      TransformWebGestureEvent(MainFrameImpl()->GetFrameView(), event);

  // Special handling for double tap and scroll events as we don't want to
  // hit test for them.
  switch (event.GetType()) {
    case WebInputEvent::Type::kGestureDoubleTap:
      if (web_settings_->DoubleTapToZoomEnabled() &&
          MinimumPageScaleFactor() != MaximumPageScaleFactor()) {
        if (auto* main_frame = MainFrameImpl()) {
          IntPoint pos_in_root_frame =
              FlooredIntPoint(scaled_event.PositionInRootFrame());
          WebRect block_bounds =
              main_frame->FrameWidgetImpl()->ComputeBlockBound(
                  pos_in_root_frame, false);
          AnimateDoubleTapZoom(pos_in_root_frame, block_bounds);
        }
      }
      event_result = WebInputEventResult::kHandledSystem;
      MainFrameImpl()->FrameWidgetImpl()->DidHandleGestureEvent(
          event, event_cancelled);
      return event_result;
    case WebInputEvent::Type::kGestureScrollBegin:
    case WebInputEvent::Type::kGestureScrollEnd:
    case WebInputEvent::Type::kGestureScrollUpdate:
      // Scrolling-related gesture events invoke EventHandler recursively for
      // each frame down the chain, doing a single-frame hit-test per frame.
      // This matches handleWheelEvent.  Perhaps we could simplify things by
      // rewriting scroll handling to work inner frame out, and then unify with
      // other gesture events.
      event_result = MainFrameImpl()
                         ->GetFrame()
                         ->GetEventHandler()
                         .HandleGestureScrollEvent(scaled_event);
      MainFrameImpl()->FrameWidgetImpl()->DidHandleGestureEvent(
          event, event_cancelled);
      return event_result;
    default:
      break;
  }

  // Hit test across all frames and do touch adjustment as necessary for the
  // event type.
  GestureEventWithHitTestResults targeted_event =
      page_->DeprecatedLocalMainFrame()->GetEventHandler().TargetGestureEvent(
          scaled_event);

  // Handle link highlighting outside the main switch to avoid getting lost in
  // the complicated set of cases handled below.
  switch (event.GetType()) {
    case WebInputEvent::Type::kGestureShowPress:
      // Queue a highlight animation, then hand off to regular handler.
      EnableTapHighlightAtPoint(targeted_event);
      break;
    case WebInputEvent::Type::kGestureTapCancel:
    case WebInputEvent::Type::kGestureTap:
    case WebInputEvent::Type::kGestureLongPress:
      GetPage()->GetLinkHighlight().StartHighlightAnimationIfNeeded();
      break;
    default:
      break;
  }

  switch (event.GetType()) {
    case WebInputEvent::Type::kGestureTap: {
      {
        ContextMenuAllowedScope scope;
        event_result =
            MainFrameImpl()->GetFrame()->GetEventHandler().HandleGestureEvent(
                targeted_event);
      }

      if (page_popup_ && last_hidden_page_popup_ &&
          page_popup_->HasSamePopupClient(last_hidden_page_popup_.get())) {
        // The tap triggered a page popup that is the same as the one we just
        // closed. It needs to be closed.
        CancelPagePopup();
      }
      // Don't have this value persist outside of a single tap gesture, plus
      // we're done with it now.
      last_hidden_page_popup_ = nullptr;
      break;
    }
    case WebInputEvent::Type::kGestureTwoFingerTap:
    case WebInputEvent::Type::kGestureLongPress:
    case WebInputEvent::Type::kGestureLongTap: {
      if (!MainFrameImpl() || !MainFrameImpl()->GetFrameView())
        break;

      if (event.GetType() == WebInputEvent::Type::kGestureLongTap) {
        if (LocalFrame* inner_frame =
                targeted_event.GetHitTestResult().InnerNodeFrame()) {
          if (!inner_frame->GetEventHandler().LongTapShouldInvokeContextMenu())
            break;
        } else if (!MainFrameImpl()
                        ->GetFrame()
                        ->GetEventHandler()
                        .LongTapShouldInvokeContextMenu()) {
          break;
        }
      }

      page_->GetContextMenuController().ClearContextMenu();
      {
        ContextMenuAllowedScope scope;
        event_result =
            MainFrameImpl()->GetFrame()->GetEventHandler().HandleGestureEvent(
                targeted_event);
      }

      break;
    }
    case WebInputEvent::Type::kGestureTapDown: {
      // Touch pinch zoom and scroll on the page (outside of a popup) must hide
      // the popup. In case of a touch scroll or pinch zoom, this function is
      // called with GestureTapDown rather than a GSB/GSU/GSE or GPB/GPU/GPE.
      // When we close a popup because of a GestureTapDown, we also save it so
      // we can prevent the following GestureTap from immediately reopening the
      // same popup.
      // This value should not persist outside of a gesture, so is cleared by
      // GestureTap (where it is used) and by GestureCancel.
      last_hidden_page_popup_ = page_popup_;
      CancelPagePopup();
      DCHECK(!page_popup_);
      event_result =
          MainFrameImpl()->GetFrame()->GetEventHandler().HandleGestureEvent(
              targeted_event);
      break;
    }
    case WebInputEvent::Type::kGestureTapCancel: {
      // Don't have this value persist outside of a single tap gesture.
      last_hidden_page_popup_ = nullptr;
      event_result =
          MainFrameImpl()->GetFrame()->GetEventHandler().HandleGestureEvent(
              targeted_event);
      break;
    }
    case WebInputEvent::Type::kGestureShowPress: {
      event_result =
          MainFrameImpl()->GetFrame()->GetEventHandler().HandleGestureEvent(
              targeted_event);
      break;
    }
    case WebInputEvent::Type::kGestureTapUnconfirmed: {
      event_result =
          MainFrameImpl()->GetFrame()->GetEventHandler().HandleGestureEvent(
              targeted_event);
      break;
    }
    default: { NOTREACHED(); }
  }
  MainFrameImpl()->FrameWidgetImpl()->DidHandleGestureEvent(event,
                                                            event_cancelled);
  return event_result;
}

bool WebViewImpl::StartPageScaleAnimation(const IntPoint& target_position,
                                          bool use_anchor,
                                          float new_scale,
                                          base::TimeDelta duration) {
  // PageScaleFactor is a property of the main frame only, and only exists when
  // compositing.
  DCHECK(MainFrameImpl());
  DCHECK(does_composite_);

  VisualViewport& visual_viewport = GetPage()->GetVisualViewport();
  gfx::Point clamped_point = target_position;
  if (!use_anchor) {
    clamped_point =
        visual_viewport.ClampDocumentOffsetAtScale(target_position, new_scale);
    if (duration.is_zero()) {
      SetPageScaleFactor(new_scale);

      LocalFrameView* view = MainFrameImpl()->GetFrameView();
      if (view && view->GetScrollableArea()) {
        view->GetScrollableArea()->SetScrollOffset(
            ScrollOffset(clamped_point.x(), clamped_point.y()),
            mojom::blink::ScrollType::kProgrammatic);
      }

      return false;
    }
  }
  if (use_anchor && new_scale == PageScaleFactor())
    return false;

  if (enable_fake_page_scale_animation_for_testing_) {
    fake_page_scale_animation_target_position_ = target_position;
    fake_page_scale_animation_use_anchor_ = use_anchor;
    fake_page_scale_animation_page_scale_factor_ = new_scale;
  } else {
    MainFrameImpl()->FrameWidgetImpl()->StartPageScaleAnimation(
        static_cast<gfx::Vector2d>(target_position), use_anchor, new_scale,
        duration);
  }
  return true;
}

void WebViewImpl::EnableFakePageScaleAnimationForTesting(bool enable) {
  enable_fake_page_scale_animation_for_testing_ = enable;
  fake_page_scale_animation_target_position_ = IntPoint();
  fake_page_scale_animation_use_anchor_ = false;
  fake_page_scale_animation_page_scale_factor_ = 0;
}

void WebViewImpl::AcceptLanguagesChanged() {
  if (web_view_client_)
    FontCache::AcceptLanguagesChanged(web_view_client_->AcceptLanguages());

  if (!GetPage())
    return;

  GetPage()->AcceptLanguagesChanged();
}

WebInputEventResult WebViewImpl::HandleKeyEvent(const WebKeyboardEvent& event) {
  DCHECK((event.GetType() == WebInputEvent::Type::kRawKeyDown) ||
         (event.GetType() == WebInputEvent::Type::kKeyDown) ||
         (event.GetType() == WebInputEvent::Type::kKeyUp));
  TRACE_EVENT2("input", "WebViewImpl::handleKeyEvent", "type",
               WebInputEvent::GetName(event.GetType()), "text",
               String(event.text).Utf8());

  // Please refer to the comments explaining |suppress_next_keypress_event_|.
  //
  // |suppress_next_keypress_event_| is set if the KeyDown is handled by
  // Webkit. A keyDown event is typically associated with a keyPress(char)
  // event and a keyUp event. We reset this flag here as this is a new keyDown
  // event.
  suppress_next_keypress_event_ = false;

  // If there is a popup, it should be the one processing the event, not the
  // page.
  if (page_popup_) {
    page_popup_->HandleKeyEvent(event);
    // We need to ignore the next Char event after this otherwise pressing
    // enter when selecting an item in the popup will go to the page.
    if (WebInputEvent::Type::kRawKeyDown == event.GetType())
      suppress_next_keypress_event_ = true;
    return WebInputEventResult::kHandledSystem;
  }

  Frame* focused_frame = FocusedCoreFrame();
  auto* focused_local_frame = DynamicTo<LocalFrame>(focused_frame);
  if (!focused_local_frame)
    return WebInputEventResult::kNotHandled;

  WebInputEventResult result =
      focused_local_frame->GetEventHandler().KeyEvent(event);
  if (result != WebInputEventResult::kNotHandled) {
    if (WebInputEvent::Type::kRawKeyDown == event.GetType()) {
      // Suppress the next keypress event unless the focused node is a plugin
      // node.  (Flash needs these keypress events to handle non-US keyboards.)
      Element* element = FocusedElement();
      if (element && element->GetLayoutObject() &&
          element->GetLayoutObject()->IsEmbeddedObject()) {
        if (event.windows_key_code == VKEY_TAB) {
          // If the plugin supports keyboard focus then we should not send a tab
          // keypress event.
          WebPluginContainerImpl* plugin_view =
              ToLayoutEmbeddedContent(element->GetLayoutObject())->Plugin();
          if (plugin_view && plugin_view->SupportsKeyboardFocus()) {
            suppress_next_keypress_event_ = true;
          }
        }
      } else {
        suppress_next_keypress_event_ = true;
      }
    }
    return result;
  }

#if !defined(OS_MAC)
  const WebInputEvent::Type kContextMenuKeyTriggeringEventType =
#if defined(OS_WIN)
      WebInputEvent::Type::kKeyUp;
#else
      WebInputEvent::Type::kRawKeyDown;
#endif
  const WebInputEvent::Type kShiftF10TriggeringEventType =
      WebInputEvent::Type::kRawKeyDown;

  bool is_unmodified_menu_key =
      !(event.GetModifiers() & WebInputEvent::kInputModifiers) &&
      event.windows_key_code == VKEY_APPS;
  bool is_shift_f10 = (event.GetModifiers() & WebInputEvent::kInputModifiers) ==
                          WebInputEvent::kShiftKey &&
                      event.windows_key_code == VKEY_F10;
  if ((is_unmodified_menu_key &&
       event.GetType() == kContextMenuKeyTriggeringEventType) ||
      (is_shift_f10 && event.GetType() == kShiftF10TriggeringEventType)) {
    SendContextMenuEvent();
    return WebInputEventResult::kHandledSystem;
  }
#endif  // !defined(OS_MAC)

  return WebInputEventResult::kNotHandled;
}

WebInputEventResult WebViewImpl::HandleCharEvent(
    const WebKeyboardEvent& event) {
  DCHECK_EQ(event.GetType(), WebInputEvent::Type::kChar);
  TRACE_EVENT1("input", "WebViewImpl::handleCharEvent", "text",
               String(event.text).Utf8());

  // Please refer to the comments explaining |suppress_next_keypress_event_|
  // |suppress_next_keypress_event_| is set if the KeyDown is
  // handled by Webkit. A keyDown event is typically associated with a
  // keyPress(char) event and a keyUp event. We reset this flag here as it
  // only applies to the current keyPress event.
  bool suppress = suppress_next_keypress_event_;
  suppress_next_keypress_event_ = false;

  // If there is a popup, it should be the one processing the event, not the
  // page.
  if (page_popup_)
    return page_popup_->HandleKeyEvent(event);

  auto* frame = To<LocalFrame>(FocusedCoreFrame());
  if (!frame) {
    return suppress ? WebInputEventResult::kHandledSuppressed
                    : WebInputEventResult::kNotHandled;
  }

  EventHandler& handler = frame->GetEventHandler();

  if (!event.IsCharacterKey())
    return WebInputEventResult::kHandledSuppressed;

  // Accesskeys are triggered by char events and can't be suppressed.
  if (handler.HandleAccessKey(event))
    return WebInputEventResult::kHandledSystem;

  // Safari 3.1 does not pass off windows system key messages (WM_SYSCHAR) to
  // the eventHandler::keyEvent. We mimic this behavior on all platforms since
  // for now we are converting other platform's key events to windows key
  // events.
  if (event.is_system_key)
    return WebInputEventResult::kNotHandled;

  if (suppress)
    return WebInputEventResult::kHandledSuppressed;

  WebInputEventResult result = handler.KeyEvent(event);
  if (result != WebInputEventResult::kNotHandled)
    return result;

  return WebInputEventResult::kNotHandled;
}

WebRect WebViewImpl::WidenRectWithinPageBounds(const WebRect& source,
                                               int target_margin,
                                               int minimum_margin) {
  // Caller should guarantee that the main frame exists and is local.
  DCHECK(MainFrame());
  DCHECK(MainFrame()->IsWebLocalFrame());
  WebSize max_size = MainFrame()->ToWebLocalFrame()->DocumentSize();
  IntSize scroll_offset = MainFrame()->ToWebLocalFrame()->GetScrollOffset();

  int left_margin = target_margin;
  int right_margin = target_margin;

  const int absolute_source_x = source.x + scroll_offset.Width();
  if (left_margin > absolute_source_x) {
    left_margin = absolute_source_x;
    right_margin = std::max(left_margin, minimum_margin);
  }

  const int maximum_right_margin =
      max_size.width - (source.width + absolute_source_x);
  if (right_margin > maximum_right_margin) {
    right_margin = maximum_right_margin;
    left_margin = std::min(left_margin, std::max(right_margin, minimum_margin));
  }

  const int new_width = source.width + left_margin + right_margin;
  const int new_x = source.x - left_margin;

  DCHECK_GE(new_width, 0);
  DCHECK_LE(scroll_offset.Width() + new_x + new_width, max_size.width);

  return WebRect(new_x, source.y, new_width, source.height);
}

float WebViewImpl::MaximumLegiblePageScale() const {
  // Pages should be as legible as on desktop when at dpi scale, so no
  // need to zoom in further when automatically determining zoom level
  // (after double tap, find in page, etc), though the user should still
  // be allowed to manually pinch zoom in further if they desire.
  if (GetPage()) {
    return maximum_legible_scale_ *
           GetPage()->GetSettings().GetAccessibilityFontScaleFactor();
  }
  return maximum_legible_scale_;
}

void WebViewImpl::ComputeScaleAndScrollForBlockRect(
    const gfx::Point& hit_point_in_root_frame,
    const WebRect& block_rect_in_root_frame,
    float padding,
    float default_scale_when_already_legible,
    float& scale,
    IntPoint& scroll) {
  scale = PageScaleFactor();
  scroll = IntPoint();

  WebRect rect = block_rect_in_root_frame;

  if (!rect.IsEmpty()) {
    float default_margin = doubleTapZoomContentDefaultMargin;
    float minimum_margin = doubleTapZoomContentMinimumMargin;
    // We want the margins to have the same physical size, which means we
    // need to express them in post-scale size. To do that we'd need to know
    // the scale we're scaling to, but that depends on the margins. Instead
    // we express them as a fraction of the target rectangle: this will be
    // correct if we end up fully zooming to it, and won't matter if we
    // don't.
    rect = WidenRectWithinPageBounds(
        rect, static_cast<int>(default_margin * rect.width / size_.width()),
        static_cast<int>(minimum_margin * rect.width / size_.width()));
    // Fit block to screen, respecting limits.
    scale = static_cast<float>(size_.width()) / rect.width;
    scale = std::min(scale, MaximumLegiblePageScale());
    if (PageScaleFactor() < default_scale_when_already_legible)
      scale = std::max(scale, default_scale_when_already_legible);
    scale = ClampPageScaleFactorToLimits(scale);
  }

  // FIXME: If this is being called for auto zoom during find in page,
  // then if the user manually zooms in it'd be nice to preserve the
  // relative increase in zoom they caused (if they zoom out then it's ok
  // to zoom them back in again). This isn't compatible with our current
  // double-tap zoom strategy (fitting the containing block to the screen)
  // though.

  float screen_width = size_.width() / scale;
  float screen_height = size_.height() / scale;

  // Scroll to vertically align the block.
  if (rect.height < screen_height) {
    // Vertically center short blocks.
    rect.y -= 0.5 * (screen_height - rect.height);
  } else {
    // Ensure position we're zooming to (+ padding) isn't off the bottom of
    // the screen.
    rect.y = std::max<float>(
        rect.y, hit_point_in_root_frame.y() + padding - screen_height);
  }  // Otherwise top align the block.

  // Do the same thing for horizontal alignment.
  if (rect.width < screen_width) {
    rect.x -= 0.5 * (screen_width - rect.width);
  } else {
    rect.x = std::max<float>(
        rect.x, hit_point_in_root_frame.x() + padding - screen_width);
  }
  scroll.SetX(rect.x);
  scroll.SetY(rect.y);

  scale = ClampPageScaleFactorToLimits(scale);
  scroll = MainFrameImpl()->GetFrameView()->RootFrameToDocument(scroll);
  scroll =
      GetPage()->GetVisualViewport().ClampDocumentOffsetAtScale(scroll, scale);
}

static Node* FindCursorDefiningAncestor(Node* node, LocalFrame* frame) {
  // Go up the tree to find the node that defines a mouse cursor style
  while (node) {
    if (node->GetLayoutObject()) {
      ECursor cursor = node->GetLayoutObject()->Style()->Cursor();
      if (cursor != ECursor::kAuto ||
          frame->GetEventHandler().UseHandCursor(node, node->IsLink()))
        break;
    }
    node = LayoutTreeBuilderTraversal::Parent(*node);
  }

  return node;
}

static bool ShowsHandCursor(Node* node, LocalFrame* frame) {
  if (!node || !node->GetLayoutObject())
    return false;

  ECursor cursor = node->GetLayoutObject()->Style()->Cursor();
  return cursor == ECursor::kPointer ||
         (cursor == ECursor::kAuto &&
          frame->GetEventHandler().UseHandCursor(node, node->IsLink()));
}

// This is for tap (link) highlight and is tested in
// link_highlight_impl_test.cc.
Node* WebViewImpl::BestTapNode(
    const GestureEventWithHitTestResults& targeted_tap_event) {
  TRACE_EVENT0("input", "WebViewImpl::bestTapNode");

  Page* page = page_.Get();
  if (!page || !page->MainFrame())
    return nullptr;

  Node* best_touch_node = targeted_tap_event.GetHitTestResult().InnerNode();
  if (!best_touch_node)
    return nullptr;

  // We might hit something like an image map that has no layoutObject on it
  // Walk up the tree until we have a node with an attached layoutObject
  while (!best_touch_node->GetLayoutObject()) {
    best_touch_node = LayoutTreeBuilderTraversal::Parent(*best_touch_node);
    if (!best_touch_node)
      return nullptr;
  }

  // Editable nodes should not be highlighted (e.g., <input>)
  if (HasEditableStyle(*best_touch_node))
    return nullptr;

  Node* cursor_defining_ancestor = FindCursorDefiningAncestor(
      best_touch_node, page->DeprecatedLocalMainFrame());
  // We show a highlight on tap only when the current node shows a hand cursor
  if (!cursor_defining_ancestor ||
      !ShowsHandCursor(cursor_defining_ancestor,
                       page->DeprecatedLocalMainFrame())) {
    return nullptr;
  }

  // We should pick the largest enclosing node with hand cursor set. We do this
  // by first jumping up to cursorDefiningAncestor (which is already known to
  // have hand cursor set). Then we locate the next cursor-defining ancestor up
  // in the the tree and repeat the jumps as long as the node has hand cursor
  // set.
  do {
    best_touch_node = cursor_defining_ancestor;
    cursor_defining_ancestor = FindCursorDefiningAncestor(
        LayoutTreeBuilderTraversal::Parent(*best_touch_node),
        page->DeprecatedLocalMainFrame());
  } while (cursor_defining_ancestor &&
           ShowsHandCursor(cursor_defining_ancestor,
                           page->DeprecatedLocalMainFrame()));

  // This happens in cases like:
  // <div style="display: contents; cursor: pointer">Text</div>.
  // The text node inherits cursor: pointer and the div doesn't have a
  // LayoutObject, so |best_touch_node| is the text node here. We should not
  // return the text node because it can't have touch actions.
  if (best_touch_node->IsTextNode())
    return nullptr;

  return best_touch_node;
}

void WebViewImpl::EnableTapHighlightAtPoint(
    const GestureEventWithHitTestResults& targeted_tap_event) {
  Node* touch_node = BestTapNode(targeted_tap_event);
  GetPage()->GetLinkHighlight().SetTapHighlight(touch_node);
  UpdateLifecycle(WebLifecycleUpdate::kAll,
                  DocumentUpdateReason::kTapHighlight);
}

void WebViewImpl::AnimateDoubleTapZoom(const gfx::Point& point_in_root_frame,
                                       const WebRect& rect_to_zoom) {
  DCHECK(MainFrameImpl());

  float scale;
  IntPoint scroll;

  ComputeScaleAndScrollForBlockRect(
      point_in_root_frame, rect_to_zoom, touchPointPadding,
      MinimumPageScaleFactor() * doubleTapZoomAlreadyLegibleRatio, scale,
      scroll);

  bool still_at_previous_double_tap_scale =
      (PageScaleFactor() == double_tap_zoom_page_scale_factor_ &&
       double_tap_zoom_page_scale_factor_ != MinimumPageScaleFactor()) ||
      double_tap_zoom_pending_;

  bool scale_unchanged = fabs(PageScaleFactor() - scale) < minScaleDifference;
  bool should_zoom_out = rect_to_zoom.IsEmpty() || scale_unchanged ||
                         still_at_previous_double_tap_scale;

  bool is_animating;

  if (should_zoom_out) {
    scale = MinimumPageScaleFactor();
    IntPoint target_position =
        MainFrameImpl()->GetFrameView()->RootFrameToDocument(
            IntPoint(point_in_root_frame.x(), point_in_root_frame.y()));
    is_animating = StartPageScaleAnimation(target_position, true, scale,
                                           kDoubleTapZoomAnimationDuration);
  } else {
    is_animating = StartPageScaleAnimation(scroll, false, scale,
                                           kDoubleTapZoomAnimationDuration);
  }

  // TODO(dglazkov): The only reason why we're using isAnimating and not just
  // checking for layer_tree_view_->HasPendingPageScaleAnimation() is because of
  // fake page scale animation plumbing for testing, which doesn't actually
  // initiate a page scale animation.
  if (is_animating) {
    double_tap_zoom_page_scale_factor_ = scale;
    double_tap_zoom_pending_ = true;
  }
}

void WebViewImpl::ZoomToFindInPageRect(const WebRect& rect_in_root_frame) {
  DCHECK(MainFrameImpl());

  WebRect block_bounds = MainFrameImpl()->FrameWidgetImpl()->ComputeBlockBound(
      gfx::Point(rect_in_root_frame.x + rect_in_root_frame.width / 2,
                 rect_in_root_frame.y + rect_in_root_frame.height / 2),
      true);

  if (block_bounds.IsEmpty()) {
    // Keep current scale (no need to scroll as x,y will normally already
    // be visible). FIXME: Revisit this if it isn't always true.
    return;
  }

  float scale;
  IntPoint scroll;

  ComputeScaleAndScrollForBlockRect(
      gfx::Point(rect_in_root_frame.x, rect_in_root_frame.y), block_bounds,
      nonUserInitiatedPointPadding, MinimumPageScaleFactor(), scale, scroll);

  StartPageScaleAnimation(scroll, false, scale, kFindInPageAnimationDuration);
}

#if !defined(OS_MAC)
// Mac has no way to open a context menu based on a keyboard event.
WebInputEventResult WebViewImpl::SendContextMenuEvent() {
  // The contextMenuController() holds onto the last context menu that was
  // popped up on the page until a new one is created. We need to clear
  // this menu before propagating the event through the DOM so that we can
  // detect if we create a new menu for this event, since we won't create
  // a new menu if the DOM swallows the event and the defaultEventHandler does
  // not run.
  GetPage()->GetContextMenuController().ClearContextMenu();

  {
    ContextMenuAllowedScope scope;
    Frame* focused_frame = GetPage()->GetFocusController().FocusedOrMainFrame();
    auto* focused_local_frame = DynamicTo<LocalFrame>(focused_frame);
    if (!focused_local_frame)
      return WebInputEventResult::kNotHandled;
    // Firefox reveal focus based on "keydown" event but not "contextmenu"
    // event, we match FF.
    if (Element* focused_element =
            focused_local_frame->GetDocument()->FocusedElement())
      focused_element->scrollIntoViewIfNeeded();
    return focused_local_frame->GetEventHandler().ShowNonLocatedContextMenu(
        nullptr, kMenuSourceKeyboard);
  }
}
#else
WebInputEventResult WebViewImpl::SendContextMenuEvent() {
  return WebInputEventResult::kNotHandled;
}
#endif

WebPagePopupImpl* WebViewImpl::OpenPagePopup(PagePopupClient* client) {
  DCHECK(client);

  // This guarantees there is never more than 1 PagePopup active at a time.
  CancelPagePopup();
  DCHECK(!page_popup_);

  WebLocalFrameImpl* frame = WebLocalFrameImpl::FromFrame(
      client->OwnerElement().GetDocument().GetFrame()->LocalFrameRoot());
  WebPagePopup* popup_widget = web_view_client_->CreatePopup(frame);
  // CreatePopup returns nullptr if this renderer process is about to die.
  if (!popup_widget)
    return nullptr;
  page_popup_ = To<WebPagePopupImpl>(popup_widget);
  page_popup_->Initialize(this, client);
  EnablePopupMouseWheelEventListener(frame);
  return page_popup_.get();
}

void WebViewImpl::CancelPagePopup() {
  if (page_popup_)
    page_popup_->Cancel();
}

void WebViewImpl::ClosePagePopup(PagePopup* popup) {
  DCHECK(popup);
  auto* popup_impl = To<WebPagePopupImpl>(popup);
  DCHECK_EQ(page_popup_.get(), popup_impl);
  if (page_popup_.get() != popup_impl)
    return;
  page_popup_->ClosePopup();
}

void WebViewImpl::CleanupPagePopup() {
  page_popup_ = nullptr;
  DisablePopupMouseWheelEventListener();
}

void WebViewImpl::UpdatePagePopup() {
  if (page_popup_)
    page_popup_->Update();
}

void WebViewImpl::EnablePopupMouseWheelEventListener(
    WebLocalFrameImpl* local_root) {
  DCHECK(!popup_mouse_wheel_event_listener_);
  Document* document = local_root->GetDocument();
  DCHECK(document);
  // We register an empty event listener, EmptyEventListener, so that mouse
  // wheel events get sent to the WebView.
  popup_mouse_wheel_event_listener_ =
      MakeGarbageCollected<EmptyEventListener>();
  document->addEventListener(event_type_names::kMousewheel,
                             popup_mouse_wheel_event_listener_, false);
  local_root_with_empty_mouse_wheel_listener_ = local_root;
}

void WebViewImpl::DisablePopupMouseWheelEventListener() {
  // TODO(kenrb): Concerns the same as in enablePopupMouseWheelEventListener.
  // See https://crbug.com/566130
  DCHECK(popup_mouse_wheel_event_listener_);
  Document* document =
      local_root_with_empty_mouse_wheel_listener_->GetDocument();
  DCHECK(document);
  // Document may have already removed the event listener, for instance, due
  // to a navigation, but remove it anyway.
  document->removeEventListener(event_type_names::kMousewheel,
                                popup_mouse_wheel_event_listener_.Release(),
                                false);
  local_root_with_empty_mouse_wheel_listener_ = nullptr;
}

LocalDOMWindow* WebViewImpl::PagePopupWindow() const {
  return page_popup_ ? page_popup_->Window() : nullptr;
}

Frame* WebViewImpl::FocusedCoreFrame() const {
  Page* page = page_.Get();
  return page ? page->GetFocusController().FocusedOrMainFrame() : nullptr;
}

// WebWidget ------------------------------------------------------------------

void WebViewImpl::Close() {
  // Closership is a single relationship, so only 1 call to Close() should
  // occur.
  CHECK(page_);
  DCHECK(AllInstances().Contains(this));
  AllInstances().erase(this);

  // Invalidate any weak ptrs as we are starting to shutdown.
  weak_ptr_factory_.InvalidateWeakPtrs();

  // Initiate shutdown for the entire frameset.  This will cause a lot of
  // notifications to be sent. This will detach all frames in this WebView's
  // frame tree.
  page_->WillBeDestroyed();

  // TODO(bokan): Temporary debugging added to diagnose
  // https://crbug.com/992315. Somehow we're synchronously calling
  // WebViewImpl::Close while handling an input event.
  CHECK(!debug_inside_input_handling_);
  page_.Clear();

  // Reset the delegate to prevent notifications being sent as we're being
  // deleted.
  web_view_client_ = nullptr;

  Release();  // Balances a reference acquired in WebView::Create
}

gfx::Size WebViewImpl::Size() {
  return size_;
}

void WebViewImpl::ResizeVisualViewport(const gfx::Size& new_size) {
  GetPage()->GetVisualViewport().SetSize(WebSize(new_size));
  GetPage()->GetVisualViewport().ClampToBoundaries();
}

void WebViewImpl::UpdateICBAndResizeViewport(
    const IntSize& visible_viewport_size) {
  // We'll keep the initial containing block size from changing when the top
  // controls hide so that the ICB will always be the same size as the
  // viewport with the browser controls shown.
  IntSize icb_size = IntSize(size_);
  if (GetBrowserControls().PermittedState() ==
          cc::BrowserControlsState::kBoth &&
      !GetBrowserControls().ShrinkViewport()) {
    icb_size.Expand(0, -(GetBrowserControls().TotalHeight() -
                         GetBrowserControls().TotalMinHeight()));
  }

  GetPageScaleConstraintsSet().DidChangeInitialContainingBlockSize(icb_size);

  UpdatePageDefinedViewportConstraints(MainFrameImpl()
                                           ->GetFrame()
                                           ->GetDocument()
                                           ->GetViewportData()
                                           .GetViewportDescription());
  UpdateMainFrameLayoutSize();

  GetPage()->GetVisualViewport().SetSize(visible_viewport_size);

  if (MainFrameImpl()->GetFrameView()) {
    MainFrameImpl()->GetFrameView()->SetInitialViewportSize(icb_size);
    if (!MainFrameImpl()->GetFrameView()->NeedsLayout())
      resize_viewport_anchor_->ResizeFrameView(MainFrameSize());
  }

  // The boundaries are not properly established until after the frame view is
  // also resized, as demonstrated by
  // VisualViewportTest.TestBrowserControlsAdjustmentAndResize.
  GetPage()->GetVisualViewport().ClampToBoundaries();
}

void WebViewImpl::UpdateBrowserControlsConstraint(
    cc::BrowserControlsState constraint) {
  cc::BrowserControlsState old_permitted_state =
      GetBrowserControls().PermittedState();

  GetBrowserControls().UpdateConstraintsAndState(
      constraint, cc::BrowserControlsState::kBoth);

  // If the controls are going from a locked hidden to unlocked state, or vice
  // versa, the ICB size needs to change but we can't rely on getting a
  // WebViewImpl::resize since the top controls shown state may not have
  // changed.
  if ((old_permitted_state == cc::BrowserControlsState::kHidden &&
       constraint == cc::BrowserControlsState::kBoth) ||
      (old_permitted_state == cc::BrowserControlsState::kBoth &&
       constraint == cc::BrowserControlsState::kHidden)) {
    UpdateICBAndResizeViewport(GetPage()->GetVisualViewport().Size());
  }
}

void WebViewImpl::DidUpdateBrowserControls() {
  // BrowserControls are a feature whereby the browser can introduce an
  // interactable element [e.g. search box] that grows/shrinks in height as the
  // user scrolls the web contents.
  //
  // This method is called by the BrowserControls class to let the compositor
  // know that the browser controls have been updated. This is only relevant if
  // the main frame is local because BrowserControls only affects the main
  // frame's viewport, and are only affected by main frame scrolling.
  //
  // The relevant state is stored on the BrowserControls object even if the main
  // frame is remote. If the main frame becomes local, the state will be
  // restored by the first commit, since the state is checked in every call to
  // ApplyScrollAndScale().
  WebLocalFrameImpl* main_frame = MainFrameImpl();
  if (!main_frame)
    return;

  WebFrameWidgetBase* widget = main_frame->LocalRootFrameWidget();
  widget->SetBrowserControlsShownRatio(GetBrowserControls().TopShownRatio(),
                                       GetBrowserControls().BottomShownRatio());
  widget->SetBrowserControlsParams(GetBrowserControls().Params());

  VisualViewport& visual_viewport = GetPage()->GetVisualViewport();

  {
    // This object will save the current visual viewport offset w.r.t. the
    // document and restore it when the object goes out of scope. It's
    // needed since the browser controls adjustment will change the maximum
    // scroll offset and we may need to reposition them to keep the user's
    // apparent position unchanged.
    ResizeViewportAnchor::ResizeScope resize_scope(*resize_viewport_anchor_);

    visual_viewport.SetBrowserControlsAdjustment(
        GetBrowserControls().UnreportedSizeAdjustment());
  }
}

BrowserControls& WebViewImpl::GetBrowserControls() {
  return GetPage()->GetBrowserControls();
}

void WebViewImpl::ResizeViewWhileAnchored(
    cc::BrowserControlsParams params,
    const IntSize& visible_viewport_size) {
  DCHECK(MainFrameImpl());

  GetBrowserControls().SetParams(params);

  {
    // Avoids unnecessary invalidations while various bits of state in
    // TextAutosizer are updated.
    TextAutosizer::DeferUpdatePageInfo defer_update_page_info(GetPage());
    LocalFrameView* frame_view = MainFrameImpl()->GetFrameView();
    IntSize old_size = frame_view->Size();
    UpdateICBAndResizeViewport(visible_viewport_size);
    IntSize new_size = frame_view->Size();
    frame_view->MarkViewportConstrainedObjectsForLayout(
        old_size.Width() != new_size.Width(),
        old_size.Height() != new_size.Height());
  }

  fullscreen_controller_->UpdateSize();

  // Update lifecycle phases immediately to recalculate the minimum scale limit
  // for rotation anchoring, and to make sure that no lifecycle states are
  // stale if this WebView is embedded in another one.
  UpdateLifecycle(WebLifecycleUpdate::kAll, DocumentUpdateReason::kSizeChange);
}

void WebViewImpl::ResizeWithBrowserControls(
    const gfx::Size& new_size,
    float top_controls_height,
    float bottom_controls_height,
    bool browser_controls_shrink_layout) {
  ResizeWithBrowserControls(
      new_size, new_size,
      {top_controls_height, GetBrowserControls().TopMinHeight(),
       bottom_controls_height, GetBrowserControls().BottomMinHeight(),
       GetBrowserControls().AnimateHeightChanges(),
       browser_controls_shrink_layout});
}

void WebViewImpl::ResizeWithBrowserControls(
    const gfx::Size& main_frame_widget_size,
    const gfx::Size& visible_viewport_size,
    cc::BrowserControlsParams browser_controls_params) {
  if (should_auto_resize_) {
    // When auto-resizing only the viewport size comes from the browser, while
    // the widget size is determined in the renderer.
    ResizeVisualViewport(visible_viewport_size);
    return;
  }

  if (size_ == main_frame_widget_size &&
      GetPage()->GetVisualViewport().Size() == IntSize(visible_viewport_size) &&
      GetBrowserControls().Params() == browser_controls_params)
    return;

  if (GetPage()->MainFrame() && !GetPage()->MainFrame()->IsLocalFrame()) {
    // Viewport resize for a remote main frame does not require any
    // particular action, but the state needs to reflect the correct size
    // so that it can be used for initialization if the main frame gets
    // swapped to a LocalFrame at a later time.
    size_ = main_frame_widget_size;
    GetPageScaleConstraintsSet().DidChangeInitialContainingBlockSize(
        IntSize(size_));
    GetPage()->GetVisualViewport().SetSize(IntSize(size_));
    GetPage()->GetBrowserControls().SetParams(browser_controls_params);
    return;
  }

  WebLocalFrameImpl* main_frame = MainFrameImpl();
  if (!main_frame)
    return;

  LocalFrameView* view = main_frame->GetFrameView();
  if (!view)
    return;

  VisualViewport& visual_viewport = GetPage()->GetVisualViewport();

  bool is_rotation =
      GetPage()->GetSettings().GetMainFrameResizesAreOrientationChanges() &&
      size_.width() && ContentsSize().Width() &&
      main_frame_widget_size.width() != size_.width() &&
      !fullscreen_controller_->IsFullscreenOrTransitioning();
  size_ = main_frame_widget_size;

  FloatSize viewport_anchor_coords(viewportAnchorCoordX, viewportAnchorCoordY);
  if (is_rotation) {
    RotationViewportAnchor anchor(*view, visual_viewport,
                                  viewport_anchor_coords,
                                  GetPageScaleConstraintsSet());
    ResizeViewWhileAnchored(browser_controls_params,
                            IntSize(visible_viewport_size));
  } else {
    ResizeViewportAnchor::ResizeScope resize_scope(*resize_viewport_anchor_);
    ResizeViewWhileAnchored(browser_controls_params,
                            IntSize(visible_viewport_size));
  }
  SendResizeEventForMainFrame();
}

void WebViewImpl::Resize(const gfx::Size& new_size) {
  if (should_auto_resize_ || size_ == new_size)
    return;

  ResizeWithBrowserControls(new_size, GetBrowserControls().TopHeight(),
                            GetBrowserControls().BottomHeight(),
                            GetBrowserControls().ShrinkViewport());
}

void WebViewImpl::SetScreenOrientationOverrideForTesting(
    base::Optional<blink::mojom::ScreenOrientation> orientation) {
  screen_orientation_override_ = orientation;

  // Since we updated the override value, notify all widgets.
  for (WebFrame* frame = MainFrame(); frame; frame = frame->TraverseNext()) {
    if (frame->IsWebLocalFrame()) {
      if (WebFrameWidgetBase* widget = static_cast<WebFrameWidgetBase*>(
              frame->ToWebLocalFrame()->FrameWidget()))
        widget->UpdateScreenInfo(widget->GetScreenInfo());
    }
  }
}

void WebViewImpl::UseSynchronousResizeModeForTesting(bool enable) {
  web_widget_->UseSynchronousResizeModeForTesting(enable);
}

void WebViewImpl::SetWindowRectSynchronouslyForTesting(
    const gfx::Rect& new_window_rect) {
  web_widget_->SetWindowRectSynchronouslyForTesting(new_window_rect);
}

base::Optional<mojom::blink::ScreenOrientation>
WebViewImpl::ScreenOrientationOverride() {
  return screen_orientation_override_;
}

void WebViewImpl::DidEnterFullscreen() {
  fullscreen_controller_->DidEnterFullscreen();
}

void WebViewImpl::DidExitFullscreen() {
  fullscreen_controller_->DidExitFullscreen();
}

void WebViewImpl::SetMainFrameViewWidget(WebViewFrameWidget* widget) {
  web_widget_ = widget;
}

void WebViewImpl::SetMouseOverURL(const KURL& url) {
  mouse_over_url_ = url;
  UpdateTargetURL(mouse_over_url_, focus_url_);
}

void WebViewImpl::SetKeyboardFocusURL(const KURL& url) {
  focus_url_ = url;
  UpdateTargetURL(focus_url_, mouse_over_url_);
}

WebViewFrameWidget* WebViewImpl::MainFrameViewWidget() {
  return web_widget_;
}

void WebViewImpl::SetSuppressFrameRequestsWorkaroundFor704763Only(
    bool suppress_frame_requests) {
  page_->Animator().SetSuppressFrameRequestsWorkaroundFor704763Only(
      suppress_frame_requests);
}
void WebViewImpl::BeginFrame(base::TimeTicks last_frame_time) {
  TRACE_EVENT1("blink", "WebViewImpl::beginFrame", "frameTime",
               last_frame_time);
  DCHECK(!last_frame_time.is_null());

  if (!MainFrameImpl())
    return;

  MainFrameImpl()
      ->GetFrame()
      ->GetEventHandler()
      .RecomputeMouseHoverStateIfNeeded();

  if (LocalFrameView* view = MainFrameImpl()->GetFrameView()) {
    if (FragmentAnchor* anchor = view->GetFragmentAnchor())
      anchor->PerformPreRafActions();
  }

  DocumentLifecycle::AllowThrottlingScope throttling_scope(
      MainFrameImpl()->GetFrame()->GetDocument()->Lifecycle());

  base::Optional<LocalFrameUkmAggregator::ScopedUkmHierarchicalTimer> ukm_timer;
  if (WidgetBase::ShouldRecordBeginMainFrameMetrics()) {
    ukm_timer.emplace(MainFrameImpl()
                          ->GetFrame()
                          ->View()
                          ->EnsureUkmAggregator()
                          .GetScopedTimer(LocalFrameUkmAggregator::kAnimate));
  }
  PageWidgetDelegate::Animate(*page_, last_frame_time);
}

void WebViewImpl::BeginUpdateLayers() {
  if (MainFrameImpl())
    update_layers_start_time_.emplace(base::TimeTicks::Now());
}

void WebViewImpl::EndUpdateLayers() {
  if (MainFrameImpl()) {
    DCHECK(update_layers_start_time_);
    MainFrameImpl()->GetFrame()->View()->EnsureUkmAggregator().RecordSample(
        LocalFrameUkmAggregator::kUpdateLayers,
        update_layers_start_time_.value(), base::TimeTicks::Now());
    probe::LayerTreeDidChange(MainFrameImpl()->GetFrame());
  }
  update_layers_start_time_.reset();
}

void WebViewImpl::RecordStartOfFrameMetrics() {
  if (!MainFrameImpl())
    return;

  MainFrameImpl()->GetFrame()->View()->EnsureUkmAggregator().BeginMainFrame();
}

void WebViewImpl::RecordEndOfFrameMetrics(
    base::TimeTicks frame_begin_time,
    cc::ActiveFrameSequenceTrackers trackers) {
  if (!MainFrameImpl())
    return;

  MainFrameImpl()
      ->GetFrame()
      ->View()
      ->EnsureUkmAggregator()
      .RecordEndOfFrameMetrics(frame_begin_time, base::TimeTicks::Now(),
                               trackers);
}

std::unique_ptr<cc::BeginMainFrameMetrics>
WebViewImpl::GetBeginMainFrameMetrics() {
  if (!MainFrameImpl())
    return nullptr;

  return MainFrameImpl()
      ->GetFrame()
      ->View()
      ->EnsureUkmAggregator()
      .GetBeginMainFrameMetrics();
}

void WebViewImpl::UpdateLifecycle(WebLifecycleUpdate requested_update,
                                  DocumentUpdateReason reason) {
  TRACE_EVENT0("blink", "WebViewImpl::updateAllLifecyclePhases");
  if (!MainFrameImpl())
    return;

  DocumentLifecycle::AllowThrottlingScope throttling_scope(
      MainFrameImpl()->GetFrame()->GetDocument()->Lifecycle());

  PageWidgetDelegate::UpdateLifecycle(*page_, *MainFrameImpl()->GetFrame(),
                                      requested_update, reason);
  if (requested_update != WebLifecycleUpdate::kAll)
    return;

  UpdatePagePopup();

  // There is no background color for non-composited WebViews (eg printing).
  if (does_composite_) {
    SkColor background_color = BackgroundColor();
    MainFrameImpl()->FrameWidgetImpl()->SetBackgroundColor(background_color);
    if (background_color != last_background_color_) {
      last_background_color_ = background_color;
      if (Page* page = page_.Get()) {
        if (auto* main_local_frame = DynamicTo<LocalFrame>(page->MainFrame()))
          main_local_frame->DidChangeBackgroundColor(background_color);
      }
    }
  }

  if (LocalFrameView* view = MainFrameImpl()->GetFrameView()) {
    LocalFrame* frame = MainFrameImpl()->GetFrame();
    WebFrameWidgetBase* frame_widget =
        WebLocalFrameImpl::FromFrame(frame)->LocalRootFrameWidget();

    if (should_dispatch_first_visually_non_empty_layout_ &&
        view->IsVisuallyNonEmpty()) {
      should_dispatch_first_visually_non_empty_layout_ = false;
      // TODO(esprehn): Move users of this callback to something
      // better, the heuristic for "visually non-empty" is bad.
      frame_widget->DidMeaningfulLayout(WebMeaningfulLayout::kVisuallyNonEmpty);
    }

    if (should_dispatch_first_layout_after_finished_parsing_ &&
        frame->GetDocument()->HasFinishedParsing()) {
      should_dispatch_first_layout_after_finished_parsing_ = false;
      frame_widget->DidMeaningfulLayout(WebMeaningfulLayout::kFinishedParsing);
    }

    if (should_dispatch_first_layout_after_finished_loading_ &&
        frame->GetDocument()->IsLoadCompleted()) {
      should_dispatch_first_layout_after_finished_loading_ = false;
      frame_widget->DidMeaningfulLayout(WebMeaningfulLayout::kFinishedLoading);
    }
  }
}

void WebViewImpl::PaintContent(cc::PaintCanvas* canvas, const gfx::Rect& rect) {
  // This should only be used when compositing is not being used for this
  // WebView, and it is painting into the recording of its parent.
  DCHECK(!does_composite_);
  // Non-composited WebViews always have a local main frame.
  DCHECK(MainFrameImpl());

  if (rect.IsEmpty())
    return;

  LocalFrameView& main_view = *MainFrameImpl()->GetFrame()->View();
  DCHECK(main_view.GetLayoutView()->GetDocument().Lifecycle().GetState() ==
         DocumentLifecycle::kPaintClean);

  PaintRecordBuilder builder;
  main_view.PaintOutsideOfLifecycle(builder.Context(), kGlobalPaintNormalPhase,
                                    CullRect(IntRect(rect)));
  // Don't bother to save/restore here as the caller is expecting the canvas
  // to be modified and take care of it.
  canvas->clipRect(gfx::RectToSkRect(rect));
  builder.EndRecording(*canvas, main_view.GetLayoutView()
                                    ->FirstFragment()
                                    .LocalBorderBoxProperties()
                                    .Unalias());
}

// static
void WebView::ApplyWebPreferences(const web_pref::WebPreferences& prefs,
                                  WebView* web_view) {
  WebViewImpl* web_view_impl = static_cast<WebViewImpl*>(web_view);
  WebSettings* settings = web_view->GetSettings();
  ApplyFontsFromMap(prefs.standard_font_family_map,
                    SetStandardFontFamilyWrapper, settings);
  ApplyFontsFromMap(prefs.fixed_font_family_map, SetFixedFontFamilyWrapper,
                    settings);
  ApplyFontsFromMap(prefs.serif_font_family_map, SetSerifFontFamilyWrapper,
                    settings);
  ApplyFontsFromMap(prefs.sans_serif_font_family_map,
                    SetSansSerifFontFamilyWrapper, settings);
  ApplyFontsFromMap(prefs.cursive_font_family_map, SetCursiveFontFamilyWrapper,
                    settings);
  ApplyFontsFromMap(prefs.fantasy_font_family_map, SetFantasyFontFamilyWrapper,
                    settings);
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

  // Enable new canvas 2d api features
  WebRuntimeFeatures::EnableNewCanvas2DAPI(prefs.new_canvas_2d_api_enabled);

  // Disable antialiasing for 2d canvas if requested on the command line.
  settings->SetAntialiased2dCanvasEnabled(
      !prefs.antialiased_2d_canvas_disabled);

  // Disable antialiasing of clips for 2d canvas if requested on the command
  // line.
  settings->SetAntialiasedClips2dCanvasEnabled(
      prefs.antialiased_clips_2d_canvas_enabled);

  // Tabs to link is not part of the settings. WebCore calls
  // ChromeClient::tabsToLinks which is part of the glue code.
  web_view_impl->SetTabsToLinks(prefs.tabs_to_links);

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
  settings->SetPrimaryPointerType(prefs.primary_pointer_type);
  settings->SetAvailableHoverTypes(prefs.available_hover_types);
  settings->SetPrimaryHoverType(prefs.primary_hover_type);
  settings->SetBarrelButtonForDragEnabled(prefs.barrel_button_for_drag_enabled);

  settings->SetEditingBehavior(prefs.editing_behavior);

  settings->SetSupportsMultipleWindows(prefs.supports_multiple_windows);

  settings->SetMainFrameClipsContent(!prefs.record_whole_document);

  settings->SetSmartInsertDeleteEnabled(prefs.smart_insert_delete_enabled);

  settings->SetSpatialNavigationEnabled(prefs.spatial_navigation_enabled);
  // Spatnav depends on KeyboardFocusableScrollers. The WebUI team has
  // disabled KFS because they need more time to update their custom elements,
  // crbug.com/907284. Meanwhile, we pre-ship KFS to spatnav users.
  if (prefs.spatial_navigation_enabled)
    WebRuntimeFeatures::EnableKeyboardFocusableScrollers(true);

  settings->SetSelectionIncludesAltImageText(true);

  settings->SetV8CacheOptions(prefs.v8_cache_options);

  settings->SetImageAnimationPolicy(prefs.animation_policy);

  settings->SetPresentationRequiresUserGesture(
      prefs.user_gesture_required_for_presentation);

  if (prefs.text_tracks_enabled) {
    settings->SetTextTrackKindUserPreference(
        WebSettings::TextTrackKindUserPreference::kCaptions);
  } else {
    settings->SetTextTrackKindUserPreference(
        WebSettings::TextTrackKindUserPreference::kDefault);
  }
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
  settings->SetTextTrackFontStyle(
      WebString::FromASCII(prefs.text_track_font_style));
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
  web_view_impl->SetIgnoreViewportTagScaleLimits(prefs.force_enable_zoom);
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

  settings->SetAccessibilityAlwaysShowFocus(prefs.always_show_focus);
  settings->SetAutoplayPolicy(prefs.autoplay_policy);
  settings->SetViewportEnabled(prefs.viewport_enabled);
  settings->SetViewportMetaEnabled(prefs.viewport_meta_enabled);
  settings->SetViewportStyle(prefs.viewport_style);

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
      GetVideoSurfaceLayerMode() !=
          blink::WebMediaPlayer::SurfaceLayerMode::kNever);

  settings->SetDataSaverHoldbackWebApi(
      prefs.data_saver_holdback_web_api_enabled);

  settings->SetLazyLoadEnabled(prefs.lazy_load_enabled);
  settings->SetPreferredColorScheme(prefs.preferred_color_scheme);

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

  settings->SetTouchDragDropEnabled(prefs.touch_drag_drop_enabled);
  settings->SetTouchDragEndContextMenu(prefs.touch_dragend_context_menu);

#if defined(OS_MAC)
  web_view_impl->SetMaximumLegibleScale(
      prefs.default_maximum_page_scale_factor);
#endif

#if defined(OS_WIN)
  WebRuntimeFeatures::EnableMiddleClickAutoscroll(true);
#endif

  WebRuntimeFeatures::EnableTranslateService(prefs.translate_service_available);
}

void WebViewImpl::ThemeChanged() {
  if (!GetPage())
    return;
  if (!GetPage()->MainFrame()->IsLocalFrame())
    return;
  LocalFrameView* view = GetPage()->DeprecatedLocalMainFrame()->View();

  WebRect damaged_rect(0, 0, size_.width(), size_.height());
  view->InvalidateRect(damaged_rect);
}

void WebViewImpl::EnterFullscreen(LocalFrame& frame,
                                  const FullscreenOptions* options,
                                  FullscreenRequestType request_type) {
  fullscreen_controller_->EnterFullscreen(frame, options, request_type);
}

void WebViewImpl::ExitFullscreen(LocalFrame& frame) {
  fullscreen_controller_->ExitFullscreen(frame);
}

void WebViewImpl::FullscreenElementChanged(Element* old_element,
                                           Element* new_element) {
  fullscreen_controller_->FullscreenElementChanged(old_element, new_element);
}

bool WebViewImpl::HasHorizontalScrollbar() {
  return MainFrameImpl()
      ->GetFrameView()
      ->LayoutViewport()
      ->HorizontalScrollbar();
}

bool WebViewImpl::HasVerticalScrollbar() {
  return MainFrameImpl()->GetFrameView()->LayoutViewport()->VerticalScrollbar();
}

WebInputEventResult WebViewImpl::DispatchBufferedTouchEvents() {
  if (!MainFrameImpl())
    return WebInputEventResult::kNotHandled;
  if (WebDevToolsAgentImpl* devtools = MainFrameDevToolsAgentImpl())
    devtools->DispatchBufferedTouchEvents();
  return MainFrameImpl()
      ->GetFrame()
      ->GetEventHandler()
      .DispatchBufferedTouchEvents();
}

WebInputEventResult WebViewImpl::HandleInputEvent(
    const WebCoalescedInputEvent& coalesced_event) {
  // TODO(bokan): Temporary debugging added to diagnose
  // https://crbug.com/992315. Somehow we're synchronously calling
  // WebViewImpl::Close while handling an input event.
  base::AutoReset<bool> inside_input_handling(&debug_inside_input_handling_,
                                              true);

  const WebInputEvent& input_event = coalesced_event.Event();
  // TODO(dcheng): The fact that this is getting called when there is no local
  // main frame is problematic and probably indicates a bug in the input event
  // routing code.
  if (!MainFrameImpl())
    return WebInputEventResult::kNotHandled;
  DCHECK(!WebInputEvent::IsTouchEventType(input_event.GetType()));

  WebFrameWidgetBase* widget = MainFrameImpl()->FrameWidgetImpl();
  DCHECK(widget);

  GetPage()->GetVisualViewport().StartTrackingPinchStats();

  TRACE_EVENT1("input,rail", "WebViewImpl::handleInputEvent", "type",
               WebInputEvent::GetName(input_event.GetType()));

  // If a drag-and-drop operation is in progress, ignore input events except
  // PointerCancel.
  if (widget->DoingDragAndDrop() &&
      input_event.GetType() != WebInputEvent::Type::kPointerCancel)
    return WebInputEventResult::kHandledSuppressed;

  if (WebDevToolsAgentImpl* devtools = MainFrameDevToolsAgentImpl()) {
    auto result = devtools->HandleInputEvent(input_event);
    if (result != WebInputEventResult::kNotHandled)
      return result;
  }

  // Report the event to be NOT processed by WebKit, so that the browser can
  // handle it appropriately.
  if (WebFrameWidgetBase::IgnoreInputEvents())
    return WebInputEventResult::kNotHandled;

  base::AutoReset<const WebInputEvent*> current_event_change(
      &CurrentInputEvent::current_input_event_, &input_event);
  UIEventWithKeyState::ClearNewTabModifierSetFromIsolatedWorld();

  if (GetPage()->GetPointerLockController().IsPointerLocked() &&
      WebInputEvent::IsMouseEventType(input_event.GetType())) {
    widget->PointerLockMouseEvent(coalesced_event);
    return WebInputEventResult::kHandledSystem;
  }

  Document& main_frame_document = *MainFrameImpl()->GetFrame()->GetDocument();

  if (input_event.GetType() != WebInputEvent::Type::kMouseMove) {
    FirstMeaningfulPaintDetector::From(main_frame_document).NotifyInputEvent();
  }

  if (input_event.GetType() != WebInputEvent::Type::kMouseMove &&
      input_event.GetType() != WebInputEvent::Type::kMouseEnter &&
      input_event.GetType() != WebInputEvent::Type::kMouseLeave) {
    InteractiveDetector* interactive_detector(
        InteractiveDetector::From(main_frame_document));
    if (interactive_detector) {
      interactive_detector->OnInvalidatingInputEvent(input_event.TimeStamp());
    }
  }

  widget->NotifyInputObservers(coalesced_event);

  // Notify the focus frame of the input. Note that the other frames are not
  // notified as input is only handled by the focused frame.
  Frame* frame = FocusedCoreFrame();
  if (auto* local_frame = DynamicTo<LocalFrame>(frame)) {
    if (auto* content_capture_manager =
            local_frame->LocalFrameRoot().GetContentCaptureManager()) {
      content_capture_manager->NotifyInputEvent(input_event.GetType(),
                                                *local_frame);
    }
  }

  // Skip the pointerrawupdate for mouse capture case.
  if (mouse_capture_element_ &&
      input_event.GetType() == WebInputEvent::Type::kPointerRawUpdate)
    return WebInputEventResult::kHandledSystem;

  if (mouse_capture_element_ &&
      WebInputEvent::IsMouseEventType(input_event.GetType()))
    return HandleCapturedMouseEvent(coalesced_event);

  // FIXME: This should take in the intended frame, not the local frame
  // root.
  return PageWidgetDelegate::HandleInputEvent(*this, coalesced_event,
                                              MainFrameImpl()->GetFrame());
}

WebInputEventResult WebViewImpl::HandleCapturedMouseEvent(
    const WebCoalescedInputEvent& coalesced_event) {
  const WebInputEvent& input_event = coalesced_event.Event();
  TRACE_EVENT1("input", "captured mouse event", "type", input_event.GetType());
  // Save |mouse_capture_element_| since |MouseCaptureLost()| will clear it.
  HTMLPlugInElement* element = mouse_capture_element_;

  // Not all platforms call mouseCaptureLost() directly.
  if (input_event.GetType() == WebInputEvent::Type::kMouseUp)
    MouseCaptureLost();

  AtomicString event_type;
  switch (input_event.GetType()) {
    case WebInputEvent::Type::kMouseEnter:
      event_type = event_type_names::kMouseover;
      break;
    case WebInputEvent::Type::kMouseMove:
      event_type = event_type_names::kMousemove;
      break;
    case WebInputEvent::Type::kPointerRawUpdate:
      // There will be no mouse event for rawupdate events.
      event_type = event_type_names::kPointerrawupdate;
      break;
    case WebInputEvent::Type::kMouseLeave:
      event_type = event_type_names::kMouseout;
      break;
    case WebInputEvent::Type::kMouseDown:
      event_type = event_type_names::kMousedown;
      LocalFrame::NotifyUserActivation(
          element->GetDocument().GetFrame(),
          mojom::blink::UserActivationNotificationType::kInteraction);
      break;
    case WebInputEvent::Type::kMouseUp:
      event_type = event_type_names::kMouseup;
      break;
    default:
      NOTREACHED();
  }

  WebMouseEvent transformed_event =
      TransformWebMouseEvent(MainFrameImpl()->GetFrameView(),
                             static_cast<const WebMouseEvent&>(input_event));
  if (LocalFrame* frame = element->GetDocument().GetFrame()) {
    frame->GetEventHandler().HandleTargetedMouseEvent(
        element, transformed_event, event_type,
        TransformWebMouseEventVector(
            MainFrameImpl()->GetFrameView(),
            coalesced_event.GetCoalescedEventsPointers()),
        TransformWebMouseEventVector(
            MainFrameImpl()->GetFrameView(),
            coalesced_event.GetPredictedEventsPointers()));
  }
  return WebInputEventResult::kHandledSystem;
}

void WebViewImpl::SetCursorVisibilityState(bool is_visible) {
  if (page_)
    page_->SetIsCursorVisible(is_visible);
}

void WebViewImpl::MouseCaptureLost() {
  TRACE_EVENT_NESTABLE_ASYNC_END0("input", "capturing mouse",
                                  TRACE_ID_LOCAL(this));
  mouse_capture_element_ = nullptr;
  if (page_->DeprecatedLocalMainFrame())
    page_->DeprecatedLocalMainFrame()->Client()->SetMouseCapture(false);
}

void WebViewImpl::SetFocus(bool enable) {
  if (enable)
    page_->GetFocusController().SetActive(true);
  page_->GetFocusController().SetFocused(enable);
  if (enable) {
    LocalFrame* focused_frame = page_->GetFocusController().FocusedFrame();
    if (focused_frame) {
      Element* element = focused_frame->GetDocument()->FocusedElement();
      if (element && focused_frame->Selection()
                         .ComputeVisibleSelectionInDOMTreeDeprecated()
                         .IsNone()) {
        // If the selection was cleared while the WebView was not
        // focused, then the focus element shows with a focus ring but
        // no caret and does respond to keyboard inputs.
        focused_frame->GetDocument()->UpdateStyleAndLayoutTree();
        if (element->IsTextControl()) {
          element->UpdateFocusAppearance(SelectionBehaviorOnFocus::kRestore);
        } else if (HasEditableStyle(*element)) {
          // updateFocusAppearance() selects all the text of
          // contentseditable DIVs. So we set the selection explicitly
          // instead. Note that this has the side effect of moving the
          // caret back to the beginning of the text.
          Position position(element, 0);
          focused_frame->Selection().SetSelectionAndEndTyping(
              SelectionInDOMTree::Builder().Collapse(position).Build());
        }
      }
    }
    ime_accept_events_ = true;
  } else {
    CancelPagePopup();

    // Clear focus on the currently focused frame if any.
    if (!page_)
      return;

    LocalFrame* frame = DynamicTo<LocalFrame>(page_->MainFrame());
    if (!frame)
      return;

    LocalFrame* focused_frame = FocusedLocalFrameInWidget();
    if (focused_frame) {
      // Finish an ongoing composition to delete the composition node.
      if (focused_frame->GetInputMethodController().HasComposition()) {
        // TODO(editing-dev): The use of
        // UpdateStyleAndLayout needs to be audited.
        // See http://crbug.com/590369 for more details.
        focused_frame->GetDocument()->UpdateStyleAndLayout(
            DocumentUpdateReason::kFocus);

        focused_frame->GetInputMethodController().FinishComposingText(
            InputMethodController::kKeepSelection);
      }
      ime_accept_events_ = false;
    }
  }
}

// WebView --------------------------------------------------------------------

WebSettingsImpl* WebViewImpl::SettingsImpl() {
  if (!web_settings_) {
    web_settings_ = std::make_unique<WebSettingsImpl>(
        &page_->GetSettings(), dev_tools_emulator_.Get());
  }
  DCHECK(web_settings_);
  return web_settings_.get();
}

WebSettings* WebViewImpl::GetSettings() {
  return SettingsImpl();
}

WebString WebViewImpl::PageEncoding() const {
  if (!page_)
    return WebString();

  auto* main_frame = DynamicTo<LocalFrame>(page_->MainFrame());
  if (!main_frame)
    return WebString();

  // FIXME: Is this check needed?
  if (!main_frame->GetDocument()->Loader())
    return WebString();

  return main_frame->GetDocument()->EncodingName();
}

WebFrame* WebViewImpl::MainFrame() {
  Page* page = page_.Get();
  return WebFrame::FromFrame(page ? page->MainFrame() : nullptr);
}

WebLocalFrameImpl* WebViewImpl::MainFrameImpl() const {
  Page* page = page_.Get();
  if (!page)
    return nullptr;
  return WebLocalFrameImpl::FromFrame(DynamicTo<LocalFrame>(page->MainFrame()));
}

void WebViewImpl::DidAttachLocalMainFrame() {
  DCHECK(MainFrameImpl());

  LocalFrame* local_frame = MainFrameImpl()->GetFrame();
  local_frame->WasAttachedAsLocalMainFrame();

  local_frame->GetRemoteNavigationAssociatedInterfaces()->GetInterface(
      local_main_frame_host_remote_.BindNewEndpointAndPassReceiver());

  if (does_composite_) {
    // When attaching a local main frame, set up any state on the compositor.
    MainFrameImpl()->FrameWidgetImpl()->SetBackgroundColor(BackgroundColor());
    auto& viewport = GetPage()->GetVisualViewport();
    MainFrameImpl()->FrameWidgetImpl()->SetPageScaleStateAndLimits(
        viewport.Scale(), viewport.IsPinchGestureActive(),
        MinimumPageScaleFactor(), MaximumPageScaleFactor());
    // Prevent main frame updates while the main frame is loading until enough
    // progress is made and BeginMainFrames are explicitly asked for.
    scoped_defer_main_frame_update_ =
        MainFrameImpl()->FrameWidgetImpl()->DeferMainFrameUpdate();
  }
}

void WebViewImpl::DidAttachRemoteMainFrame() {
  DCHECK(!MainFrameImpl());

  RemoteFrame* remote_frame = DynamicTo<RemoteFrame>(GetPage()->MainFrame());
  remote_frame->WasAttachedAsRemoteMainFrame();

  remote_frame->GetRemoteAssociatedInterfaces()->GetInterface(
      remote_main_frame_host_remote_.BindNewEndpointAndPassReceiver());
}

void WebViewImpl::DidDetachLocalMainFrame() {
  // The WebWidgetClient that generated the |scoped_defer_main_frame_update_|
  // for a local main frame is going away.
  scoped_defer_main_frame_update_ = nullptr;
  local_main_frame_host_remote_.reset();
}

void WebViewImpl::DidDetachRemoteMainFrame() {
  remote_main_frame_host_remote_.reset();
}

WebLocalFrame* WebViewImpl::FocusedFrame() {
  Frame* frame = FocusedCoreFrame();
  // TODO(yabinh): focusedCoreFrame() should always return a local frame, and
  // the following check should be unnecessary.
  // See crbug.com/625068
  return WebLocalFrameImpl::FromFrame(DynamicTo<LocalFrame>(frame));
}

void WebViewImpl::SetFocusedFrame(WebFrame* frame) {
  if (!frame) {
    // Clears the focused frame if any.
    Frame* focused_frame = FocusedCoreFrame();
    if (auto* focused_local_frame = DynamicTo<LocalFrame>(focused_frame))
      focused_local_frame->Selection().SetFrameIsFocused(false);
    return;
  }
  LocalFrame* core_frame = To<WebLocalFrameImpl>(frame)->GetFrame();
  core_frame->GetPage()->GetFocusController().SetFocusedFrame(core_frame);
}

// TODO(dglazkov): Remove and replace with Node:hasEditableStyle.
// http://crbug.com/612560
static bool IsElementEditable(const Element* element) {
  element->GetDocument().UpdateStyleAndLayoutTree();
  if (HasEditableStyle(*element))
    return true;

  if (auto* text_control = ToTextControlOrNull(element)) {
    if (!text_control->IsDisabledOrReadOnly())
      return true;
  }

  return EqualIgnoringASCIICase(
      element->FastGetAttribute(html_names::kRoleAttr), "textbox");
}

bool WebViewImpl::ScrollFocusedEditableElementIntoView() {
  DCHECK(MainFrameImpl());
  LocalFrameView* main_frame_view = MainFrameImpl()->GetFrame()->View();
  if (!main_frame_view)
    return false;

  Element* element = FocusedElement();
  if (!element || !IsElementEditable(element))
    return false;

  element->GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kSelection);

  LayoutObject* layout_object = element->GetLayoutObject();
  if (!layout_object)
    return false;

  // Since the page has been resized, the layout may have changed. The page
  // scale animation started by ZoomAndScrollToFocusedEditableRect will scroll
  // only the visual and layout viewports. We'll call ScrollRectToVisible with
  // the stop_at_main_frame_layout_viewport param to ensure the element is
  // actually visible in the page.
  auto params = ScrollAlignment::CreateScrollIntoViewParams(
      ScrollAlignment::CenterIfNeeded(), ScrollAlignment::CenterIfNeeded(),
      mojom::blink::ScrollType::kProgrammatic, false,
      mojom::blink::ScrollBehavior::kInstant);
  params->stop_at_main_frame_layout_viewport = true;
  layout_object->ScrollRectToVisible(
      PhysicalRect(layout_object->AbsoluteBoundingBoxRect()),
      std::move(params));

  ZoomAndScrollToFocusedEditableElementRect(
      main_frame_view->RootFrameToDocument(
          element->GetDocument().View()->ConvertToRootFrame(
              layout_object->AbsoluteBoundingBoxRect())),
      main_frame_view->RootFrameToDocument(
          element->GetDocument().View()->ConvertToRootFrame(
              element->GetDocument()
                  .GetFrame()
                  ->Selection()
                  .ComputeRectToScroll(kDoNotRevealExtent))),
      ShouldZoomToLegibleScale(*element));

  return true;
}

bool WebViewImpl::ShouldZoomToLegibleScale(const Element& element) {
  bool zoom_into_legible_scale =
      web_settings_->AutoZoomFocusedNodeToLegibleScale() &&
      !GetPage()->GetVisualViewport().ShouldDisableDesktopWorkarounds();

  if (zoom_into_legible_scale) {
    // When deciding whether to zoom in on a focused text box, we should
    // decide not to zoom in if the user won't be able to zoom out. e.g if the
    // textbox is within a touch-action: none container the user can't zoom
    // back out.
    TouchAction action =
        touch_action_util::ComputeEffectiveTouchAction(element);
    if (!(static_cast<int>(action) & static_cast<int>(TouchAction::kPinchZoom)))
      zoom_into_legible_scale = false;
  }

  return zoom_into_legible_scale;
}

void WebViewImpl::ZoomAndScrollToFocusedEditableElementRect(
    const IntRect& element_bounds_in_document,
    const IntRect& caret_bounds_in_document,
    bool zoom_into_legible_scale) {
  float scale;
  IntPoint scroll;
  bool need_animation = false;
  ComputeScaleAndScrollForEditableElementRects(
      element_bounds_in_document, caret_bounds_in_document,
      zoom_into_legible_scale, scale, scroll, need_animation);
  if (need_animation) {
    StartPageScaleAnimation(scroll, false, scale,
                            kScrollAndScaleAnimationDuration);
  }
}

void WebViewImpl::SmoothScroll(int target_x,
                               int target_y,
                               base::TimeDelta duration) {
  IntPoint target_position(target_x, target_y);
  StartPageScaleAnimation(target_position, false, PageScaleFactor(), duration);
}

void WebViewImpl::ComputeScaleAndScrollForEditableElementRects(
    const IntRect& element_bounds_in_document,
    const IntRect& caret_bounds_in_document,
    bool zoom_into_legible_scale,
    float& new_scale,
    IntPoint& new_scroll,
    bool& need_animation) {
  VisualViewport& visual_viewport = GetPage()->GetVisualViewport();

  TopDocumentRootScrollerController& controller =
      GetPage()->GlobalRootScrollerController();
  Node* root_scroller = controller.GlobalRootScroller();

  IntRect element_bounds_in_content = element_bounds_in_document;
  IntRect caret_bounds_in_content = caret_bounds_in_document;

  // If the page has a non-default root scroller then we need to scroll that
  // rather than the "real" viewport. However, the given coordinates are in the
  // real viewport's document space rather than the root scroller's so we
  // perform the conversion here.  TODO(bokan): Convert this function to take
  // coordinates in absolute/root-frame coordinates to make this more
  // consistent. https://crbug.com/931447.
  if (root_scroller != MainFrameImpl()->GetFrame()->GetDocument() &&
      controller.RootScrollerArea()) {
    ScrollOffset offset = controller.RootScrollerArea()->GetScrollOffset();
    element_bounds_in_content.Move(FlooredIntSize(offset));
    caret_bounds_in_content.Move(FlooredIntSize(offset));
  }

  if (!zoom_into_legible_scale) {
    new_scale = PageScaleFactor();
  } else {
    // Pick a scale which is reasonably readable. This is the scale at which
    // the caret height will become minReadableCaretHeightForNode (adjusted
    // for dpi and font scale factor).
    const int min_readable_caret_height_for_node =
        (element_bounds_in_content.Height() >=
                 2 * caret_bounds_in_content.Height()
             ? minReadableCaretHeightForTextArea
             : minReadableCaretHeight) *
        MainFrameImpl()->GetFrame()->PageZoomFactor();
    new_scale = ClampPageScaleFactorToLimits(
        MaximumLegiblePageScale() * min_readable_caret_height_for_node /
        caret_bounds_in_content.Height());
    new_scale = std::max(new_scale, PageScaleFactor());
  }
  const float delta_scale = new_scale / PageScaleFactor();

  need_animation = false;

  // If we are at less than the target zoom level, zoom in.
  if (delta_scale > minScaleChangeToTriggerZoom)
    need_animation = true;
  else
    new_scale = PageScaleFactor();

  ScrollableArea* root_viewport =
      MainFrameImpl()->GetFrame()->View()->GetScrollableArea();

  // If the caret is offscreen, then animate.
  if (!root_viewport->VisibleContentRect().Contains(caret_bounds_in_content))
    need_animation = true;

  // If the box is partially offscreen and it's possible to bring it fully
  // onscreen, then animate.
  if (visual_viewport.VisibleRect().Width() >=
          element_bounds_in_content.Width() &&
      visual_viewport.VisibleRect().Height() >=
          element_bounds_in_content.Height() &&
      !root_viewport->VisibleContentRect().Contains(element_bounds_in_content))
    need_animation = true;

  if (!need_animation)
    return;

  FloatSize target_viewport_size(visual_viewport.Size());
  target_viewport_size.Scale(1 / new_scale);

  if (element_bounds_in_content.Width() <= target_viewport_size.Width()) {
    // Field is narrower than screen. Try to leave padding on left so field's
    // label is visible, but it's more important to ensure entire field is
    // onscreen.
    int ideal_left_padding = target_viewport_size.Width() * leftBoxRatio;
    int max_left_padding_keeping_box_onscreen =
        target_viewport_size.Width() - element_bounds_in_content.Width();
    new_scroll.SetX(element_bounds_in_content.X() -
                    std::min<int>(ideal_left_padding,
                                  max_left_padding_keeping_box_onscreen));
  } else {
    // Field is wider than screen. Try to left-align field, unless caret would
    // be offscreen, in which case right-align the caret.
    new_scroll.SetX(std::max<int>(
        element_bounds_in_content.X(),
        caret_bounds_in_content.X() + caret_bounds_in_content.Width() +
            caretPadding - target_viewport_size.Width()));
  }
  if (element_bounds_in_content.Height() <= target_viewport_size.Height()) {
    // Field is shorter than screen. Vertically center it.
    new_scroll.SetY(
        element_bounds_in_content.Y() -
        (target_viewport_size.Height() - element_bounds_in_content.Height()) /
            2);
  } else {
    // Field is taller than screen. Try to top align field, unless caret would
    // be offscreen, in which case bottom-align the caret.
    new_scroll.SetY(std::max<int>(
        element_bounds_in_content.Y(),
        caret_bounds_in_content.Y() + caret_bounds_in_content.Height() +
            caretPadding - target_viewport_size.Height()));
  }
}

void WebViewImpl::AdvanceFocus(bool reverse) {
  GetPage()->GetFocusController().AdvanceFocus(
      reverse ? mojom::blink::FocusType::kBackward
              : mojom::blink::FocusType::kForward);
}

double WebViewImpl::ZoomLevel() {
  return zoom_level_;
}

void WebViewImpl::PropagateZoomFactorToLocalFrameRoots(Frame* frame,
                                                       float zoom_factor) {
  auto* local_frame = DynamicTo<LocalFrame>(frame);
  if (local_frame && local_frame->IsLocalRoot()) {
    if (Document* document = local_frame->GetDocument()) {
      auto* plugin_document = DynamicTo<PluginDocument>(document);
      if (!plugin_document || !plugin_document->GetPluginView()) {
        local_frame->SetPageZoomFactor(zoom_factor);
      }
    }
  }

  for (Frame* child = frame->Tree().FirstChild(); child;
       child = child->Tree().NextSibling())
    PropagateZoomFactorToLocalFrameRoots(child, zoom_factor);
}

double WebViewImpl::SetZoomLevel(double zoom_level) {
  double old_zoom_level = zoom_level_;
  if (zoom_level < minimum_zoom_level_)
    zoom_level_ = minimum_zoom_level_;
  else if (zoom_level > maximum_zoom_level_)
    zoom_level_ = maximum_zoom_level_;
  else
    zoom_level_ = zoom_level;

  float zoom_factor =
      zoom_factor_override_
          ? zoom_factor_override_
          : static_cast<float>(PageZoomLevelToZoomFactor(zoom_level_));
  if (zoom_factor_for_device_scale_factor_) {
    if (compositor_device_scale_factor_override_) {
      // Adjust the page's DSF so that DevicePixelRatio becomes
      // |zoom_factor_for_device_scale_factor_|.
      page_->SetDeviceScaleFactorDeprecated(
          zoom_factor_for_device_scale_factor_ /
          compositor_device_scale_factor_override_);
      zoom_factor *= compositor_device_scale_factor_override_;
    } else {
      page_->SetDeviceScaleFactorDeprecated(1.f);
      zoom_factor *= zoom_factor_for_device_scale_factor_;
    }
  }
  PropagateZoomFactorToLocalFrameRoots(page_->MainFrame(), zoom_factor);

  if (old_zoom_level != zoom_level_) {
    Client()->ZoomLevelChanged();
    CancelPagePopup();
  }

  return zoom_level_;
}

float WebViewImpl::PageScaleFactor() const {
  if (!GetPage())
    return 1;

  return GetPage()->GetVisualViewport().Scale();
}

float WebViewImpl::ClampPageScaleFactorToLimits(float scale_factor) const {
  return GetPageScaleConstraintsSet().FinalConstraints().ClampToConstraints(
      scale_factor);
}

void WebViewImpl::SetVisualViewportOffset(const gfx::PointF& offset) {
  DCHECK(GetPage());
  GetPage()->GetVisualViewport().SetLocation(FloatPoint(offset));
}

gfx::PointF WebViewImpl::VisualViewportOffset() const {
  DCHECK(GetPage());
  return GetPage()->GetVisualViewport().VisibleRect().Location();
}

gfx::SizeF WebViewImpl::VisualViewportSize() const {
  DCHECK(GetPage());
  return gfx::SizeF(GetPage()->GetVisualViewport().VisibleRect().Size());
}

void WebViewImpl::SetPageScaleFactorAndLocation(float scale_factor,
                                                bool is_pinch_gesture_active,
                                                const FloatPoint& location) {
  DCHECK(GetPage());

  GetPage()->GetVisualViewport().SetScaleAndLocation(
      ClampPageScaleFactorToLimits(scale_factor), is_pinch_gesture_active,
      location);
}

void WebViewImpl::SetPageScaleFactor(float scale_factor) {
  DCHECK(GetPage());
  DCHECK(MainFrameImpl());

  MainFrameImpl()->GetFrame()->SetScaleFactor(scale_factor);
}

void WebViewImpl::SetDeviceScaleFactor(float scale_factor) {
  if (!GetPage())
    return;

  if (GetPage()->DeviceScaleFactorDeprecated() == scale_factor)
    return;

  GetPage()->SetDeviceScaleFactorDeprecated(scale_factor);
}

void WebViewImpl::SetZoomFactorForDeviceScaleFactor(
    float zoom_factor_for_device_scale_factor) {
  DCHECK(does_composite_);
  // We can't early-return here if these are already equal, because we may
  // need to propagate the correct zoom factor to newly navigated frames.
  zoom_factor_for_device_scale_factor_ = zoom_factor_for_device_scale_factor;
  SetZoomLevel(zoom_level_);
}

void WebViewImpl::SetPageLifecycleStateFromNewPageCommit(
    mojom::blink::PageVisibilityState visibility,
    mojom::blink::PagehideDispatch pagehide_dispatch) {
  mojom::blink::PageLifecycleStatePtr state =
      GetPage()->GetPageLifecycleState().Clone();
  state->visibility = visibility;
  state->pagehide_dispatch = pagehide_dispatch;
  SetPageLifecycleStateInternal(std::move(state),
                                /*page_restore_params=*/nullptr);
}

void WebViewImpl::SetPageLifecycleState(
    mojom::blink::PageLifecycleStatePtr state,
    mojom::blink::PageRestoreParamsPtr page_restore_params,
    SetPageLifecycleStateCallback callback) {
  SetPageLifecycleStateInternal(std::move(state),
                                std::move(page_restore_params));
  // Tell the browser that the lifecycle update was successful.
  std::move(callback).Run();
}

void WebViewImpl::SetPageLifecycleStateInternal(
    mojom::blink::PageLifecycleStatePtr new_state,
    mojom::blink::PageRestoreParamsPtr page_restore_params) {
  Page* page = GetPage();
  if (!page)
    return;
  auto& old_state = page->GetPageLifecycleState();
  bool storing_in_bfcache = new_state->is_in_back_forward_cache &&
                            !old_state->is_in_back_forward_cache;
  bool restoring_from_bfcache = !new_state->is_in_back_forward_cache &&
                                old_state->is_in_back_forward_cache;
  bool hiding_page =
      (new_state->visibility != mojom::blink::PageVisibilityState::kVisible) &&
      (old_state->visibility == mojom::blink::PageVisibilityState::kVisible);
  bool showing_page =
      (new_state->visibility == mojom::blink::PageVisibilityState::kVisible) &&
      (old_state->visibility != mojom::blink::PageVisibilityState::kVisible);
  bool freezing_page = new_state->is_frozen && !old_state->is_frozen;
  bool resuming_page = !new_state->is_frozen && old_state->is_frozen;
  bool dispatching_pagehide =
      (new_state->pagehide_dispatch !=
       mojom::blink::PagehideDispatch::kNotDispatched) &&
      !GetPage()->DispatchedPagehideAndStillHidden();
  bool dispatching_pageshow =
      (new_state->pagehide_dispatch ==
       mojom::blink::PagehideDispatch::kNotDispatched) &&
      GetPage()->DispatchedPagehideAndStillHidden();

  if (dispatching_pagehide) {
    RemoveFocusAndTextInputState();
  }
  if (dispatching_pagehide) {
    // Note that |dispatching_pagehide| is different than |hiding_page|.
    // |dispatching_pagehide| will only be true when we're navigating away from
    // a page, while |hiding_page| might be true in other cases too such as when
    // the tab containing a page is backgrounded, and might be false even when
    // we're navigating away from a page, if the page is already hidden.
    DispatchPagehide(new_state->pagehide_dispatch);
  }
  if (hiding_page) {
    SetVisibilityState(new_state->visibility, /*is_initial_state=*/false);
  }
  if (storing_in_bfcache) {
    Scheduler()->SetPageBackForwardCached(new_state->is_in_back_forward_cache);
  }
  if (freezing_page)
    SetPageFrozen(true);
  if (storing_in_bfcache)
    HookBackForwardCacheEviction(true);
  if (restoring_from_bfcache) {
    DCHECK(page_restore_params);
    // Update the history offset and length value saved in RenderViewImpl, as
    // pages that are kept in the back-forward cache do not get notified about
    // updates on these values, so the currently saved value might be stale.
    web_view_client_->OnSetHistoryOffsetAndLength(
        page_restore_params->pending_history_list_offset,
        page_restore_params->current_history_list_length);
    HookBackForwardCacheEviction(false);
  }
  if (resuming_page)
    SetPageFrozen(false);
  if (showing_page) {
    SetVisibilityState(new_state->visibility, /*is_initial_state=*/false);
  }
  if (dispatching_pageshow) {
    DCHECK(restoring_from_bfcache);
    DispatchPageshow(page_restore_params->navigation_start);
  }
  if (restoring_from_bfcache) {
    DCHECK(dispatching_pageshow);
    DCHECK(page_restore_params);
    Scheduler()->SetPageBackForwardCached(new_state->is_in_back_forward_cache);
  }

  // Make sure no TrackedFeaturesUpdate message is sent after the ACK
  // TODO(carlscab): Do we really need to go through LocalFrame =>
  // platform/scheduler/ => LocalFrame to report the features? We can probably
  // move SchedulerTrackedFeatures to core/ and remove the back and forth.
  ReportActiveSchedulerTrackedFeatures();

  GetPage()->SetPageLifecycleState(std::move(new_state));
}

void WebViewImpl::ReportActiveSchedulerTrackedFeatures() {
  Page* page = GetPage();
  if (!page)
    return;

  for (Frame* frame = page->MainFrame(); frame;
       frame = frame->Tree().TraverseNext()) {
    if (!frame->IsLocalFrame())
      continue;
    auto* local_frame = DynamicTo<LocalFrame>(frame);
    if (!local_frame->GetFrameScheduler())
      continue;
    local_frame->GetFrameScheduler()->ReportActiveSchedulerTrackedFeatures();
  }
}

void WebViewImpl::AudioStateChanged(bool is_audio_playing) {
  GetPage()->GetPageScheduler()->AudioStateChanged(is_audio_playing);
}

void WebViewImpl::RemoveFocusAndTextInputState() {
  auto& focus_controller = GetPage()->GetFocusController();
  auto* focused_frame = focus_controller.FocusedFrame();
  if (!focused_frame)
    return;
  // Remove focus from the currently focused element and frame.
  focus_controller.SetFocusedElement(nullptr, nullptr);
  // Clear composing state, and make sure we send a TextInputState update.
  // Note that the TextInputState itself is cleared when we clear the focus,
  // but no updates to the browser will be triggered until the next animation
  // frame, which won't happen if we're freezing the page.
  if (auto* widget = static_cast<WebFrameWidgetBase*>(
          focused_frame->GetWidgetForLocalRoot())) {
    widget->FinishComposingText(false /* keep_selection */);
    widget->UpdateTextInputState();
  }
}

void WebViewImpl::DispatchPagehide(
    mojom::blink::PagehideDispatch pagehide_dispatch) {
  DCHECK_NE(pagehide_dispatch, mojom::blink::PagehideDispatch::kNotDispatched);
  bool persisted = (pagehide_dispatch ==
                    mojom::blink::PagehideDispatch::kDispatchedPersisted);
  // Dispatch pagehide on all frames.
  for (Frame* frame = GetPage()->MainFrame(); frame;
       frame = frame->Tree().TraverseNext()) {
    if (frame->DomWindow() && frame->DomWindow()->IsLocalDOMWindow()) {
      frame->DomWindow()->ToLocalDOMWindow()->DispatchPagehideEvent(
          persisted
              ? PageTransitionEventPersistence::kPageTransitionEventPersisted
              : PageTransitionEventPersistence::
                    kPageTransitionEventNotPersisted);
    }
  }
}

void WebViewImpl::DispatchPageshow(base::TimeTicks navigation_start) {
  for (Frame* frame = GetPage()->MainFrame(); frame;
       frame = frame->Tree().TraverseNext()) {
    auto* local_frame = DynamicTo<LocalFrame>(frame);
    // Record the metics.
    if (local_frame && local_frame->View()) {
      Document* document = local_frame->GetDocument();
      if (document) {
        PaintTiming::From(*document).OnRestoredFromBackForwardCache();
        InteractiveDetector::From(*document)->OnRestoredFromBackForwardCache();
      }
      DocumentLoader* loader = local_frame->Loader().GetDocumentLoader();
      if (loader) {
        loader->GetTiming().MarkBackForwardCacheRestoreNavigationStart(
            navigation_start);
      }
    }
    if (frame->DomWindow() && frame->DomWindow()->IsLocalDOMWindow()) {
      frame->DomWindow()->ToLocalDOMWindow()->DispatchPersistedPageshowEvent(
          navigation_start);
      if (frame->IsMainFrame()) {
        UMA_HISTOGRAM_BOOLEAN(
            "BackForwardCache.MainFrameHasPageshowListenersOnRestore",
            frame->DomWindow()->ToLocalDOMWindow()->HasEventListeners(
                event_type_names::kPageshow));
      }
    }
  }
}

void WebViewImpl::HookBackForwardCacheEviction(bool hook) {
  DCHECK(GetPage());
  for (Frame* frame = GetPage()->MainFrame(); frame;
       frame = frame->Tree().TraverseNext()) {
    auto* local_frame = DynamicTo<LocalFrame>(frame);
    if (!local_frame)
      continue;
    if (hook)
      local_frame->HookBackForwardCacheEviction();
    else
      local_frame->RemoveBackForwardCacheEviction();
  }
}

void WebViewImpl::EnableAutoResizeMode(const gfx::Size& min_size,
                                       const gfx::Size& max_size) {
  should_auto_resize_ = true;
  min_auto_size_ = IntSize(min_size);
  max_auto_size_ = IntSize(max_size);
  ConfigureAutoResizeMode();
}

void WebViewImpl::DisableAutoResizeMode() {
  should_auto_resize_ = false;
  ConfigureAutoResizeMode();
}

bool WebViewImpl::AutoResizeMode() {
  return should_auto_resize_;
}

void WebViewImpl::EnableAutoResizeForTesting(const gfx::Size& min_window_size,
                                             const gfx::Size& max_window_size) {
  EnableAutoResizeMode(web_widget_->DIPsToCeiledBlinkSpace(min_window_size),
                       web_widget_->DIPsToCeiledBlinkSpace(max_window_size));
}

void WebViewImpl::DisableAutoResizeForTesting(
    const gfx::Size& new_window_size) {
  if (!should_auto_resize_)
    return;
  DisableAutoResizeMode();

  // The |new_size| is empty when resetting auto resize in between tests. In
  // this case the current size should just be preserved.
  if (!new_window_size.IsEmpty()) {
    web_widget_->Resize(web_widget_->DIPsToCeiledBlinkSpace(new_window_size));
  }
}

void WebViewImpl::SetDefaultPageScaleLimits(float min_scale, float max_scale) {
  GetPage()->SetDefaultPageScaleLimits(min_scale, max_scale);
}

void WebViewImpl::SetInitialPageScaleOverride(
    float initial_page_scale_factor_override) {
  PageScaleConstraints constraints =
      GetPageScaleConstraintsSet().UserAgentConstraints();
  constraints.initial_scale = initial_page_scale_factor_override;

  if (constraints == GetPageScaleConstraintsSet().UserAgentConstraints())
    return;

  GetPageScaleConstraintsSet().SetNeedsReset(true);
  GetPage()->SetUserAgentPageScaleConstraints(constraints);
}

void WebViewImpl::SetMaximumLegibleScale(float maximum_legible_scale) {
  maximum_legible_scale_ = maximum_legible_scale;
}

void WebViewImpl::SetIgnoreViewportTagScaleLimits(bool ignore) {
  PageScaleConstraints constraints =
      GetPageScaleConstraintsSet().UserAgentConstraints();
  if (ignore) {
    // Don't ignore the minimum limits in touchless mode to prevent wide
    // loading elements from causing us to zoom pages out beyond their layout
    // which is fairly common.
    if (!RuntimeEnabledFeatures::FocuslessSpatialNavigationEnabled()) {
      constraints.minimum_scale =
          GetPageScaleConstraintsSet().DefaultConstraints().minimum_scale;
    }
    constraints.maximum_scale =
        GetPageScaleConstraintsSet().DefaultConstraints().maximum_scale;
  } else {
    if (!RuntimeEnabledFeatures::FocuslessSpatialNavigationEnabled()) {
      constraints.minimum_scale = -1;
    }
    constraints.maximum_scale = -1;
  }
  GetPage()->SetUserAgentPageScaleConstraints(constraints);
}

IntSize WebViewImpl::MainFrameSize() {
  // The frame size should match the viewport size at minimum scale, since the
  // viewport must always be contained by the frame.
  return IntSize(gfx::ScaleToCeiledSize(size_, 1 / MinimumPageScaleFactor()));
}

PageScaleConstraintsSet& WebViewImpl::GetPageScaleConstraintsSet() const {
  return GetPage()->GetPageScaleConstraintsSet();
}

void WebViewImpl::RefreshPageScaleFactor() {
  if (!MainFrame() || !GetPage() || !GetPage()->MainFrame() ||
      !GetPage()->MainFrame()->IsLocalFrame() ||
      !GetPage()->DeprecatedLocalMainFrame()->View())
    return;
  UpdatePageDefinedViewportConstraints(MainFrameImpl()
                                           ->GetFrame()
                                           ->GetDocument()
                                           ->GetViewportData()
                                           .GetViewportDescription());
  GetPageScaleConstraintsSet().ComputeFinalConstraints();

  float new_page_scale_factor = PageScaleFactor();
  if (GetPageScaleConstraintsSet().NeedsReset() &&
      GetPageScaleConstraintsSet().FinalConstraints().initial_scale != -1) {
    new_page_scale_factor =
        GetPageScaleConstraintsSet().FinalConstraints().initial_scale;
    GetPageScaleConstraintsSet().SetNeedsReset(false);
  }
  SetPageScaleFactor(new_page_scale_factor);

  // The constraints may have changed above which affects the page scale limits,
  // so we must update those even though SetPageScaleFactor() may do the same if
  // the scale factor is changed.
  if (does_composite_) {
    auto& viewport = GetPage()->GetVisualViewport();
    MainFrameImpl()->FrameWidgetImpl()->SetPageScaleStateAndLimits(
        viewport.Scale(), viewport.IsPinchGestureActive(),
        MinimumPageScaleFactor(), MaximumPageScaleFactor());
  }
}

void WebViewImpl::UpdatePageDefinedViewportConstraints(
    const ViewportDescription& description) {
  if (!GetPage() || (!size_.width() && !size_.height()))
    return;
  // The viewport is a property of the main frame and its widget, so ignore it
  // when the main frame is remote.
  // TODO(danakj): Remove calls to this method from ChromeClient and DCHECK this
  // instead.
  if (!GetPage()->MainFrame()->IsLocalFrame())
    return;

  if (!GetSettings()->ViewportEnabled()) {
    GetPageScaleConstraintsSet().ClearPageDefinedConstraints();
    UpdateMainFrameLayoutSize();
    return;
  }

  Document* document = GetPage()->DeprecatedLocalMainFrame()->GetDocument();

  Length default_min_width =
      document->GetViewportData().ViewportDefaultMinWidth();
  if (default_min_width.IsAuto())
    default_min_width = Length::ExtendToZoom();

  float old_initial_scale =
      GetPageScaleConstraintsSet().PageDefinedConstraints().initial_scale;
  GetPageScaleConstraintsSet().UpdatePageDefinedConstraints(description,
                                                            default_min_width);

  if (SettingsImpl()->ClobberUserAgentInitialScaleQuirk() &&
      GetPageScaleConstraintsSet().UserAgentConstraints().initial_scale != -1 &&
      GetPageScaleConstraintsSet().UserAgentConstraints().initial_scale *
              DeviceScaleFactor() <=
          1) {
    if (description.max_width == Length::DeviceWidth() ||
        (description.max_width.IsAuto() &&
         GetPageScaleConstraintsSet().PageDefinedConstraints().initial_scale ==
             1.0f))
      SetInitialPageScaleOverride(-1);
  }

  Settings& page_settings = GetPage()->GetSettings();
  GetPageScaleConstraintsSet().AdjustForAndroidWebViewQuirks(
      description, default_min_width.IntValue(), DeviceScaleFactor(),
      SettingsImpl()->SupportDeprecatedTargetDensityDPI(),
      page_settings.GetWideViewportQuirkEnabled(),
      page_settings.GetUseWideViewport(),
      page_settings.GetLoadWithOverviewMode(),
      SettingsImpl()->ViewportMetaNonUserScalableQuirk());
  float new_initial_scale =
      GetPageScaleConstraintsSet().PageDefinedConstraints().initial_scale;
  if (old_initial_scale != new_initial_scale && new_initial_scale != -1) {
    GetPageScaleConstraintsSet().SetNeedsReset(true);
    if (MainFrameImpl() && MainFrameImpl()->GetFrameView())
      MainFrameImpl()->GetFrameView()->SetNeedsLayout();
  }

  UpdateMainFrameLayoutSize();
}

void WebViewImpl::UpdateMainFrameLayoutSize() {
  if (should_auto_resize_ || !MainFrameImpl())
    return;

  LocalFrameView* view = MainFrameImpl()->GetFrameView();
  if (!view)
    return;

  gfx::Size layout_size = size_;

  if (GetSettings()->ViewportEnabled())
    layout_size = gfx::Size(GetPageScaleConstraintsSet().GetLayoutSize());

  if (GetPage()->GetSettings().GetForceZeroLayoutHeight())
    layout_size.set_height(0);

  view->SetLayoutSize(IntSize(layout_size));
}

IntSize WebViewImpl::ContentsSize() const {
  if (!GetPage()->MainFrame()->IsLocalFrame())
    return IntSize();
  auto* layout_view =
      GetPage()->DeprecatedLocalMainFrame()->ContentLayoutObject();
  if (!layout_view)
    return IntSize();
  return PixelSnappedIntRect(layout_view->DocumentRect()).Size();
}

gfx::Size WebViewImpl::ContentsPreferredMinimumSize() {
  DCHECK(page_->MainFrame()->IsLocalFrame());

  auto* main_local_frame = DynamicTo<LocalFrame>(page_->MainFrame());
  Document* document = main_local_frame->GetDocument();
  if (!document || !document->GetLayoutView() || !document->documentElement() ||
      !document->documentElement()->GetLayoutBox())
    return gfx::Size();

  // The preferred size requires an up-to-date layout tree.
  DCHECK(!document->NeedsLayoutTreeUpdate() &&
         !document->View()->NeedsLayout());

  // Needed for computing MinPreferredWidth.
  FontCachePurgePreventer fontCachePurgePreventer;
  int width_scaled = document->GetLayoutView()
                         ->PreferredLogicalWidths()
                         .min_size.Round();  // Already accounts for zoom.
  int height_scaled =
      document->documentElement()->GetLayoutBox()->ScrollHeight().Round();
  return gfx::Size(width_scaled, height_scaled);
}

void WebViewImpl::UpdatePreferredSize() {
  // We don't always want to send the change messages over IPC, only if we've
  // been put in that mode by getting a |ViewMsg_EnablePreferredSizeChangedMode|
  // message.
  if (!send_preferred_size_changes_ || !MainFrameImpl())
    return;

  if (!needs_preferred_size_update_)
    return;
  needs_preferred_size_update_ = false;

  gfx::Size size_in_dips =
      MainFrameImpl()->LocalRootFrameWidget()->BlinkSpaceToFlooredDIPs(
          gfx::Size(ContentsPreferredMinimumSize()));

  if (size_in_dips != preferred_size_in_dips_) {
    preferred_size_in_dips_ = size_in_dips;
    local_main_frame_host_remote_->ContentsPreferredSizeChanged(size_in_dips);
  }
}

void WebViewImpl::EnablePreferredSizeChangedMode() {
  if (send_preferred_size_changes_)
    return;
  send_preferred_size_changes_ = true;
  needs_preferred_size_update_ = true;

  // We need to ensure |UpdatePreferredSize| gets called. If a layout is needed,
  // force an update here which will call |DidUpdateMainFrameLayout|.
  if (MainFrameWidget()) {
    MainFrameWidget()->UpdateLifecycle(WebLifecycleUpdate::kLayout,
                                       DocumentUpdateReason::kSizeChange);
  }

  // If a layout was not needed, |DidUpdateMainFrameLayout| will not be called.
  // We explicitly update the preferred size here to ensure the preferred size
  // notification is sent.
  UpdatePreferredSize();
}

void WebViewImpl::Focus() {
  if (GetPage()->MainFrame()->IsLocalFrame()) {
    DCHECK(local_main_frame_host_remote_);
    local_main_frame_host_remote_->FocusPage();
  } else {
    DCHECK(remote_main_frame_host_remote_);
    remote_main_frame_host_remote_->FocusPage();
  }
}

void WebViewImpl::TakeFocus(bool reverse) {
  if (GetPage()->MainFrame()->IsLocalFrame()) {
    DCHECK(local_main_frame_host_remote_);
    local_main_frame_host_remote_->TakeFocus(reverse);
  } else {
    DCHECK(remote_main_frame_host_remote_);
    remote_main_frame_host_remote_->TakeFocus(reverse);
  }
}

void WebViewImpl::UpdateTargetURL(const WebURL& url,
                                  const WebURL& fallback_url) {
  KURL latest_url = KURL(url.IsEmpty() ? fallback_url : url);
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
    if (latest_url.GetString().length() > url::kMaxURLChars)
      latest_url = KURL();
    SendUpdatedTargetURLToBrowser(latest_url);
    target_url_ = latest_url;
    target_url_status_ = TARGET_INFLIGHT;
  }
}

void WebViewImpl::SendUpdatedTargetURLToBrowser(const KURL& target_url) {
  // Note: WTF::Unretained() usage below is safe, since `this` owns both
  // `mojo::Remote` objects.
  if (GetPage()->MainFrame()->IsLocalFrame()) {
    DCHECK(local_main_frame_host_remote_);
    local_main_frame_host_remote_->UpdateTargetURL(
        target_url, WTF::Bind(&WebViewImpl::TargetURLUpdatedInBrowser,
                              WTF::Unretained(this)));
  } else {
    DCHECK(remote_main_frame_host_remote_);
    remote_main_frame_host_remote_->UpdateTargetURL(
        target_url, WTF::Bind(&WebViewImpl::TargetURLUpdatedInBrowser,
                              WTF::Unretained(this)));
  }
}

void WebViewImpl::TargetURLUpdatedInBrowser() {
  // Check if there is a targeturl waiting to be sent.
  if (target_url_status_ == TARGET_PENDING)
    SendUpdatedTargetURLToBrowser(pending_target_url_);

  target_url_status_ = TARGET_NONE;
}

float WebViewImpl::DefaultMinimumPageScaleFactor() const {
  return GetPageScaleConstraintsSet().DefaultConstraints().minimum_scale;
}

float WebViewImpl::DefaultMaximumPageScaleFactor() const {
  return GetPageScaleConstraintsSet().DefaultConstraints().maximum_scale;
}

float WebViewImpl::MinimumPageScaleFactor() const {
  return GetPageScaleConstraintsSet().FinalConstraints().minimum_scale;
}

float WebViewImpl::MaximumPageScaleFactor() const {
  return GetPageScaleConstraintsSet().FinalConstraints().maximum_scale;
}

void WebViewImpl::ResetScaleStateImmediately() {
  GetPageScaleConstraintsSet().SetNeedsReset(true);
}

void WebViewImpl::ResetScrollAndScaleState() {
  GetPage()->GetVisualViewport().Reset();

  auto* main_local_frame = DynamicTo<LocalFrame>(GetPage()->MainFrame());
  if (!main_local_frame)
    return;

  if (LocalFrameView* frame_view = main_local_frame->View()) {
    ScrollableArea* scrollable_area = frame_view->LayoutViewport();

    if (!scrollable_area->GetScrollOffset().IsZero()) {
      scrollable_area->SetScrollOffset(ScrollOffset(),
                                       mojom::blink::ScrollType::kProgrammatic);
    }
  }

  if (Document* document = main_local_frame->GetDocument()) {
    if (DocumentLoader* loader = document->Loader()) {
      if (HistoryItem* item = loader->GetHistoryItem())
        item->ClearViewState();
    }
  }

  GetPageScaleConstraintsSet().SetNeedsReset(true);
}

WebHitTestResult WebViewImpl::HitTestResultAt(const gfx::PointF& point) {
  return CoreHitTestResultAt(point);
}

HitTestResult WebViewImpl::CoreHitTestResultAt(
    const gfx::PointF& point_in_viewport) {
  // TODO(crbug.com/843128): When we do async hit-testing, we might try to do
  // hit-testing when the local main frame is not valid anymore. Look into if we
  // can avoid getting here earlier in the pipeline.
  if (!MainFrameImpl() || !MainFrameImpl()->GetFrameView())
    return HitTestResult();

  DocumentLifecycle::AllowThrottlingScope throttling_scope(
      MainFrameImpl()->GetFrame()->GetDocument()->Lifecycle());
  LocalFrameView* view = MainFrameImpl()->GetFrameView();
  FloatPoint point_in_root_frame =
      view->ViewportToFrame(FloatPoint(point_in_viewport));
  return HitTestResultForRootFramePos(point_in_root_frame);
}

void WebViewImpl::SendResizeEventForMainFrame() {
  // FIXME: This is wrong. The LocalFrameView is responsible sending a
  // resizeEvent as part of layout. Layout is also responsible for sending
  // invalidations to the embedder. This method and all callers may be wrong. --
  // eseidel.
  if (MainFrameImpl()->GetFrameView()) {
    // Enqueues the resize event.
    MainFrameImpl()->GetFrame()->GetDocument()->EnqueueResizeEvent();
  }

  // A resized main frame can change the page scale limits.
  if (does_composite_) {
    auto& viewport = GetPage()->GetVisualViewport();
    MainFrameImpl()->FrameWidgetImpl()->SetPageScaleStateAndLimits(
        viewport.Scale(), viewport.IsPinchGestureActive(),
        MinimumPageScaleFactor(), MaximumPageScaleFactor());
  }
}

void WebViewImpl::ConfigureAutoResizeMode() {
  if (!MainFrameImpl() || !MainFrameImpl()->GetFrame() ||
      !MainFrameImpl()->GetFrame()->View())
    return;

  if (should_auto_resize_) {
    MainFrameImpl()->GetFrame()->View()->EnableAutoSizeMode(min_auto_size_,
                                                            max_auto_size_);
  } else {
    MainFrameImpl()->GetFrame()->View()->DisableAutoSizeMode();
  }
}

void WebViewImpl::SetCompositorDeviceScaleFactorOverride(
    float device_scale_factor) {
  if (compositor_device_scale_factor_override_ == device_scale_factor)
    return;
  compositor_device_scale_factor_override_ = device_scale_factor;
  if (zoom_factor_for_device_scale_factor_) {
    SetZoomLevel(ZoomLevel());
    return;
  }
}

void WebViewImpl::SetDeviceEmulationTransform(
    const TransformationMatrix& transform) {
  if (transform == device_emulation_transform_)
    return;
  device_emulation_transform_ = transform;
  UpdateDeviceEmulationTransform();
}

TransformationMatrix WebViewImpl::GetDeviceEmulationTransform() const {
  return device_emulation_transform_;
}

void WebViewImpl::EnableDeviceEmulation(const DeviceEmulationParams& params) {
  web_widget_->EnableDeviceEmulation(params);
}

void WebViewImpl::ActivateDevToolsTransform(
    const DeviceEmulationParams& params) {
  TransformationMatrix device_emulation_transform =
      dev_tools_emulator_->EnableDeviceEmulation(params);
  SetDeviceEmulationTransform(device_emulation_transform);
}

void WebViewImpl::DisableDeviceEmulation() {
  web_widget_->DisableDeviceEmulation();
}

void WebViewImpl::DeactivateDevToolsTransform() {
  dev_tools_emulator_->DisableDeviceEmulation();
  SetDeviceEmulationTransform(TransformationMatrix());
}

void WebViewImpl::PerformCustomContextMenuAction(unsigned action) {
  if (page_) {
    page_->GetContextMenuController().CustomContextMenuItemSelected(action);
  }
}

void WebViewImpl::DidCloseContextMenu() {
  LocalFrame* frame = page_->GetFocusController().FocusedFrame();
  if (frame)
    frame->Selection().SetCaretBlinkingSuspended(false);
}

WebInputMethodController* WebViewImpl::GetActiveWebInputMethodController()
    const {
  WebLocalFrameImpl* local_frame =
      WebLocalFrameImpl::FromFrame(FocusedLocalFrameInWidget());
  return local_frame ? local_frame->GetInputMethodController() : nullptr;
}

SkColor WebViewImpl::BackgroundColor() const {
  if (background_color_override_enabled_)
    return background_color_override_;
  Page* page = page_.Get();
  if (!page)
    return BaseBackgroundColor().Rgb();
  if (auto* main_local_frame = DynamicTo<LocalFrame>(page->MainFrame())) {
    LocalFrameView* view = main_local_frame->View();
    if (view)
      return view->DocumentBackgroundColor().Rgb();
  }
  return BaseBackgroundColor().Rgb();
}

Color WebViewImpl::BaseBackgroundColor() const {
  return base_background_color_override_enabled_
             ? base_background_color_override_
             : base_background_color_;
}

void WebViewImpl::SetBaseBackgroundColor(SkColor color) {
  if (base_background_color_ == color)
    return;

  base_background_color_ = color;
  UpdateBaseBackgroundColor();
}

void WebViewImpl::SetBaseBackgroundColorOverride(SkColor color) {
  if (base_background_color_override_enabled_ &&
      base_background_color_override_ == color) {
    return;
  }

  base_background_color_override_enabled_ = true;
  base_background_color_override_ = color;
  if (MainFrameImpl()) {
    // Force lifecycle update to ensure we're good to call
    // LocalFrameView::setBaseBackgroundColor().
    MainFrameImpl()
        ->GetFrame()
        ->View()
        ->UpdateLifecycleToCompositingCleanPlusScrolling(
            DocumentUpdateReason::kBaseColor);
  }
  UpdateBaseBackgroundColor();
}

void WebViewImpl::ClearBaseBackgroundColorOverride() {
  if (!base_background_color_override_enabled_)
    return;

  base_background_color_override_enabled_ = false;
  if (MainFrameImpl()) {
    // Force lifecycle update to ensure we're good to call
    // LocalFrameView::setBaseBackgroundColor().
    MainFrameImpl()
        ->GetFrame()
        ->View()
        ->UpdateLifecycleToCompositingCleanPlusScrolling(
            DocumentUpdateReason::kBaseColor);
  }
  UpdateBaseBackgroundColor();
}

void WebViewImpl::UpdateBaseBackgroundColor() {
  Color color = BaseBackgroundColor();
  if (auto* local_frame = DynamicTo<LocalFrame>(page_->MainFrame())) {
    LocalFrameView* view = local_frame->View();
    view->UpdateBaseBackgroundColorRecursively(color);
  }
}

void WebViewImpl::SetInsidePortal(bool inside_portal) {
  GetPage()->SetInsidePortal(inside_portal);

  // We may not have created the frame widget yet but that's ok because it'll
  // be created with this value correctly initialized. This can also be null if
  // the main frame is remote.
  if (web_widget_)
    web_widget_->SetIsNestedMainFrameWidget(inside_portal);
}

void WebViewImpl::SetWebPreferences(
    const web_pref::WebPreferences& preferences) {
  UpdateWebPreferences(preferences);
}

const web_pref::WebPreferences& WebViewImpl::GetWebPreferences() {
  return web_preferences_;
}

void WebViewImpl::UpdateWebPreferences(
    const blink::web_pref::WebPreferences& preferences) {
  web_preferences_ = preferences;
  ApplyWebPreferences(preferences, this);
  ApplyCommandLineToSettings(SettingsImpl());
}

void WebViewImpl::SetIsActive(bool active) {
  if (GetPage())
    GetPage()->GetFocusController().SetActive(active);
}

bool WebViewImpl::IsActive() const {
  return GetPage() ? GetPage()->GetFocusController().IsActive() : false;
}

void WebViewImpl::SetWindowFeatures(const WebWindowFeatures& features) {
  page_->SetWindowFeatures(features);
}

void WebViewImpl::SetOpenedByDOM() {
  page_->SetOpenedByDOM();
}

void WebViewImpl::DidCommitLoad(bool is_new_navigation,
                                bool is_navigation_within_page) {
  if (!is_navigation_within_page) {
    should_dispatch_first_visually_non_empty_layout_ = true;
    should_dispatch_first_layout_after_finished_parsing_ = true;
    should_dispatch_first_layout_after_finished_loading_ = true;

    if (is_new_navigation)
      GetPageScaleConstraintsSet().SetNeedsReset(true);
  }

  // Give the visual viewport's scroll layer its initial size.
  GetPage()->GetVisualViewport().MainFrameDidChangeSize();
}

void WebViewImpl::ResizeAfterLayout() {
  DCHECK(MainFrameImpl());

  if (!web_view_client_ || !web_view_client_->CanUpdateLayout())
    return;

  if (should_auto_resize_) {
    LocalFrameView* view = MainFrameImpl()->GetFrame()->View();
    gfx::Size frame_size = gfx::Size(view->Size());
    if (frame_size != size_) {
      size_ = frame_size;

      GetPage()->GetVisualViewport().SetSize(IntSize(size_));
      GetPageScaleConstraintsSet().DidChangeInitialContainingBlockSize(
          IntSize(size_));
      view->SetInitialViewportSize(IntSize(size_));

      web_view_client_->DidAutoResize(size_);
      web_widget_->DidAutoResize(size_);
      SendResizeEventForMainFrame();
    }
  }

  if (does_composite_ && GetPageScaleConstraintsSet().ConstraintsDirty())
    RefreshPageScaleFactor();

  resize_viewport_anchor_->ResizeFrameView(MainFrameSize());
}

void WebViewImpl::MainFrameLayoutUpdated() {
  DCHECK(MainFrameImpl());
  if (!web_view_client_)
    return;

  web_view_client_->DidUpdateMainFrameLayout();
  needs_preferred_size_update_ = true;
}

void WebViewImpl::DidChangeContentsSize() {
  auto* local_frame = DynamicTo<LocalFrame>(GetPage()->MainFrame());
  if (!local_frame)
    return;

  LocalFrameView* view = local_frame->View();

  int vertical_scrollbar_width = 0;
  if (view && view->LayoutViewport()) {
    Scrollbar* vertical_scrollbar = view->LayoutViewport()->VerticalScrollbar();
    if (vertical_scrollbar && !vertical_scrollbar->IsOverlayScrollbar())
      vertical_scrollbar_width = vertical_scrollbar->Width();
  }

  GetPageScaleConstraintsSet().DidChangeContentsSize(
      ContentsSize(), vertical_scrollbar_width, PageScaleFactor());
}

void WebViewImpl::PageScaleFactorChanged() {
  // This is called from the VisualViewport which only is used to control the
  // page scale/scroll viewport for a local main frame, and only when
  // compositing as PageScaleFactor doesn't exist otherwise.
  DCHECK(MainFrameImpl());
  DCHECK(does_composite_);

  GetPageScaleConstraintsSet().SetNeedsReset(false);
  // Set up the compositor and inform the browser of the PageScaleFactor,
  // which is tracked per-view.
  auto& viewport = GetPage()->GetVisualViewport();
  MainFrameImpl()->FrameWidgetImpl()->SetPageScaleStateAndLimits(
      viewport.Scale(), viewport.IsPinchGestureActive(),
      MinimumPageScaleFactor(), MaximumPageScaleFactor());

  local_main_frame_host_remote_->ScaleFactorChanged(viewport.Scale());

  if (dev_tools_emulator_->HasViewportOverride()) {
    TransformationMatrix device_emulation_transform =
        dev_tools_emulator_->MainFrameScrollOrScaleChanged();
    SetDeviceEmulationTransform(device_emulation_transform);
  }
}

void WebViewImpl::MainFrameScrollOffsetChanged() {
  DCHECK(MainFrameImpl());
  if (dev_tools_emulator_->HasViewportOverride()) {
    TransformationMatrix device_emulation_transform =
        dev_tools_emulator_->MainFrameScrollOrScaleChanged();
    SetDeviceEmulationTransform(device_emulation_transform);
  }
}

void WebViewImpl::TextAutosizerPageInfoChanged(
    const mojom::blink::TextAutosizerPageInfo& page_info) {
  DCHECK(MainFrameImpl());
  local_main_frame_host_remote_->TextAutosizerPageInfoChanged(
      page_info.Clone());
}

void WebViewImpl::SetBackgroundColorOverride(SkColor color) {
  DCHECK(does_composite_);

  background_color_override_enabled_ = true;
  background_color_override_ = color;
  if (MainFrameImpl()) {
    MainFrameImpl()->FrameWidgetImpl()->SetBackgroundColor(BackgroundColor());
  }
}

void WebViewImpl::ClearBackgroundColorOverride() {
  DCHECK(does_composite_);

  background_color_override_enabled_ = false;
  if (MainFrameImpl()) {
    MainFrameImpl()->FrameWidgetImpl()->SetBackgroundColor(BackgroundColor());
  }
}

void WebViewImpl::SetZoomFactorOverride(float zoom_factor) {
  zoom_factor_override_ = zoom_factor;
  SetZoomLevel(ZoomLevel());
}

Element* WebViewImpl::FocusedElement() const {
  LocalFrame* frame = page_->GetFocusController().FocusedFrame();
  if (!frame)
    return nullptr;

  Document* document = frame->GetDocument();
  if (!document)
    return nullptr;

  return document->FocusedElement();
}

HitTestResult WebViewImpl::HitTestResultForRootFramePos(
    const FloatPoint& pos_in_root_frame) {
  auto* main_frame = DynamicTo<LocalFrame>(page_->MainFrame());
  if (!main_frame)
    return HitTestResult();
  HitTestLocation location(
      main_frame->View()->ConvertFromRootFrame(pos_in_root_frame));
  HitTestResult result = main_frame->GetEventHandler().HitTestResultAtLocation(
      location, HitTestRequest::kReadOnly | HitTestRequest::kActive);
  result.SetToShadowHostIfInRestrictedShadowRoot();
  return result;
}

WebHitTestResult WebViewImpl::HitTestResultForTap(
    const gfx::Point& tap_point_window_pos,
    const gfx::Size& tap_area) {
  auto* main_frame = DynamicTo<LocalFrame>(page_->MainFrame());
  if (!main_frame)
    return HitTestResult();

  WebGestureEvent tap_event(WebInputEvent::Type::kGestureTap,
                            WebInputEvent::kNoModifiers, base::TimeTicks::Now(),
                            WebGestureDevice::kTouchscreen);
  // GestureTap is only ever from a touchscreen.
  tap_event.SetPositionInWidget(FloatPoint(IntPoint(tap_point_window_pos)));
  tap_event.data.tap.tap_count = 1;
  tap_event.data.tap.width = tap_area.width();
  tap_event.data.tap.height = tap_area.height();

  WebGestureEvent scaled_event =
      TransformWebGestureEvent(MainFrameImpl()->GetFrameView(), tap_event);

  HitTestResult result =
      main_frame->GetEventHandler()
          .HitTestResultForGestureEvent(
              scaled_event, HitTestRequest::kReadOnly | HitTestRequest::kActive)
          .GetHitTestResult();

  result.SetToShadowHostIfInRestrictedShadowRoot();
  return result;
}

void WebViewImpl::SetTabsToLinks(bool enable) {
  tabs_to_links_ = enable;
}

bool WebViewImpl::TabsToLinks() const {
  return tabs_to_links_;
}

void WebViewImpl::DidChangeRootLayer(bool root_layer_exists) {
  if (!MainFrameImpl()) {
    DCHECK(!root_layer_exists);
    return;
  }
  if (root_layer_exists) {
    UpdateDeviceEmulationTransform();
  } else {
    // When the document in an already-attached main frame is being replaced by
    // a navigation then DidChangeRootLayer(false) will be called. Since we are
    // navigating, defer BeginMainFrames until the new document is ready for
    // them.
    //
    // TODO(crbug.com/936696): This should not be needed once we always swap
    // frames when swapping documents.
    scoped_defer_main_frame_update_ =
        MainFrameImpl()->FrameWidgetImpl()->DeferMainFrameUpdate();
  }
}

void WebViewImpl::InvalidateRect(const IntRect& rect) {
  // This is only for WebViewPlugin.
  if (!does_composite_ && web_view_client_)
    web_view_client_->DidInvalidateRect(rect);
}

void WebViewImpl::ApplyViewportChanges(const ApplyViewportChangesArgs& args) {
  VisualViewport& visual_viewport = GetPage()->GetVisualViewport();

  // Store the desired offsets the visual viewport before setting the top
  // controls ratio since doing so will change the bounds and move the
  // viewports to keep the offsets valid. The compositor may have already
  // done that so we don't want to double apply the deltas here.
  FloatPoint visual_viewport_offset = visual_viewport.VisibleRect().Location();
  visual_viewport_offset.Move(args.inner_delta.x(), args.inner_delta.y());

  GetBrowserControls().SetShownRatio(
      GetBrowserControls().TopShownRatio() + args.top_controls_delta,
      GetBrowserControls().BottomShownRatio() + args.bottom_controls_delta);

  SetPageScaleFactorAndLocation(PageScaleFactor() * args.page_scale_delta,
                                args.is_pinch_gesture_active,
                                visual_viewport_offset);

  if (args.page_scale_delta != 1) {
    double_tap_zoom_pending_ = false;
    visual_viewport.UserDidChangeScale();
  }

  elastic_overscroll_ += FloatSize(args.elastic_overscroll_delta.x(),
                                   args.elastic_overscroll_delta.y());
  UpdateBrowserControlsConstraint(args.browser_controls_constraint);

  if (args.scroll_gesture_did_end)
    MainFrameImpl()->GetFrame()->GetEventHandler().MarkHoverStateDirty();
}

void WebViewImpl::RecordManipulationTypeCounts(cc::ManipulationInfo info) {
  if (!MainFrameImpl())
    return;

  if ((info & cc::kManipulationInfoWheel) == cc::kManipulationInfoWheel) {
    UseCounter::Count(MainFrameImpl()->GetDocument(),
                      WebFeature::kScrollByWheel);
  }
  if ((info & cc::kManipulationInfoTouch) == cc::kManipulationInfoTouch) {
    UseCounter::Count(MainFrameImpl()->GetDocument(),
                      WebFeature::kScrollByTouch);
  }
  if ((info & cc::kManipulationInfoPinchZoom) ==
      cc::kManipulationInfoPinchZoom) {
    UseCounter::Count(MainFrameImpl()->GetDocument(), WebFeature::kPinchZoom);
  }
  if ((info & cc::kManipulationInfoPrecisionTouchPad) ==
      cc::kManipulationInfoPrecisionTouchPad) {
    UseCounter::Count(MainFrameImpl()->GetDocument(),
                      WebFeature::kScrollByPrecisionTouchPad);
  }
}

Node* WebViewImpl::FindNodeFromScrollableCompositorElementId(
    cc::ElementId element_id) const {
  if (!GetPage())
    return nullptr;

  if (element_id == GetPage()->GetVisualViewport().GetScrollElementId()) {
    // Return the Document in this case since the window.visualViewport DOM
    // object is not a node.
    if (MainFrameImpl())
      return MainFrameImpl()->GetDocument();
  }

  if (!GetPage()->GetScrollingCoordinator())
    return nullptr;
  ScrollableArea* scrollable_area =
      GetPage()
          ->GetScrollingCoordinator()
          ->ScrollableAreaWithElementIdInAllLocalFrames(element_id);
  if (!scrollable_area || !scrollable_area->GetLayoutBox())
    return nullptr;

  return scrollable_area->GetLayoutBox()->GetNode();
}

void WebViewImpl::SendOverscrollEventFromImplSide(
    const gfx::Vector2dF& overscroll_delta,
    cc::ElementId scroll_latched_element_id) {
  if (!RuntimeEnabledFeatures::OverscrollCustomizationEnabled())
    return;

  DCHECK(!overscroll_delta.IsZero());
  Node* target_node =
      FindNodeFromScrollableCompositorElementId(scroll_latched_element_id);
  if (target_node) {
    target_node->GetDocument().EnqueueOverscrollEventForNode(
        target_node, overscroll_delta.x(), overscroll_delta.y());
  }
}

void WebViewImpl::SendScrollEndEventFromImplSide(
    cc::ElementId scroll_latched_element_id) {
  if (!RuntimeEnabledFeatures::OverscrollCustomizationEnabled())
    return;

  Node* target_node =
      FindNodeFromScrollableCompositorElementId(scroll_latched_element_id);
  if (target_node)
    target_node->GetDocument().EnqueueScrollEndEventForNode(target_node);
}

void WebViewImpl::UpdateDeviceEmulationTransform() {
  GetPage()->GetVisualViewport().SetNeedsPaintPropertyUpdate();

  if (MainFrameImpl()) {
    // When the device emulation transform is updated, to avoid incorrect
    // scales and fuzzy raster from the compositor, force all content to
    // pick ideal raster scales.
    // TODO(wjmaclean): This is only done on the main frame's widget currently,
    // it should update all local frames.
    MainFrameImpl()->FrameWidgetImpl()->SetNeedsRecalculateRasterScales();
  }
}

PageScheduler* WebViewImpl::Scheduler() const {
  DCHECK(GetPage());
  return GetPage()->GetPageScheduler();
}

void WebViewImpl::SetVisibilityState(
    mojom::blink::PageVisibilityState visibility_state,
    bool is_initial_state) {
  DCHECK(GetPage());
  if (!is_initial_state) {
    // Preserve the side effects of visibility change.
    web_view_client_->OnPageVisibilityChanged(visibility_state);
  }
  GetPage()->SetVisibilityState(visibility_state, is_initial_state);
  GetPage()->GetPageScheduler()->SetPageVisible(
      visibility_state == mojom::blink::PageVisibilityState::kVisible);
}

mojom::blink::PageVisibilityState WebViewImpl::GetVisibilityState() {
  DCHECK(GetPage());
  return GetPage()->GetVisibilityState();
}

float WebViewImpl::DeviceScaleFactor() const {
  // TODO(oshima): Investigate if this should return the ScreenInfo's scale
  // factor rather than page's scale factor, which can be 1 in use-zoom-for-dsf
  // mode.
  if (!GetPage())
    return 1;

  return GetPage()->DeviceScaleFactorDeprecated();
}

LocalFrame* WebViewImpl::FocusedLocalFrameInWidget() const {
  if (!MainFrameImpl())
    return nullptr;

  auto* focused_frame = To<LocalFrame>(FocusedCoreFrame());
  if (focused_frame->LocalFrameRoot() != MainFrameImpl()->GetFrame())
    return nullptr;
  return focused_frame;
}

LocalFrame* WebViewImpl::FocusedLocalFrameAvailableForIme() const {
  return ime_accept_events_ ? FocusedLocalFrameInWidget() : nullptr;
}

void WebViewImpl::SetPageFrozen(bool frozen) {
  Scheduler()->SetPageFrozen(frozen);
  web_view_client_->OnPageFrozenChanged(frozen);
}

WebFrameWidget* WebViewImpl::MainFrameWidget() {
  return web_widget_;
}

void WebViewImpl::AddAutoplayFlags(int32_t value) {
  page_->AddAutoplayFlags(value);
}

void WebViewImpl::ClearAutoplayFlags() {
  page_->ClearAutoplayFlags();
}

int32_t WebViewImpl::AutoplayFlagsForTest() {
  return page_->AutoplayFlags();
}

gfx::Size WebViewImpl::GetPreferredSizeForTest() {
  return preferred_size_in_dips_;
}

void WebViewImpl::StopDeferringMainFrameUpdate() {
  DCHECK(MainFrameImpl());
  scoped_defer_main_frame_update_ = nullptr;
}

void WebViewImpl::SetDeviceColorSpaceForTesting(
    const gfx::ColorSpace& color_space) {
  web_widget_->SetDeviceColorSpaceForTesting(color_space);
}

void WebViewImpl::RunPaintBenchmark(int repeat_count,
                                    cc::PaintBenchmarkResult& result) {
  DCHECK(MainFrameImpl());
  if (auto* frame_view = MainFrameImpl()->GetFrameView())
    frame_view->RunPaintBenchmark(repeat_count, result);
}

}  // namespace blink
