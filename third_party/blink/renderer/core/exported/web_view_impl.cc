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

#include "base/command_line.h"
#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "cc/layers/picture_layer.h"
#include "components/viz/common/features.h"
#include "media/base/media_switches.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/history/session_history_constants.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/input/web_menu_source_type.h"
#include "third_party/blink/public/common/page/color_provider_color_maps.h"
#include "third_party/blink/public/common/page/page_zoom.h"
#include "third_party/blink/public/common/renderer_preferences/renderer_preferences.h"
#include "third_party/blink/public/common/switches.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "third_party/blink/public/mojom/frame/frame_replication_state.mojom-blink.h"
#include "third_party/blink/public/mojom/input/focus_type.mojom-blink.h"
#include "third_party/blink/public/mojom/page/draggable_region.mojom-blink.h"
#include "third_party/blink/public/mojom/page/prerender_page_param.mojom.h"
#include "third_party/blink/public/mojom/partitioned_popins/partitioned_popin_params.mojom.h"
#include "third_party/blink/public/mojom/window_features/window_features.mojom-blink.h"
#include "third_party/blink/public/platform/interface_registry.h"
#include "third_party/blink/public/platform/platform.h"
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
#include "third_party/blink/public/web/web_navigation_type.h"
#include "third_party/blink/public/web/web_node.h"
#include "third_party/blink/public/web/web_plugin.h"
#include "third_party/blink/public/web/web_range.h"
#include "third_party/blink/public/web/web_render_theme.h"
#include "third_party/blink/public/web/web_view_client.h"
#include "third_party/blink/public/web/web_window_features.h"
#include "third_party/blink/renderer/core/clipboard/data_object.h"
#include "third_party/blink/renderer/core/content_capture/content_capture_manager.h"
#include "third_party/blink/renderer/core/core_initializer.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"
#include "third_party/blink/renderer/core/dom/layout_tree_builder_traversal.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/editor.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/ime/edit_context.h"
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
#include "third_party/blink/renderer/core/exported/web_settings_impl.h"
#include "third_party/blink/renderer/core/frame/browser_controls.h"
#include "third_party/blink/renderer/core/frame/dom_window.h"
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
#include "third_party/blink/renderer/core/frame/web_frame_widget_impl.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/frame/web_remote_frame_impl.h"
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
#include "third_party/blink/renderer/core/loader/no_state_prefetch_client.h"
#include "third_party/blink/renderer/core/page/chrome_client_impl.h"
#include "third_party/blink/renderer/core/page/context_menu_controller.h"
#include "third_party/blink/renderer/core/page/context_menu_provider.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/page/frame_tree.h"
#include "third_party/blink/renderer/core/page/link_highlight.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/page_popup_client.h"
#include "third_party/blink/renderer/core/page/pointer_lock_controller.h"
#include "third_party/blink/renderer/core/page/scrolling/scrolling_coordinator.h"
#include "third_party/blink/renderer/core/page/scrolling/top_document_root_scroller_controller.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing_detector.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/scroll/scroll_into_view_util.h"
#include "third_party/blink/renderer/core/scroll/scrollbar_theme.h"
#include "third_party/blink/renderer/core/speculation_rules/document_speculation_rules.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/core/timing/performance.h"
#include "third_party/blink/renderer/core/timing/window_performance.h"
#include "third_party/blink/renderer/core/view_transition/view_transition_supplement.h"
#include "third_party/blink/renderer/platform/fonts/font_cache.h"
#include "third_party/blink/renderer/platform/fonts/generic_font_family_settings.h"
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
#include "third_party/blink/renderer/platform/scheduler/public/page_scheduler.h"
#include "third_party/blink/renderer/platform/theme/web_theme_engine_helper.h"
#include "third_party/blink/renderer/platform/weborigin/known_ports.h"
#include "third_party/blink/renderer/platform/widget/widget_base.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "third_party/icu/source/common/unicode/uscript.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/geometry/skia_conversions.h"

#if !BUILDFLAG(IS_MAC)
#include "skia/ext/legacy_display_globals.h"
#include "third_party/blink/public/platform/web_font_render_style.h"
#include "ui/gfx/font_render_params.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "third_party/blink/public/web/win/web_font_rendering.h"
#endif

// Get rid of WTF's pow define so we can use std::pow.
#undef pow
#include <cmath>  // for std::pow

#include "build/chromeos_buildflags.h"

// The following constants control parameters for automated scaling of webpages
// (such as due to a double tap gesture or find in page etc.). These are
// experimentally determined.
static const int touchPointPadding = 32;
static const int nonUserInitiatedPointPadding = 11;
static const float minScaleDifference = 0.01f;
static const float doubleTapZoomContentDefaultMargin = 5;
static const float doubleTapZoomContentMinimumMargin = 2;
static constexpr base::TimeDelta kDoubleTapZoomAnimationDuration =
    base::Milliseconds(250);
static const float doubleTapZoomAlreadyLegibleRatio = 1.2f;

static constexpr base::TimeDelta kFindInPageAnimationDuration;

// Constants for viewport anchoring on resize.
static const float viewportAnchorCoordX = 0.5f;
static const float viewportAnchorCoordY = 0;

// Constants for zooming in on a focused text field.
static constexpr base::TimeDelta kScrollAndScaleAnimationDuration =
    base::Milliseconds(200);
static const int minReadableCaretHeight = 16;
static const int minReadableCaretHeightForTextArea = 13;
static const float minScaleChangeToTriggerZoom = 1.5f;
static const float leftBoxRatio = 0.3f;
static const int caretPadding = 10;

namespace blink {

using mojom::blink::EffectiveConnectionType;

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
                                     const std::u16string&,
                                     UScriptCode);

void SetStandardFontFamilyWrapper(WebSettings* settings,
                                  const std::u16string& font,
                                  UScriptCode script) {
  settings->SetStandardFontFamily(WebString::FromUTF16(font), script);
}

void SetFixedFontFamilyWrapper(WebSettings* settings,
                               const std::u16string& font,
                               UScriptCode script) {
  settings->SetFixedFontFamily(WebString::FromUTF16(font), script);
}

void SetSerifFontFamilyWrapper(WebSettings* settings,
                               const std::u16string& font,
                               UScriptCode script) {
  settings->SetSerifFontFamily(WebString::FromUTF16(font), script);
}

void SetSansSerifFontFamilyWrapper(WebSettings* settings,
                                   const std::u16string& font,
                                   UScriptCode script) {
  settings->SetSansSerifFontFamily(WebString::FromUTF16(font), script);
}

void SetCursiveFontFamilyWrapper(WebSettings* settings,
                                 const std::u16string& font,
                                 UScriptCode script) {
  settings->SetCursiveFontFamily(WebString::FromUTF16(font), script);
}

void SetFantasyFontFamilyWrapper(WebSettings* settings,
                                 const std::u16string& font,
                                 UScriptCode script) {
  settings->SetFantasyFontFamily(WebString::FromUTF16(font), script);
}

void SetMathFontFamilyWrapper(WebSettings* settings,
                              const std::u16string& font,
                              UScriptCode script) {
  settings->SetMathFontFamily(WebString::FromUTF16(font), script);
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

  std::string touch_text_selection_strategy =
      command_line.GetSwitchValueASCII(switches::kTouchTextSelectionStrategy);
  if (touch_text_selection_strategy ==
      switches::kTouchTextSelectionStrategy_Character) {
    settings->SetSelectionStrategy(
        WebSettings::SelectionStrategyType::kCharacter);
  } else if (touch_text_selection_strategy ==
             switches::kTouchTextSelectionStrategy_Direction) {
    settings->SetSelectionStrategy(
        WebSettings::SelectionStrategyType::kDirection);
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

ui::mojom::blink::WindowOpenDisposition NavigationPolicyToDisposition(
    NavigationPolicy policy) {
  switch (policy) {
    case kNavigationPolicyDownload:
      return ui::mojom::blink::WindowOpenDisposition::SAVE_TO_DISK;
    case kNavigationPolicyCurrentTab:
      return ui::mojom::blink::WindowOpenDisposition::CURRENT_TAB;
    case kNavigationPolicyNewBackgroundTab:
      return ui::mojom::blink::WindowOpenDisposition::NEW_BACKGROUND_TAB;
    case kNavigationPolicyNewForegroundTab:
      return ui::mojom::blink::WindowOpenDisposition::NEW_FOREGROUND_TAB;
    case kNavigationPolicyNewWindow:
      return ui::mojom::blink::WindowOpenDisposition::NEW_WINDOW;
    case kNavigationPolicyNewPopup:
      return ui::mojom::blink::WindowOpenDisposition::NEW_POPUP;
    case kNavigationPolicyPictureInPicture:
      return ui::mojom::blink::WindowOpenDisposition::NEW_PICTURE_IN_PICTURE;
    case kNavigationPolicyLinkPreview:
      NOTREACHED();
  }
  NOTREACHED_IN_MIGRATION() << "Unexpected NavigationPolicy";
  return ui::mojom::blink::WindowOpenDisposition::IGNORE_ACTION;
}

// Records the queuing duration for activation IPC.
void RecordPrerenderActivationSignalDelay(const String& metric_suffix) {
  auto* task = base::TaskAnnotator::CurrentTaskForThread();

  // It should be a Mojo call, so `RunTask` executes it as a non-delayed task.
  CHECK(task);
  CHECK(task->delayed_run_time.is_null());
  base::TimeDelta queueing_time =
      !task->queue_time.is_null() ? base::TimeTicks::Now() - task->queue_time
                                  : base::TimeDelta();
  base::UmaHistogramTimes(
      "Prerender.Experimental.ActivationIPCDelay" + metric_suffix.Ascii(),
      queueing_time);
}

#if !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_WIN)
SkFontHinting RendererPreferencesToSkiaHinting(
    const blink::RendererPreferences& prefs) {
// TODO(crbug.com/1052397): Revisit once build flag switch of lacros-chrome is
// complete.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
  if (!prefs.should_antialias_text) {
    // When anti-aliasing is off, GTK maps all non-zero hinting settings to
    // 'Normal' hinting so we do the same. Otherwise, folks who have 'Slight'
    // hinting selected will see readable text in everything expect Chromium.
    switch (prefs.hinting) {
      case gfx::FontRenderParams::HINTING_NONE:
        return SkFontHinting::kNone;
      case gfx::FontRenderParams::HINTING_SLIGHT:
      case gfx::FontRenderParams::HINTING_MEDIUM:
      case gfx::FontRenderParams::HINTING_FULL:
        return SkFontHinting::kNormal;
      default:
        NOTREACHED_IN_MIGRATION();
        return SkFontHinting::kNormal;
    }
  }
#endif

  switch (prefs.hinting) {
    case gfx::FontRenderParams::HINTING_NONE:
      return SkFontHinting::kNone;
    case gfx::FontRenderParams::HINTING_SLIGHT:
      return SkFontHinting::kSlight;
    case gfx::FontRenderParams::HINTING_MEDIUM:
      return SkFontHinting::kNormal;
    case gfx::FontRenderParams::HINTING_FULL:
      return SkFontHinting::kFull;
    default:
      NOTREACHED_IN_MIGRATION();
      return SkFontHinting::kNormal;
  }
}
#endif  // !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_WIN)

void ForEachFrameWidgetControlledByView(
    WebViewImpl& web_view,
    base::FunctionRef<void(WebFrameWidgetImpl*)> callback) {
  for (WebFrame* frame = web_view.MainFrame(); frame;
       frame = frame->TraverseNext()) {
    if (auto* frame_impl = DynamicTo<WebLocalFrameImpl>(frame)) {
      if (frame_impl->GetFrame()->IsLocalRoot()) {
        if (auto* widget = frame_impl->FrameWidgetImpl()) {
          callback(widget);
        }
      }
    }
  }
}

void MaybePreloadSystemFonts(Page* page) {
  static bool is_first_run = true;
  if (!is_first_run) {
    return;
  }
  is_first_run = false;

  page->GetAgentGroupScheduler().DefaultTaskRunner()->PostTask(
      FROM_HERE, WTF::BindOnce([]() { FontCache::MaybePreloadSystemFonts(); }));
}

}  // namespace

// WebView ----------------------------------------------------------------

WebView* WebView::Create(
    WebViewClient* client,
    bool is_hidden,
    blink::mojom::PrerenderParamPtr prerender_param,
    std::optional<blink::FencedFrame::DeprecatedFencedFrameMode>
        fenced_frame_mode,
    bool compositing_enabled,
    bool widgets_never_composited,
    WebView* opener,
    CrossVariantMojoAssociatedReceiver<mojom::PageBroadcastInterfaceBase>
        page_handle,
    scheduler::WebAgentGroupScheduler& agent_group_scheduler,
    const SessionStorageNamespaceId& session_storage_namespace_id,
    std::optional<SkColor> page_base_background_color,
    const BrowsingContextGroupInfo& browsing_context_group_info,
    const ColorProviderColorMaps* color_provider_colors,
    blink::mojom::PartitionedPopinParamsPtr partitioned_popin_params) {
  return WebViewImpl::Create(
      client,
      is_hidden ? mojom::blink::PageVisibilityState::kHidden
                : mojom::blink::PageVisibilityState::kVisible,
      std::move(prerender_param), fenced_frame_mode, compositing_enabled,
      widgets_never_composited, To<WebViewImpl>(opener), std::move(page_handle),
      agent_group_scheduler, session_storage_namespace_id,
      std::move(page_base_background_color), browsing_context_group_info,
      color_provider_colors, std::move(partitioned_popin_params));
}

WebViewImpl* WebViewImpl::Create(
    WebViewClient* client,
    mojom::blink::PageVisibilityState visibility,
    blink::mojom::PrerenderParamPtr prerender_param,
    std::optional<blink::FencedFrame::DeprecatedFencedFrameMode>
        fenced_frame_mode,
    bool compositing_enabled,
    bool widgets_never_composited,
    WebViewImpl* opener,
    mojo::PendingAssociatedReceiver<mojom::blink::PageBroadcast> page_handle,
    blink::scheduler::WebAgentGroupScheduler& agent_group_scheduler,
    const SessionStorageNamespaceId& session_storage_namespace_id,
    std::optional<SkColor> page_base_background_color,
    const BrowsingContextGroupInfo& browsing_context_group_info,
    const ColorProviderColorMaps* color_provider_colors,
    blink::mojom::PartitionedPopinParamsPtr partitioned_popin_params) {
  return new WebViewImpl(
      client, visibility, std::move(prerender_param), fenced_frame_mode,
      compositing_enabled, widgets_never_composited, opener,
      std::move(page_handle), agent_group_scheduler,
      session_storage_namespace_id, std::move(page_base_background_color),
      browsing_context_group_info, color_provider_colors,
      std::move(partitioned_popin_params));
}

size_t WebView::GetWebViewCount() {
  return WebViewImpl::AllInstances().size();
}

void WebView::UpdateVisitedLinkState(uint64_t link_hash) {
  Page::VisitedStateChanged(link_hash);
}

void WebView::ResetVisitedLinkState(bool invalidate_visited_link_hashes) {
  Page::AllVisitedStateChanged(invalidate_visited_link_hashes);
}

void WebViewImpl::SetNoStatePrefetchClient(
    WebNoStatePrefetchClient* no_state_prefetch_client) {
  DCHECK(page_);
  ProvideNoStatePrefetchClientTo(*page_,
                                 MakeGarbageCollected<NoStatePrefetchClient>(
                                     *page_, no_state_prefetch_client));
}

void WebViewImpl::CloseWindow() {
#if !(BUILDFLAG(IS_ANDROID) || \
      (BUILDFLAG(IS_CHROMEOS) && defined(ARCH_CPU_ARM64)))
  auto close_task_trace = close_task_posted_stack_trace_;
  base::debug::Alias(&close_task_trace);
  auto close_trace = close_called_stack_trace_;
  base::debug::Alias(&close_trace);
  auto prev_close_window_trace = close_window_called_stack_trace_;
  base::debug::Alias(&prev_close_window_trace);
  close_window_called_stack_trace_.emplace();
  auto cur_close_window_trace = close_window_called_stack_trace_;
  base::debug::Alias(&cur_close_window_trace);
#endif
  SCOPED_CRASH_KEY_BOOL("Bug1499519", "page_exists", !!page_);

  // Have the browser process a close request. We should have either a
  // |local_main_frame_host_remote_| or |remote_main_frame_host_remote_|.
  // This method will not execute if Close has been called as WeakPtrs
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
    blink::mojom::PrerenderParamPtr prerender_param,
    std::optional<blink::FencedFrame::DeprecatedFencedFrameMode>
        fenced_frame_mode,
    bool does_composite,
    bool widgets_never_composited,
    WebViewImpl* opener,
    mojo::PendingAssociatedReceiver<mojom::blink::PageBroadcast> page_handle,
    blink::scheduler::WebAgentGroupScheduler& agent_group_scheduler,
    const SessionStorageNamespaceId& session_storage_namespace_id,
    std::optional<SkColor> page_base_background_color,
    const BrowsingContextGroupInfo& browsing_context_group_info,
    const ColorProviderColorMaps* color_provider_colors,
    blink::mojom::PartitionedPopinParamsPtr partitioned_popin_params)
    : widgets_never_composited_(widgets_never_composited),
      web_view_client_(client),
      chrome_client_(MakeGarbageCollected<ChromeClientImpl>(this)),
      minimum_zoom_level_(
          blink::ZoomFactorToZoomLevel(kMinimumBrowserZoomFactor)),
      maximum_zoom_level_(
          blink::ZoomFactorToZoomLevel(kMaximumBrowserZoomFactor)),
      does_composite_(does_composite),
      fullscreen_controller_(std::make_unique<FullscreenController>(this)),
      page_base_background_color_(
          page_base_background_color.value_or(SK_ColorWHITE)),
      receiver_(this,
                std::move(page_handle),
                agent_group_scheduler.DefaultTaskRunner()),
      session_storage_namespace_id_(session_storage_namespace_id),
      web_agent_group_scheduler_(agent_group_scheduler) {
  if (receiver_) {
    // Typically, the browser process closes the corresponding peer handle
    // to signal the renderer process to destroy `this`. In certain
    // situations where the lifetime of `this` is not controlled by a
    // corresponding browser-side `RenderViewHostImpl` (e.g. tests or
    // printing), call `Close()` directly instead to delete `this`.
    receiver_.set_disconnect_handler(
        WTF::BindOnce(&WebViewImpl::MojoDisconnected, WTF::Unretained(this)));
  }
  if (!web_view_client_)
    DCHECK(!does_composite_);
  page_ = Page::CreateOrdinary(
      *chrome_client_, opener ? opener->GetPage() : nullptr,
      agent_group_scheduler.GetAgentGroupScheduler(),
      browsing_context_group_info, color_provider_colors,
      std::move(partitioned_popin_params));
  CoreInitializer::GetInstance().ProvideModulesToPage(
      *page_, session_storage_namespace_id_);

  SetVisibilityState(visibility, /*is_initial_state=*/true);
  if (prerender_param) {
    page_->SetIsPrerendering(true);
    page_->SetPrerenderMetricSuffix(
        String(prerender_param->page_metric_suffix));
    page_->SetShouldWarmUpCompositorOnPrerender(
        prerender_param->should_warm_up_compositor);
  }

  if (fenced_frame_mode && features::IsFencedFramesEnabled()) {
    page_->SetIsMainFrameFencedFrameRoot();
    page_->SetDeprecatedFencedFrameMode(*fenced_frame_mode);
  } else {
    // `fenced_frame_mode` should only be set if creating an MPArch
    // fenced frame.
    DCHECK(!fenced_frame_mode);
  }

  // When not compositing, keep the Page in the loop so that it will paint all
  // content into the root layer, as multiple layers can only be used when
  // compositing them together later.
  if (does_composite_)
    page_->GetSettings().SetAcceleratedCompositingEnabled(true);

  dev_tools_emulator_ = MakeGarbageCollected<DevToolsEmulator>(this);

  AllInstances().insert(this);

  resize_viewport_anchor_ = MakeGarbageCollected<ResizeViewportAnchor>(*page_);

  // Ensure we have valid page scale constraints even if the embedder never
  // changes defaults.
  GetPageScaleConstraintsSet().ComputeFinalConstraints();
}

WebViewImpl::~WebViewImpl() {
  DCHECK(!page_);
}

void WebViewImpl::SetTabKeyCyclesThroughElements(bool value) {
  if (page_)
    page_->SetTabKeyCyclesThroughElements(value);
}

bool WebViewImpl::StartPageScaleAnimation(const gfx::Point& target_position,
                                          bool use_anchor,
                                          float new_scale,
                                          base::TimeDelta duration) {
  // PageScaleFactor is a property of the main frame only, and only exists when
  // compositing.
  DCHECK(MainFrameImpl());
  DCHECK(does_composite_);

  VisualViewport& visual_viewport = GetPage()->GetVisualViewport();
  DCHECK(visual_viewport.IsActiveViewport());

  gfx::Point clamped_point = target_position;
  if (!use_anchor) {
    clamped_point =
        visual_viewport.ClampDocumentOffsetAtScale(target_position, new_scale);

    // TODO(bokan): Why special case duration zero? PageScaleAnimation should
    // work ok for that.
    if (duration.is_zero()) {
      SetPageScaleFactor(new_scale);

      LocalFrameView* view = MainFrameImpl()->GetFrameView();
      if (view && view->GetScrollableArea()) {
        view->GetScrollableArea()->SetScrollOffset(
            ScrollOffset(gfx::Vector2dF(clamped_point.OffsetFromOrigin())),
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
        target_position, use_anchor, new_scale, duration);
  }
  return true;
}

void WebViewImpl::EnableFakePageScaleAnimationForTesting(bool enable) {
  enable_fake_page_scale_animation_for_testing_ = enable;
  fake_page_scale_animation_target_position_ = gfx::Point();
  fake_page_scale_animation_use_anchor_ = false;
  fake_page_scale_animation_page_scale_factor_ = 0;
}

void WebViewImpl::AcceptLanguagesChanged() {
  FontCache::AcceptLanguagesChanged(
      String::FromUTF8(renderer_preferences_.accept_languages));

  if (!GetPage())
    return;

  GetPage()->AcceptLanguagesChanged();
}

gfx::Rect WebViewImpl::WidenRectWithinPageBounds(const gfx::Rect& source,
                                                 int target_margin,
                                                 int minimum_margin) {
  // Caller should guarantee that the main frame exists and is local.
  DCHECK(MainFrame());
  DCHECK(MainFrame()->IsWebLocalFrame());
  gfx::Size max_size = MainFrame()->ToWebLocalFrame()->DocumentSize();
  gfx::PointF scroll_offset = MainFrame()->ToWebLocalFrame()->GetScrollOffset();

  int left_margin = target_margin;
  int right_margin = target_margin;

  const int absolute_source_x = source.x() + scroll_offset.x();
  if (left_margin > absolute_source_x) {
    left_margin = absolute_source_x;
    right_margin = std::max(left_margin, minimum_margin);
  }

  const int maximum_right_margin =
      max_size.width() - (source.width() + absolute_source_x);
  if (right_margin > maximum_right_margin) {
    right_margin = maximum_right_margin;
    left_margin = std::min(left_margin, std::max(right_margin, minimum_margin));
  }

  const int new_width = source.width() + left_margin + right_margin;
  const int new_x = source.x() - left_margin;

  DCHECK_GE(new_width, 0);
  DCHECK_LE(scroll_offset.x() + new_x + new_width, max_size.width());

  return gfx::Rect(new_x, source.y(), new_width, source.height());
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
    const gfx::Rect& block_rect_in_root_frame,
    float padding,
    float default_scale_when_already_legible,
    float& scale,
    gfx::Point& scroll) {
  DCHECK(GetPage()->GetVisualViewport().IsActiveViewport());
  scale = PageScaleFactor();
  scroll = gfx::Point();

  gfx::Rect rect = block_rect_in_root_frame;

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
        rect, static_cast<int>(default_margin * rect.width() / size_.width()),
        static_cast<int>(minimum_margin * rect.width() / size_.width()));
    // Fit block to screen, respecting limits.
    scale = static_cast<float>(size_.width()) / rect.width();
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
  if (rect.height() < screen_height) {
    // Vertically center short blocks.
    rect.Offset(0, -0.5 * (screen_height - rect.height()));
  } else {
    // Ensure position we're zooming to (+ padding) isn't off the bottom of
    // the screen.
    rect.set_y(std::max<float>(
        rect.y(), hit_point_in_root_frame.y() + padding - screen_height));
  }  // Otherwise top align the block.

  // Do the same thing for horizontal alignment.
  if (rect.width() < screen_width) {
    rect.Offset(-0.5 * (screen_width - rect.width()), 0);
  } else {
    rect.set_x(std::max<float>(
        rect.x(), hit_point_in_root_frame.x() + padding - screen_width));
  }
  scroll.set_x(rect.x());
  scroll.set_y(rect.y());

  scale = ClampPageScaleFactorToLimits(scale);
  scroll = MainFrameImpl()->GetFrameView()->RootFrameToDocument(scroll);
  scroll =
      GetPage()->GetVisualViewport().ClampDocumentOffsetAtScale(scroll, scale);
}

static Node* FindLinkHighlightAncestor(Node* node) {
  // Go up the tree to find the node that defines a mouse cursor style
  while (node) {
    const LinkHighlightCandidate type = node->IsLinkHighlightCandidate();
    if (type == LinkHighlightCandidate::kYes)
      return node;
    if (type == LinkHighlightCandidate::kNo)
      return nullptr;
    node = LayoutTreeBuilderTraversal::Parent(*node);
  }
  return nullptr;
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
  if (IsEditable(*best_touch_node))
    return nullptr;

  Node* hand_cursor_ancestor = FindLinkHighlightAncestor(best_touch_node);
  // We show a highlight on tap only when the current node shows a hand cursor
  if (!hand_cursor_ancestor) {
    return nullptr;
  }

  // We should pick the largest enclosing node with hand cursor set. We do this
  // by first jumping up to the closest ancestor with hand cursor set. Then we
  // locate the next ancestor up in the the tree and repeat the jumps as long as
  // the node has hand cursor set.
  do {
    best_touch_node = hand_cursor_ancestor;
    hand_cursor_ancestor = FindLinkHighlightAncestor(
        LayoutTreeBuilderTraversal::Parent(*best_touch_node));
  } while (hand_cursor_ancestor);

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
  DCHECK(MainFrameImpl());
  Node* touch_node = BestTapNode(targeted_tap_event);
  GetPage()->GetLinkHighlight().SetTapHighlight(touch_node);
  MainFrameWidget()->UpdateLifecycle(WebLifecycleUpdate::kAll,
                                     DocumentUpdateReason::kTapHighlight);
}

void WebViewImpl::AnimateDoubleTapZoom(const gfx::Point& point_in_root_frame,
                                       const gfx::Rect& rect_to_zoom) {
  DCHECK(MainFrameImpl());

  float scale;
  gfx::Point scroll;

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
    gfx::Point target_position =
        MainFrameImpl()->GetFrameView()->RootFrameToDocument(
            gfx::Point(point_in_root_frame.x(), point_in_root_frame.y()));
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

void WebViewImpl::ZoomToFindInPageRect(const gfx::Rect& rect_in_root_frame) {
  DCHECK(MainFrameImpl());

  gfx::Rect block_bounds =
      MainFrameImpl()->FrameWidgetImpl()->ComputeBlockBound(
          gfx::Point(rect_in_root_frame.x() + rect_in_root_frame.width() / 2,
                     rect_in_root_frame.y() + rect_in_root_frame.height() / 2),
          true);

  if (block_bounds.IsEmpty()) {
    // Keep current scale (no need to scroll as x,y will normally already
    // be visible). FIXME: Revisit this if it isn't always true.
    return;
  }

  float scale;
  gfx::Point scroll;

  ComputeScaleAndScrollForBlockRect(rect_in_root_frame.origin(), block_bounds,
                                    nonUserInitiatedPointPadding,
                                    MinimumPageScaleFactor(), scale, scroll);

  StartPageScaleAnimation(scroll, false, scale, kFindInPageAnimationDuration);
}

#if !BUILDFLAG(IS_MAC)
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

  LocalFrame* opener_frame = client->OwnerElement().GetDocument().GetFrame();
  WebLocalFrameImpl* web_opener_frame =
      WebLocalFrameImpl::FromFrame(opener_frame);

  mojo::PendingAssociatedRemote<mojom::blink::Widget> widget;
  mojo::PendingAssociatedReceiver<mojom::blink::Widget> widget_receiver =
      widget.InitWithNewEndpointAndPassReceiver();

  mojo::PendingAssociatedRemote<mojom::blink::WidgetHost> widget_host;
  mojo::PendingAssociatedReceiver<mojom::blink::WidgetHost>
      widget_host_receiver = widget_host.InitWithNewEndpointAndPassReceiver();

  mojo::PendingAssociatedRemote<mojom::blink::PopupWidgetHost>
      popup_widget_host;
  mojo::PendingAssociatedReceiver<mojom::blink::PopupWidgetHost>
      popup_widget_host_receiver =
          popup_widget_host.InitWithNewEndpointAndPassReceiver();

  opener_frame->GetLocalFrameHostRemote().CreateNewPopupWidget(
      std::move(popup_widget_host_receiver), std::move(widget_host_receiver),
      std::move(widget));
  WebFrameWidgetImpl* opener_widget = web_opener_frame->LocalRootFrameWidget();

  AgentGroupScheduler& agent_group_scheduler =
      opener_frame->GetPage()->GetPageScheduler()->GetAgentGroupScheduler();
  // The returned WebPagePopup is self-referencing, so the pointer here is not
  // an owning pointer. It is de-referenced by the PopupWidgetHost disconnecting
  // and calling Close().
  page_popup_ = WebPagePopupImpl::Create(
      std::move(popup_widget_host), std::move(widget_host),
      std::move(widget_receiver), this, agent_group_scheduler,
      opener_widget->GetOriginalScreenInfos(), client);
  EnablePopupMouseWheelEventListener(web_opener_frame->LocalRoot());
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
#if !(BUILDFLAG(IS_ANDROID) || \
      (BUILDFLAG(IS_CHROMEOS) && defined(ARCH_CPU_ARM64)))
  auto close_task_trace = close_task_posted_stack_trace_;
  base::debug::Alias(&close_task_trace);
  auto prev_close_trace = close_called_stack_trace_;
  base::debug::Alias(&prev_close_trace);
  close_called_stack_trace_.emplace();
  auto cur_close_trace = close_called_stack_trace_;
  base::debug::Alias(&cur_close_trace);
  auto close_window_trace = close_window_called_stack_trace_;
  base::debug::Alias(&close_window_trace);
#endif
  SCOPED_CRASH_KEY_BOOL("Bug1499519", "page_exists", !!page_);

  // Closership is a single relationship, so only 1 call to Close() should
  // occur.
  CHECK(page_);
  DCHECK(AllInstances().Contains(this));
  AllInstances().erase(this);

  // Ensure if we have a page popup we cancel it immediately as we do not
  // want page popups to re-enter WebViewImpl during our shutdown.
  CancelPagePopup();

  receiver_.reset();

  dev_tools_emulator_->Shutdown();

  // Initiate shutdown for the entire frameset.  This will cause a lot of
  // notifications to be sent. This will detach all frames in this WebView's
  // frame tree.
  page_->WillBeDestroyed();
  page_.Clear();

  if (web_view_client_)
    web_view_client_->OnDestruct();

  // Reset the delegate to prevent notifications being sent as we're being
  // deleted.
  web_view_client_ = nullptr;

  for (auto& observer : observers_)
    observer.WebViewDestroyed();

  delete this;
}

gfx::Size WebViewImpl::Size() {
  return size_;
}

void WebViewImpl::ResizeVisualViewport(const gfx::Size& new_size) {
  GetPage()->GetVisualViewport().SetSize(new_size);
  GetPage()->GetVisualViewport().ClampToBoundaries();
}

void WebViewImpl::DidFirstVisuallyNonEmptyPaint() {
  DCHECK(MainFrameImpl());
  local_main_frame_host_remote_->DidFirstVisuallyNonEmptyPaint();
}

void WebViewImpl::UpdateICBAndResizeViewport(
    const gfx::Size& visible_viewport_size) {
  // We'll keep the initial containing block size from changing when the top
  // controls hide so that the ICB will always be the same size as the
  // viewport with the browser controls shown.
  gfx::Size icb_size = size_;
  if (GetBrowserControls().PermittedState() ==
          cc::BrowserControlsState::kBoth &&
      !GetBrowserControls().ShrinkViewport()) {
    icb_size.Enlarge(0, -(GetBrowserControls().TotalHeight() -
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
  if (!main_frame || !main_frame->IsOutermostMainFrame())
    return;

  WebFrameWidgetImpl* widget = main_frame->LocalRootFrameWidget();
  widget->SetBrowserControlsShownRatio(GetBrowserControls().TopShownRatio(),
                                       GetBrowserControls().BottomShownRatio());
  widget->SetBrowserControlsParams(GetBrowserControls().Params());

  VisualViewport& visual_viewport = GetPage()->GetVisualViewport();
  DCHECK(visual_viewport.IsActiveViewport());

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

  if (RuntimeEnabledFeatures::DynamicSafeAreaInsetsEnabled() &&
      RuntimeEnabledFeatures::DynamicSafeAreaInsetsOnScrollEnabled()) {
    GetPage()->UpdateSafeAreaInsetWithBrowserControls(GetBrowserControls());
  }
}

BrowserControls& WebViewImpl::GetBrowserControls() {
  return GetPage()->GetBrowserControls();
}

void WebViewImpl::ResizeViewWhileAnchored(
    cc::BrowserControlsParams params,
    const gfx::Size& visible_viewport_size) {
  DCHECK(MainFrameImpl());

  bool old_viewport_shrink = GetBrowserControls().ShrinkViewport();

  GetBrowserControls().SetParams(params);

  if (old_viewport_shrink != GetBrowserControls().ShrinkViewport())
    MainFrameImpl()->GetFrameView()->DynamicViewportUnitsChanged();

  if (RuntimeEnabledFeatures::DynamicSafeAreaInsetsEnabled()) {
    GetPage()->UpdateSafeAreaInsetWithBrowserControls(GetBrowserControls(),
                                                      /* force_update= */ true);
  }

  {
    // Avoids unnecessary invalidations while various bits of state in
    // TextAutosizer are updated.
    TextAutosizer::DeferUpdatePageInfo defer_update_page_info(GetPage());
    LocalFrameView* frame_view = MainFrameImpl()->GetFrameView();
    gfx::Size old_size = frame_view->Size();
    UpdateICBAndResizeViewport(visible_viewport_size);
    if (old_size != frame_view->Size()) {
      frame_view->InvalidateLayoutForViewportConstrainedObjects();
    }
  }

  fullscreen_controller_->UpdateSize();

  if (!scoped_defer_main_frame_update_) {
    // Page scale constraints may need to be updated; running layout now will
    // do that.
    MainFrameWidget()->UpdateLifecycle(WebLifecycleUpdate::kLayout,
                                       DocumentUpdateReason::kSizeChange);
  }
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
      GetPage()->GetVisualViewport().Size() == visible_viewport_size &&
      GetBrowserControls().Params() == browser_controls_params)
    return;

  if (GetPage()->MainFrame() && !GetPage()->MainFrame()->IsLocalFrame()) {
    // Viewport resize for a remote main frame does not require any
    // particular action, but the state needs to reflect the correct size
    // so that it can be used for initialization if the main frame gets
    // swapped to a LocalFrame at a later time.
    size_ = main_frame_widget_size;
    GetPageScaleConstraintsSet().DidChangeInitialContainingBlockSize(size_);
    GetPage()->GetVisualViewport().SetSize(size_);
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
      size_.width() && ContentsSize().width() &&
      main_frame_widget_size.width() != size_.width() &&
      !fullscreen_controller_->IsFullscreenOrTransitioning();
  size_ = main_frame_widget_size;

  if (!main_frame->IsOutermostMainFrame()) {
    // Anchoring should not be performed from embedded frames as anchoring
    // should only be performed when the size/orientation is user controlled.
    ResizeViewWhileAnchored(browser_controls_params, visible_viewport_size);
  } else if (is_rotation) {
    gfx::PointF viewport_anchor_coords(viewportAnchorCoordX,
                                       viewportAnchorCoordY);
    RotationViewportAnchor anchor(*view, visual_viewport,
                                  viewport_anchor_coords,
                                  GetPageScaleConstraintsSet());
    ResizeViewWhileAnchored(browser_controls_params, visible_viewport_size);
  } else {
    DCHECK(visual_viewport.IsActiveViewport());
    ResizeViewportAnchor::ResizeScope resize_scope(*resize_viewport_anchor_);
    ResizeViewWhileAnchored(browser_controls_params, visible_viewport_size);
  }

  // TODO(bokan): This will send a resize event even if the innerHeight on the
  // page didn't change (e.g. virtual keyboard causes resize of only visual
  // viewport). Lets remove this and have the frame send this event when its
  // frame rect is resized (as noted by the ancient FIXME inside this method).
  // https://crbug.com/1353728.
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
    std::optional<display::mojom::blink::ScreenOrientation> orientation) {
  screen_orientation_override_ = orientation;

  // Since we updated the override value, notify all widgets.
  for (WebFrame* frame = MainFrame(); frame; frame = frame->TraverseNext()) {
    if (frame->IsWebLocalFrame()) {
      if (WebFrameWidgetImpl* widget = static_cast<WebFrameWidgetImpl*>(
              frame->ToWebLocalFrame()->FrameWidget())) {
        widget->UpdateScreenInfo(widget->GetScreenInfos());
      }
    }
  }
}

void WebViewImpl::SetWindowRectSynchronouslyForTesting(
    const gfx::Rect& new_window_rect) {
  // We need to call UpdateScreenRects to ensure the 'move' event is enqueued.
  // TODO(jfernandez): Ideally updating the window rect should do that
  // automatically.
  web_widget_->UpdateScreenRects(new_window_rect, new_window_rect);
  web_widget_->SetWindowRectSynchronouslyForTesting(new_window_rect);
}

std::optional<display::mojom::blink::ScreenOrientation>
WebViewImpl::ScreenOrientationOverride() {
  return screen_orientation_override_;
}

void WebViewImpl::DidEnterFullscreen() {
  fullscreen_controller_->DidEnterFullscreen();
}

void WebViewImpl::DidExitFullscreen() {
  fullscreen_controller_->DidExitFullscreen();
}

void WebViewImpl::SetMainFrameViewWidget(WebFrameWidgetImpl* widget) {
  DCHECK(!widget || widget->ForMainFrame());
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

WebFrameWidgetImpl* WebViewImpl::MainFrameViewWidget() {
  return web_widget_;
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
  // TODO(crbug.com/1442088): Investigate the reason.
  if (!main_view.GetLayoutView()
           ->FirstFragment()
           .HasLocalBorderBoxProperties()) {
    return;
  }
  DCHECK_EQ(main_view.GetLayoutView()->GetDocument().Lifecycle().GetState(),
            DocumentLifecycle::kPaintClean);

  PaintRecordBuilder builder;
  main_view.PaintOutsideOfLifecycleWithThrottlingAllowed(
      builder.Context(), PaintFlag::kNoFlag, CullRect(rect));
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
  WebViewImpl* web_view_impl = To<WebViewImpl>(web_view);
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
  ApplyFontsFromMap(prefs.math_font_family_map, SetMathFontFamilyWrapper,
                    settings);
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
  settings->SetDNSPrefetchingEnabled(prefs.dns_prefetching_enabled);
  blink::WebNetworkStateNotifier::SetSaveDataEnabled(prefs.data_saver_enabled);
  settings->SetLocalStorageEnabled(prefs.local_storage_enabled);
  settings->SetSyncXHRInDocumentsEnabled(prefs.sync_xhr_in_documents_enabled);
  settings->SetTargetBlankImpliesNoOpenerEnabledWillBeRemoved(
      prefs.target_blank_implies_no_opener_enabled_will_be_removed);
  settings->SetAllowNonEmptyNavigatorPlugins(
      prefs.allow_non_empty_navigator_plugins);
  RuntimeEnabledFeatures::SetDatabaseEnabled(prefs.databases_enabled);
  settings->SetShouldProtectAgainstIpcFlooding(
      !prefs.disable_ipc_flooding_protection);
  settings->SetHyperlinkAuditingEnabled(prefs.hyperlink_auditing_enabled);
  settings->SetCookieEnabled(prefs.cookie_enabled);

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

  settings->SetPrefersDefaultScrollbarStyles(
      prefs.prefers_default_scrollbar_styles);

  // Enable gpu-accelerated 2d canvas if requested on the command line.
  RuntimeEnabledFeatures::SetAccelerated2dCanvasEnabled(
      prefs.accelerated_2d_canvas_enabled);

  RuntimeEnabledFeatures::SetCanvas2dLayersEnabled(
      prefs.canvas_2d_layers_enabled);

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

  DCHECK(!(web_view_impl->IsFencedFrameRoot() &&
           prefs.allow_running_insecure_content));
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
  settings->SetPrefersReducedTransparency(prefs.prefers_reduced_transparency);
  settings->SetInvertedColors(prefs.inverted_colors);

  RuntimeEnabledFeatures::SetTouchEventFeatureDetectionEnabled(
      prefs.touch_event_feature_detection_enabled);
  settings->SetMaxTouchPoints(prefs.pointer_events_max_touch_points);
  settings->SetAvailablePointerTypes(prefs.available_pointer_types);
  settings->SetPrimaryPointerType(prefs.primary_pointer_type);
  settings->SetAvailableHoverTypes(prefs.available_hover_types);
  settings->SetPrimaryHoverType(prefs.primary_hover_type);
  settings->SetOutputDeviceUpdateAbilityType(
      prefs.output_device_update_ability_type);
  settings->SetBarrelButtonForDragEnabled(prefs.barrel_button_for_drag_enabled);

  settings->SetEditingBehavior(prefs.editing_behavior);

  settings->SetSupportsMultipleWindows(prefs.supports_multiple_windows);

  settings->SetMainFrameClipsContent(!prefs.record_whole_document);

  RuntimeEnabledFeatures::SetStylusHandwritingEnabled(
      prefs.stylus_handwriting_enabled);

  settings->SetSmartInsertDeleteEnabled(prefs.smart_insert_delete_enabled);

  settings->SetSpatialNavigationEnabled(prefs.spatial_navigation_enabled);
  // Spatnav depends on KeyboardFocusableScrollers. The WebUI team has
  // disabled KFS because they need more time to update their custom elements,
  // crbug.com/907284. Meanwhile, we pre-ship KFS to spatnav users.
  if (prefs.spatial_navigation_enabled)
    RuntimeEnabledFeatures::SetKeyboardFocusableScrollersEnabled(true);

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

#if BUILDFLAG(IS_ANDROID)
  settings->SetAllowCustomScrollbarInMainFrame(false);
  settings->SetAccessibilityFontScaleFactor(prefs.font_scale_factor);
  settings->SetAccessibilityFontWeightAdjustment(prefs.font_weight_adjustment);
  settings->SetAccessibilityTextSizeContrastFactor(
      prefs.text_size_contrast_factor);
  settings->SetDeviceScaleAdjustment(prefs.device_scale_adjustment);
  web_view_impl->SetIgnoreViewportTagScaleLimits(prefs.force_enable_zoom);
  settings->SetDefaultVideoPosterURL(
      WebString::FromASCII(prefs.default_video_poster_url.spec()));
  settings->SetSupportDeprecatedTargetDensityDPI(
      prefs.support_deprecated_target_density_dpi);
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

  RuntimeEnabledFeatures::SetVideoFullscreenOrientationLockEnabled(
      prefs.video_fullscreen_orientation_lock_enabled);
  RuntimeEnabledFeatures::SetVideoRotateToFullscreenEnabled(
      prefs.video_rotate_to_fullscreen_enabled);
  settings->SetEmbeddedMediaExperienceEnabled(
      prefs.embedded_media_experience_enabled);
  settings->SetImmersiveModeEnabled(prefs.immersive_mode_enabled);
  settings->SetDoNotUpdateSelectionOnMutatingSelectionRange(
      prefs.do_not_update_selection_on_mutating_selection_range);
  RuntimeEnabledFeatures::SetCSSHexAlphaColorEnabled(
      prefs.css_hex_alpha_color_enabled);
  RuntimeEnabledFeatures::SetScrollTopLeftInteropEnabled(
      prefs.scroll_top_left_interop_enabled);
  RuntimeEnabledFeatures::SetAcceleratedSmallCanvasesEnabled(
      !prefs.disable_accelerated_small_canvases);
  RuntimeEnabledFeatures::SetLongPressLinkSelectTextEnabled(
      prefs.long_press_link_select_text);
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_FUCHSIA)
  RuntimeEnabledFeatures::SetWebAuthEnabled(!prefs.disable_webauthn);
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_FUCHSIA)

  settings->SetForceDarkModeEnabled(prefs.force_dark_mode_enabled);

  settings->SetAccessibilityAlwaysShowFocus(prefs.always_show_focus);
  settings->SetAutoplayPolicy(prefs.autoplay_policy);
  settings->SetRequireTransientActivationForGetDisplayMedia(
      prefs.require_transient_activation_for_get_display_media);
  settings->SetRequireTransientActivationForShowFileOrDirectoryPicker(
      prefs.require_transient_activation_for_show_file_or_directory_picker);
  settings->SetViewportEnabled(prefs.viewport_enabled);
  settings->SetViewportMetaEnabled(prefs.viewport_meta_enabled);
  settings->SetViewportStyle(prefs.viewport_style);
  settings->SetAutoZoomFocusedEditableToLegibleScale(
      prefs.auto_zoom_focused_editable_to_legible_scale);

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

  settings->SetPictureInPictureEnabled(prefs.picture_in_picture_enabled &&
                                       ::features::UseSurfaceLayerForVideo());

  settings->SetLazyLoadEnabled(prefs.lazy_load_enabled);
  settings->SetInForcedColors(prefs.in_forced_colors);
  settings->SetIsForcedColorsDisabled(prefs.is_forced_colors_disabled);
  settings->SetPreferredRootScrollbarColorScheme(
      prefs.preferred_root_scrollbar_color_scheme);
  settings->SetPreferredColorScheme(prefs.preferred_color_scheme);
  settings->SetPreferredContrast(prefs.preferred_contrast);

  settings->SetTouchDragDropEnabled(prefs.touch_drag_drop_enabled);
  settings->SetTouchDragEndContextMenu(prefs.touch_dragend_context_menu);
  settings->SetWebXRImmersiveArAllowed(prefs.webxr_immersive_ar_allowed);
  settings->SetModalContextMenu(prefs.modal_context_menu);
  settings->SetRequireTransientActivationAndAuthorizationForSubAppsAPIs(
      prefs.subapps_apis_require_user_gesture_and_authorization);

#if BUILDFLAG(IS_MAC)
  web_view_impl->SetMaximumLegibleScale(
      prefs.default_maximum_page_scale_factor);
#endif

#if BUILDFLAG(IS_WIN)
  RuntimeEnabledFeatures::SetMiddleClickAutoscrollEnabled(true);
#endif

  RuntimeEnabledFeatures::SetTranslateServiceEnabled(
      prefs.translate_service_available);

#if BUILDFLAG(IS_WIN)
  if (web_view_impl->GetPage() &&
      base::FeatureList::IsEnabled(features::kPrewarmDefaultFontFamilies)) {
    if (auto* prewarmer = WebFontRendering::GetFontPrewarmer()) {
      GenericFontFamilySettings& font_settings =
          web_view_impl->GetPage()
              ->GetSettings()
              .GetGenericFontFamilySettings();
      if (features::kPrewarmStandard.Get())
        prewarmer->PrewarmFamily(font_settings.Standard());
      if (features::kPrewarmFixed.Get())
        prewarmer->PrewarmFamily(font_settings.Fixed());
      if (features::kPrewarmSerif.Get())
        prewarmer->PrewarmFamily(font_settings.Serif());
      if (features::kPrewarmSansSerif.Get())
        prewarmer->PrewarmFamily(font_settings.SansSerif());
      if (features::kPrewarmCursive.Get())
        prewarmer->PrewarmFamily(font_settings.Cursive());
      if (features::kPrewarmFantasy.Get())
        prewarmer->PrewarmFamily(font_settings.Fantasy());
    }
  }
#endif

  // Disabling the StrictMimetypeCheckForWorkerScriptsEnabled enterprise policy
  // overrides the corresponding RuntimeEnabledFeature (via its Pref).
  if (!prefs.strict_mime_type_check_for_worker_scripts_enabled) {
    RuntimeEnabledFeatures::SetStrictMimeTypesForWorkersEnabled(false);
  }
}

void WebViewImpl::ThemeChanged() {
  if (auto* page = GetPage())
    page->InvalidatePaint();
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
                                           Element* new_element,
                                           const FullscreenOptions* options,
                                           FullscreenRequestType request_type) {
  fullscreen_controller_->FullscreenElementChanged(old_element, new_element,
                                                   options, request_type);
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

void WebViewImpl::SetPageFocus(bool enable) {
  page_->GetFocusController().SetFocused(enable);
  if (enable) {
    LocalFrame* focused_frame = page_->GetFocusController().FocusedFrame();
    if (focused_frame) {
      // TODO(editing-dev): The use of UpdateStyleAndLayout needs to be audited.
      // See http://crbug.com/590369 for more details.
      focused_frame->GetDocument()->UpdateStyleAndLayout(
          DocumentUpdateReason::kFocus);
      Element* element = focused_frame->GetDocument()->FocusedElement();
      if (element && focused_frame->Selection()
                         .ComputeVisibleSelectionInDOMTree()
                         .IsNone()) {
        // If the selection was cleared while the WebView was not
        // focused, then the focus element shows with a focus ring but
        // no caret and does respond to keyboard inputs.
        if (element->IsTextControl()) {
          element->UpdateSelectionOnFocus(SelectionBehaviorOnFocus::kRestore);
        } else if (IsEditable(*element)) {
          // updateFocusAppearance() selects all the text of
          // contentseditable DIVs. So we set the selection explicitly
          // instead. Note that this has the side effect of moving the
          // caret back to the beginning of the text.
          Position position(element, 0);
          focused_frame->Selection().SetSelection(
              SelectionInDOMTree::Builder().Collapse(position).Build(),
              SetSelectionOptions());
        }
      }
    }
  } else {
    CancelPagePopup();

    LocalFrame* focused_frame = page_->GetFocusController().FocusedFrame();
    if (focused_frame) {
      // Finish an ongoing composition to delete the composition node.
      if (focused_frame->GetInputMethodController().GetActiveEditContext()) {
        focused_frame->GetInputMethodController()
            .GetActiveEditContext()
            ->FinishComposingText(WebInputMethodController::kKeepSelection);
      } else if (focused_frame->GetInputMethodController().HasComposition()) {
        // TODO(editing-dev): The use of
        // UpdateStyleAndLayout needs to be audited.
        // See http://crbug.com/590369 for more details.
        focused_frame->GetDocument()->UpdateStyleAndLayout(
            DocumentUpdateReason::kFocus);

        focused_frame->GetInputMethodController().FinishComposingText(
            InputMethodController::kKeepSelection);
      }
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
  return WebFrame::FromCoreFrame(page ? page->MainFrame() : nullptr);
}

const WebFrame* WebViewImpl::MainFrame() const {
  Page* page = page_.Get();
  return WebFrame::FromCoreFrame(page ? page->MainFrame() : nullptr);
}

WebLocalFrameImpl* WebViewImpl::MainFrameImpl() const {
  Page* page = page_.Get();
  if (!page)
    return nullptr;
  return WebLocalFrameImpl::FromFrame(DynamicTo<LocalFrame>(page->MainFrame()));
}

std::string WebViewImpl::GetNullFrameReasonForBug1139104() const {
  Page* page = page_.Get();
  if (!page)
    return "WebViewImpl::page";
  if (!page->MainFrame())
    return "WebViewImpl::page->MainFrame";
  LocalFrame* local_frame = DynamicTo<LocalFrame>(page->MainFrame());
  if (!local_frame)
    return "WebViewImpl::local_frame";
  return WebLocalFrameImpl::GetNullFrameReasonForBug1139104(local_frame);
}

void WebViewImpl::DidAttachLocalMainFrame() {
  DCHECK(MainFrameImpl());
  DCHECK(!remote_main_frame_host_remote_);

  LocalFrame* local_frame = MainFrameImpl()->GetFrame();
  local_frame->WasAttachedAsLocalMainFrame();

  local_frame->GetRemoteNavigationAssociatedInterfaces()->GetInterface(
      local_main_frame_host_remote_.BindNewEndpointAndPassReceiver(
          GetPage()
              ->GetPageScheduler()
              ->GetAgentGroupScheduler()
              .DefaultTaskRunner()));

  auto& viewport = GetPage()->GetVisualViewport();
  if (does_composite_) {
    // When attaching a local main frame, set up any state on the compositor.
    MainFrameImpl()->FrameWidgetImpl()->SetBackgroundColor(BackgroundColor());
    MainFrameImpl()->FrameWidgetImpl()->SetPrefersReducedMotion(
        web_preferences_.prefers_reduced_motion);
    MainFrameImpl()->FrameWidgetImpl()->SetPageScaleStateAndLimits(
        viewport.Scale(), viewport.IsPinchGestureActive(),
        MinimumPageScaleFactor(), MaximumPageScaleFactor());
    // Prevent main frame updates while the main frame is loading until enough
    // progress is made and BeginMainFrames are explicitly asked for.
    scoped_defer_main_frame_update_ =
        MainFrameImpl()->FrameWidgetImpl()->DeferMainFrameUpdate();
  }

  // It's possible that at the time that `local_frame` attached its document it
  // was provisional so it couldn't initialize the root scroller. Try again now
  // that the frame has been attached; this is a no-op if the root scroller is
  // already initialized.
  if (viewport.IsActiveViewport()) {
    DCHECK(local_frame->GetDocument());
    // DidAttachLocalMainFrame can be called before a new document is attached
    // so ensure we don't try to initialize the root scroller on a stopped
    // document.
    if (local_frame->GetDocument()->IsActive())
      local_frame->View()->InitializeRootScroller();
  }
}

void WebViewImpl::DidAttachRemoteMainFrame(
    CrossVariantMojoAssociatedRemote<
        mojom::blink::RemoteMainFrameHostInterfaceBase> main_frame_host,
    CrossVariantMojoAssociatedReceiver<
        mojom::blink::RemoteMainFrameInterfaceBase> main_frame) {
  DCHECK(!MainFrameImpl());
  DCHECK(!local_main_frame_host_remote_);
  // Note that we didn't DCHECK the `main_frame_host` and `main_frame`, because
  // it's possible for those to be null, in case the remote main frame is a
  // placeholder RemoteFrame that does not have any browser-side counterpart.
  // This is possible when the WebView is created in preparation for a main
  // frame LocalFrame <-> LocalFrame swap. See the comments in
  // `AgentSchedulingGroup::CreateWebView()` for more details.

  RemoteFrame* remote_frame = DynamicTo<RemoteFrame>(GetPage()->MainFrame());
  remote_frame->WasAttachedAsRemoteMainFrame(std::move(main_frame));

  remote_main_frame_host_remote_.Bind(std::move(main_frame_host));

  auto& viewport = GetPage()->GetVisualViewport();
  DCHECK(!viewport.IsActiveViewport());
  viewport.Reset();
}

void WebViewImpl::DidDetachLocalMainFrame() {
  // The WebFrameWidget that generated the |scoped_defer_main_frame_update_|
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

void WebViewImpl::FinishScrollFocusedEditableIntoView(
    const gfx::RectF& caret_rect_in_root_frame,
    mojom::blink::ScrollIntoViewParamsPtr params) {
  DCHECK(MainFrameImpl());
  DCHECK(!IsFencedFrameRoot());
  DCHECK(!caret_rect_in_root_frame.IsEmpty());
  DCHECK(params->for_focused_editable);

  // Zoom if:
  // (1) Zoom to legible scale is enabled (i.e. Android)
  // (2) We're on a non-mobile-friendly page
  // (3) The element doesn't explicitly block pinch-zoom gestures so the user
  //     can zoom back out.
  bool zoom_into_legible_scale =
      web_settings_->AutoZoomFocusedEditableToLegibleScale() &&
      !GetPage()->GetVisualViewport().ShouldDisableDesktopWorkarounds() &&
      params->for_focused_editable->can_zoom;

  // Reconstruct the editable element's absolute rect from the caret-relative
  // location.
  gfx::RectF editable_rect_in_root_frame =
      scroll_into_view_util::FocusedEditableBoundsFromParams(
          caret_rect_in_root_frame, params);

  DCHECK(!editable_rect_in_root_frame.IsEmpty());

  float scale;
  gfx::Point scroll;
  bool need_animation = false;
  ComputeScaleAndScrollForEditableElementRects(
      gfx::ToEnclosedRect(editable_rect_in_root_frame),
      gfx::ToEnclosedRect(caret_rect_in_root_frame), zoom_into_legible_scale,
      scale, scroll, need_animation);

  if (need_animation) {
    StartPageScaleAnimation(scroll, false, scale,
                            kScrollAndScaleAnimationDuration);
  }
}

void WebViewImpl::SmoothScroll(int target_x,
                               int target_y,
                               base::TimeDelta duration) {
  gfx::Point target_position(target_x, target_y);
  StartPageScaleAnimation(target_position, false, PageScaleFactor(), duration);
}

void WebViewImpl::ComputeScaleAndScrollForEditableElementRects(
    const gfx::Rect& element_bounds_in_root_frame,
    const gfx::Rect& caret_bounds_in_root_frame,
    bool zoom_into_legible_scale,
    float& new_scale,
    gfx::Point& new_scroll_position,
    bool& need_animation) {
  VisualViewport& visual_viewport = GetPage()->GetVisualViewport();

  TopDocumentRootScrollerController& controller =
      GetPage()->GlobalRootScrollerController();
  Node* root_scroller = controller.GlobalRootScroller();

  gfx::Rect element_bounds_in_content;
  gfx::Rect caret_bounds_in_content;

  // If the page has a non-default root scroller then we need to put the
  // "in_content" coordinates into that scroller's coordinate space, rather
  // than the root frame's.
  if (root_scroller != MainFrameImpl()->GetFrame()->GetDocument() &&
      controller.RootScrollerArea()) {
    ScrollOffset offset = controller.RootScrollerArea()->GetScrollOffset();

    element_bounds_in_content = element_bounds_in_root_frame;
    caret_bounds_in_content = caret_bounds_in_root_frame;

    element_bounds_in_content.Offset(gfx::ToFlooredVector2d(offset));
    caret_bounds_in_content.Offset(gfx::ToFlooredVector2d(offset));
  } else {
    element_bounds_in_content =
        MainFrameImpl()->GetFrameView()->RootFrameToDocument(
            element_bounds_in_root_frame);
    caret_bounds_in_content =
        MainFrameImpl()->GetFrameView()->RootFrameToDocument(
            caret_bounds_in_root_frame);
  }

  if (!zoom_into_legible_scale) {
    new_scale = PageScaleFactor();
  } else {
    // Pick a scale which is reasonably readable. This is the scale at which
    // the caret height will become minReadableCaretHeightForNode (adjusted
    // for dpi and font scale factor).
    const int min_readable_caret_height_for_node =
        (element_bounds_in_content.height() >=
                 2 * caret_bounds_in_content.height()
             ? minReadableCaretHeightForTextArea
             : minReadableCaretHeight) *
        MainFrameImpl()->GetFrame()->LayoutZoomFactor();
    new_scale = ClampPageScaleFactorToLimits(
        MaximumLegiblePageScale() * min_readable_caret_height_for_node /
        caret_bounds_in_content.height());
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
  if (visual_viewport.VisibleRect().width() >=
          element_bounds_in_content.width() &&
      visual_viewport.VisibleRect().height() >=
          element_bounds_in_content.height() &&
      !root_viewport->VisibleContentRect().Contains(element_bounds_in_content))
    need_animation = true;

  if (!need_animation)
    return;

  gfx::SizeF target_viewport_size(visual_viewport.Size());
  target_viewport_size.Scale(1 / new_scale);

  // TODO(bokan): The logic below is all tailored assuming LTR writing mode.
  // Ideally, it'd perform its computations based on writing mode.
  ScrollOffset scroll_offset;
  if (element_bounds_in_content.width() <= target_viewport_size.width()) {
    // Field is narrower than screen. Try to leave padding on left so field's
    // label is visible, but it's more important to ensure entire field is
    // onscreen.
    int ideal_left_padding = target_viewport_size.width() * leftBoxRatio;
    int max_left_padding_keeping_box_onscreen =
        target_viewport_size.width() - element_bounds_in_content.width();
    scroll_offset.set_x(element_bounds_in_content.x() -
                        std::min<int>(ideal_left_padding,
                                      max_left_padding_keeping_box_onscreen));
  } else {
    // Field is wider than screen. Try to left-align field, unless caret would
    // be offscreen, in which case right-align the caret.
    scroll_offset.set_x(std::max<int>(
        element_bounds_in_content.x(),
        caret_bounds_in_content.x() + caret_bounds_in_content.width() +
            caretPadding - target_viewport_size.width()));
  }
  if (element_bounds_in_content.height() <= target_viewport_size.height()) {
    // Field is shorter than screen. Vertically center it.
    scroll_offset.set_y(
        element_bounds_in_content.y() -
        (target_viewport_size.height() - element_bounds_in_content.height()) /
            2);
  } else {
    // Field is taller than screen. Try to top align field, unless caret would
    // be offscreen, in which case bottom-align the caret.
    scroll_offset.set_y(std::max<int>(
        element_bounds_in_content.y(),
        caret_bounds_in_content.y() + caret_bounds_in_content.height() +
            caretPadding - target_viewport_size.height()));
  }

  // The output scroll will be used by the compositor so we must convert the
  // scroll-origin relative (i.e. writing-mode dependent) ScrollOffset with a
  // top-left relative scroll position.
  new_scroll_position =
      ToFlooredPoint(root_viewport->ScrollOffsetToPosition(scroll_offset));
}

void WebViewImpl::AdvanceFocus(bool reverse) {
  GetPage()->GetFocusController().AdvanceFocus(
      reverse ? mojom::blink::FocusType::kBackward
              : mojom::blink::FocusType::kForward);
}

double WebViewImpl::ClampZoomLevel(double zoom_level) const {
  return std::max(minimum_zoom_level_,
                  std::min(maximum_zoom_level_, zoom_level));
}

double WebViewImpl::ZoomLevelToZoomFactor(double zoom_level) const {
  if (zoom_factor_override_) {
    return zoom_factor_override_;
  }
  return blink::ZoomLevelToZoomFactor(zoom_level);
}

void WebViewImpl::UpdateWidgetZoomFactors() {
  ForEachFrameWidgetControlledByView(*this, [](WebFrameWidgetImpl* widget) {
    widget->SetZoomLevel(widget->GetZoomLevel());
  });
}

void WebViewImpl::UpdateInspectorDeviceScaleFactorOverride() {
  if (compositor_device_scale_factor_override_) {
    page_->SetInspectorDeviceScaleFactorOverride(
        zoom_factor_for_device_scale_factor_ /
        compositor_device_scale_factor_override_);
  } else {
    page_->SetInspectorDeviceScaleFactorOverride(1.0f);
  }
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
  GetPage()->GetVisualViewport().SetLocation(offset);
}

gfx::PointF WebViewImpl::VisualViewportOffset() const {
  DCHECK(GetPage());
  return GetPage()->GetVisualViewport().VisibleRect().origin();
}

gfx::SizeF WebViewImpl::VisualViewportSize() const {
  DCHECK(GetPage());
  return GetPage()->GetVisualViewport().VisibleRect().size();
}

void WebViewImpl::SetPageScaleFactorAndLocation(float scale_factor,
                                                bool is_pinch_gesture_active,
                                                const gfx::PointF& location) {
  DCHECK(GetPage());

  GetPage()->GetVisualViewport().SetScaleAndLocation(
      ClampPageScaleFactorToLimits(scale_factor), is_pinch_gesture_active,
      location);
}

void WebViewImpl::SetPageScaleFactor(float scale_factor) {
  DCHECK(GetPage());
  DCHECK(MainFrameImpl());

  if (LocalFrame* frame = MainFrameImpl()->GetFrame()) {
    frame->SetScaleFactor(scale_factor);
  }
}

void WebViewImpl::SetZoomFactorForDeviceScaleFactor(
    float zoom_factor_for_device_scale_factor) {
  DCHECK(does_composite_);
  if (zoom_factor_for_device_scale_factor_ !=
      zoom_factor_for_device_scale_factor) {
    zoom_factor_for_device_scale_factor_ = zoom_factor_for_device_scale_factor;
    UpdateWidgetZoomFactors();
    UpdateInspectorDeviceScaleFactorOverride();
  }
}

void WebViewImpl::SetPageLifecycleStateFromNewPageCommit(
    mojom::blink::PageVisibilityState visibility,
    mojom::blink::PagehideDispatch pagehide_dispatch) {
  TRACE_EVENT0("navigation",
               "WebViewImpl::SetPageLifecycleStateFromNewPageCommit");
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
  TRACE_EVENT0("navigation", "WebViewImpl::SetPageLifecycleState");
  SetPageLifecycleStateInternal(std::move(state),
                                std::move(page_restore_params));
  // Tell the browser that the lifecycle update was successful.
  std::move(callback).Run();
}

// Returns true if this state update is for the page being restored from
// back-forward cache, causing the pageshow event to fire with persisted=true.
bool IsRestoredFromBackForwardCache(
    const mojom::blink::PageLifecycleStatePtr& old_state,
    const mojom::blink::PageLifecycleStatePtr& new_state) {
  if (!old_state)
    return false;
  bool old_state_hidden = old_state->pagehide_dispatch !=
                          mojom::blink::PagehideDispatch::kNotDispatched;
  bool new_state_shown = new_state->pagehide_dispatch ==
                         mojom::blink::PagehideDispatch::kNotDispatched;
  // It's a pageshow but it can't be the initial pageshow since it was already
  // hidden. So it must be a back-forward cache restore.
  return old_state_hidden && new_state_shown;
}

void WebViewImpl::SetPageLifecycleStateInternal(
    mojom::blink::PageLifecycleStatePtr new_state,
    mojom::blink::PageRestoreParamsPtr page_restore_params) {
  Page* page = GetPage();
  if (!page)
    return;
  auto& old_state = page->GetPageLifecycleState();
  TRACE_EVENT2("navigation", "WebViewImpl::SetPageLifecycleStateInternal",
               "old_state", old_state, "new_state", new_state);

  bool storing_in_bfcache = new_state->is_in_back_forward_cache &&
                            !old_state->is_in_back_forward_cache;
  bool restoring_from_bfcache = !new_state->is_in_back_forward_cache &&
                                old_state->is_in_back_forward_cache;
  // `hiding_page` indicates that the page is switching visibility states in a
  // way that we should treat as a change.  There are two definitions of this
  // (see below), but both require that the new state is not `kVisible`.
  bool hiding_page =
      new_state->visibility != mojom::blink::PageVisibilityState::kVisible;
  if (RuntimeEnabledFeatures::DispatchHiddenVisibilityTransitionsEnabled()) {
    // Dispatch a visibility change from `kVisible` to either hidden state, and
    // also between the two hidden states.
    hiding_page &= (old_state->visibility != new_state->visibility);
  } else {
    // Dispatch a visibility change only when entering or leaving `kVisible` to
    // one of the two hidden states, but not when switching between `kHidden`
    // and `kHiddenButPainting` in either direction.
    hiding_page &=
        (old_state->visibility == mojom::blink::PageVisibilityState::kVisible);
  }
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
      IsRestoredFromBackForwardCache(old_state, new_state);
  bool eviction_changed =
      new_state->eviction_enabled != old_state->eviction_enabled;

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
    // TODO(https://crbug.com/1378279): Consider moving this to happen earlier
    // and together with other page state updates so that the ordering is clear.
    Scheduler()->SetPageBackForwardCached(new_state->is_in_back_forward_cache);
  }

  if (freezing_page) {
    // Notify all local frames that we are about to freeze.
    for (WebFrame* frame = MainFrame(); frame; frame = frame->TraverseNext()) {
      if (frame->IsWebLocalFrame()) {
        frame->ToWebLocalFrame()->Client()->WillFreezePage();
      }
    }

    // TODO(https://crbug.com/1378279): Consider moving this to happen earlier
    // and together with other page state updates so that the ordering is clear.
    SetPageFrozen(true);
  }

  if (restoring_from_bfcache) {
    DCHECK(page_restore_params);
    // Update the history offset and length value, as pages that are kept in
    // the back-forward cache do not get notified about updates on these
    // values, so the currently saved value might be stale.
    SetHistoryOffsetAndLength(page_restore_params->pending_history_list_offset,
                              page_restore_params->current_history_list_length);
  }
  if (eviction_changed)
    HookBackForwardCacheEviction(new_state->eviction_enabled);
  if (resuming_page) {
    // TODO(https://crbug.com/1378279): Consider moving this to happen earlier
    // and together with other page state updates so that the ordering is clear.
    SetPageFrozen(false);
  }
  if (showing_page) {
    SetVisibilityState(new_state->visibility, /*is_initial_state=*/false);
  }
  if (restoring_from_bfcache) {
    DCHECK(dispatching_pageshow);
    DCHECK(page_restore_params);
    // Increment the navigation counter on the main frame and all nested frames
    // in its frame tree.
    // Navigation Id increment should happen before a
    // BackForwardCacheRestoration instance is created which happens inside the
    // DispatchPageshow method.
    for (Frame* frame = page->MainFrame(); frame;
         frame = frame->Tree().TraverseNext()) {
      auto* local_frame = DynamicTo<LocalFrame>(frame);
      if (local_frame && local_frame->View()) {
        DCHECK(local_frame->DomWindow());
        local_frame->DomWindow()->GenerateNewNavigationId();
      }
    }

    DispatchPersistedPageshow(page_restore_params->navigation_start);

    // TODO(https://crbug.com/1378279): Consider moving this to happen earlier
    // and together with other page state updates so that the ordering is clear.
    Scheduler()->SetPageBackForwardCached(new_state->is_in_back_forward_cache);
    if (MainFrame()->IsWebLocalFrame()) {
      LocalFrame* local_frame = To<LocalFrame>(page->MainFrame());
      probe::DidRestoreFromBackForwardCache(local_frame);

      if (local_frame->IsOutermostMainFrame()) {
        Document* document = local_frame->GetDocument();
        if (auto* document_rules =
                DocumentSpeculationRules::FromIfExists(*document)) {
          document_rules->DocumentRestoredFromBFCache();
        }
      }
    }
  }

  // Make sure no TrackedFeaturesUpdate message is sent after the ACK
  // TODO(carlscab): Do we really need to go through LocalFrame =>
  // platform/scheduler/ => LocalFrame to report the features? We can probably
  // move SchedulerTrackedFeatures to core/ and remove the back and forth.
  ReportActiveSchedulerTrackedFeatures();

  // TODO(https://crbug.com/1378279): Consider moving this to happen earlier
  // and together with other page state updates so that the ordering is clear.
  GetPage()->SetPageLifecycleState(std::move(new_state));

  // Notify all local frames that we've updated the page lifecycle state.
  for (WebFrame* frame = MainFrame(); frame; frame = frame->TraverseNext()) {
    if (frame->IsWebLocalFrame()) {
      frame->ToWebLocalFrame()->Client()->DidSetPageLifecycleState(
          restoring_from_bfcache);
    }
  }

  UpdateViewTransitionState(restoring_from_bfcache, storing_in_bfcache,
                            page_restore_params);

  if (RuntimeEnabledFeatures::PageRevealEventEnabled()) {
    if (restoring_from_bfcache) {
      for (Frame* frame = GetPage()->MainFrame(); frame;
           frame = frame->Tree().TraverseNext()) {
        if (auto* local_frame = DynamicTo<LocalFrame>(frame)) {
          CHECK(local_frame->GetDocument());
          local_frame->GetDocument()->EnqueuePageRevealEvent();
        }
      }
    }
  }
}

void WebViewImpl::UpdateViewTransitionState(
    bool restoring_from_bfcache,
    bool storing_in_bfcache,
    const mojom::blink::PageRestoreParamsPtr& page_restore_params) {
  // If we have view_transition_state, then we must be a main frame.
  DCHECK(!page_restore_params || !page_restore_params->view_transition_state ||
         MainFrame()->IsWebLocalFrame());
  // We can't be both restoring and storing things.
  DCHECK(!restoring_from_bfcache || !storing_in_bfcache);

  if (!MainFrame()->IsWebLocalFrame()) {
    return;
  }
  LocalFrame* local_frame = To<LocalFrame>(GetPage()->MainFrame());
  DCHECK(local_frame);

  // When restoring from BFCache, start a transition if we have a view
  // transition state.
  if (restoring_from_bfcache && page_restore_params->view_transition_state) {
    if (auto* document = local_frame->GetDocument()) {
      ViewTransitionSupplement::CreateFromSnapshotForNavigation(
          *document, std::move(*page_restore_params->view_transition_state));
    }
  }

  // If we're storing the page in BFCache, abort any pending transitions. This
  // is important since when we bring the page back from BFCache, we might
  // attempt to create a transition and fail if there is one already happening.
  // Note that even if we won't be creating a transition, it's harmless to abort
  // the main frame transition when going into BFCache.
  if (storing_in_bfcache) {
    if (auto* document = local_frame->GetDocument()) {
      ViewTransitionSupplement::AbortTransition(*document);
    }
  }
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
  if (auto* widget = static_cast<WebFrameWidgetImpl*>(
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

void WebViewImpl::DispatchPersistedPageshow(base::TimeTicks navigation_start) {
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
        loader->GetTiming().SetBackForwardCacheRestoreNavigationStart(
            navigation_start);
      }
    }
    if (frame->DomWindow() && frame->DomWindow()->IsLocalDOMWindow()) {
      auto pageshow_start_time = base::TimeTicks::Now();
      LocalDOMWindow* window = frame->DomWindow()->ToLocalDOMWindow();

      window->DispatchPersistedPageshowEvent(navigation_start);

      if (RuntimeEnabledFeatures::NavigationIdEnabled(window)) {
        auto pageshow_end_time = base::TimeTicks::Now();

        WindowPerformance* performance =
            DOMWindowPerformance::performance(*window);
        DCHECK(performance);

        performance->AddBackForwardCacheRestoration(
            navigation_start, pageshow_start_time, pageshow_end_time);
      }
      if (frame->IsOutermostMainFrame()) {
        UMA_HISTOGRAM_BOOLEAN(
            "BackForwardCache.MainFrameHasPageshowListenersOnRestore",
            window->HasEventListeners(event_type_names::kPageshow));
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
  min_auto_size_ = min_size;
  max_auto_size_ = max_size;
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
  dev_tools_emulator_->SetDefaultPageScaleLimits(min_scale, max_scale);
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
    constraints.minimum_scale =
        GetPageScaleConstraintsSet().DefaultConstraints().minimum_scale;
    constraints.maximum_scale =
        GetPageScaleConstraintsSet().DefaultConstraints().maximum_scale;
  } else {
    constraints.minimum_scale = -1;
    constraints.maximum_scale = -1;
  }
  GetPage()->SetUserAgentPageScaleConstraints(constraints);
}

gfx::Size WebViewImpl::MainFrameSize() {
  // The frame size should match the viewport size at minimum scale, since the
  // viewport must always be contained by the frame.
  return gfx::ScaleToCeiledSize(size_, 1 / MinimumPageScaleFactor());
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

  if (virtual_keyboard_mode_ != description.virtual_keyboard_mode) {
    DCHECK(MainFrameImpl()->IsOutermostMainFrame());
    virtual_keyboard_mode_ = description.virtual_keyboard_mode;
    mojom::blink::LocalFrameHost& frame_host =
        MainFrameImpl()->GetFrame()->GetLocalFrameHostRemote();

    frame_host.SetVirtualKeyboardMode(virtual_keyboard_mode_);
  }

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
      GetPageScaleConstraintsSet().UserAgentConstraints().initial_scale <= 1) {
    if (description.max_width == Length::DeviceWidth() ||
        (description.max_width.IsAuto() &&
         GetPageScaleConstraintsSet().PageDefinedConstraints().initial_scale ==
             1.0f))
      SetInitialPageScaleOverride(-1);
  }

  Settings& page_settings = GetPage()->GetSettings();
  GetPageScaleConstraintsSet().AdjustForAndroidWebViewQuirks(
      description, default_min_width.IntValue(),
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

  if (does_composite_) {
    MainFrameImpl()->FrameWidgetImpl()->UpdateViewportDescription(description);
  }

  UpdateMainFrameLayoutSize();

  if (RuntimeEnabledFeatures::ViewportChangesUpdateTextAutosizingEnabled()) {
    TextAutosizer::UpdatePageInfoInAllFrames(GetPage()->MainFrame());
  }
}

void WebViewImpl::UpdateMainFrameLayoutSize() {
  if (should_auto_resize_ || !MainFrameImpl())
    return;

  LocalFrameView* view = MainFrameImpl()->GetFrameView();
  if (!view)
    return;

  gfx::Size layout_size = size_;

  if (GetSettings()->ViewportEnabled())
    layout_size = GetPageScaleConstraintsSet().GetLayoutSize();

  if (GetPage()->GetSettings().GetForceZeroLayoutHeight())
    layout_size.set_height(0);

  view->SetLayoutSize(layout_size);
}

gfx::Size WebViewImpl::ContentsSize() const {
  if (!GetPage()->MainFrame()->IsLocalFrame())
    return gfx::Size();
  auto* layout_view =
      GetPage()->DeprecatedLocalMainFrame()->ContentLayoutObject();
  if (!layout_view)
    return gfx::Size();
  return ToPixelSnappedRect(layout_view->DocumentRect()).size();
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
  // Already accounts for zoom.
  int width_scaled = document->GetLayoutView()->ComputeMinimumWidth().Round();
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

void WebViewImpl::Show(const LocalFrameToken& opener_frame_token,
                       NavigationPolicy policy,
                       const gfx::Rect& requested_rect,
                       const gfx::Rect& adjusted_rect,
                       bool opened_by_user_gesture) {
  // This is only called on local main frames.
  DCHECK(local_main_frame_host_remote_);
  DCHECK(web_widget_);
  web_widget_->SetPendingWindowRect(adjusted_rect);
  const WebWindowFeatures& web_window_features = page_->GetWindowFeatures();
  mojom::blink::WindowFeaturesPtr window_features =
      mojom::blink::WindowFeatures::New();
  window_features->bounds = requested_rect;
  window_features->has_x = web_window_features.x_set;
  window_features->has_y = web_window_features.y_set;
  window_features->has_width = web_window_features.width_set;
  window_features->has_height = web_window_features.height_set;
  window_features->is_popup = web_window_features.is_popup;
  window_features->is_partitioned_popin =
      web_window_features.is_partitioned_popin;
  local_main_frame_host_remote_->ShowCreatedWindow(
      opener_frame_token, NavigationPolicyToDisposition(policy),
      std::move(window_features), opened_by_user_gesture,
      WTF::BindOnce(&WebViewImpl::DidShowCreatedWindow, WTF::Unretained(this)));

  if (auto* dev_tools_agent =
          MainFrameImpl()->DevToolsAgentImpl(/*create_if_necessary=*/false)) {
    dev_tools_agent->DidShowNewWindow();
  }
}

void WebViewImpl::DidShowCreatedWindow() {
  web_widget_->AckPendingWindowRect();
}

void WebViewImpl::SendWindowRectToMainFrameHost(
    const gfx::Rect& bounds,
    base::OnceClosure ack_callback) {
  DCHECK(local_main_frame_host_remote_);
  local_main_frame_host_remote_->SetWindowRect(bounds, std::move(ack_callback));
}

void WebViewImpl::DidAccessInitialMainDocument() {
  DCHECK(local_main_frame_host_remote_);
  local_main_frame_host_remote_->DidAccessInitialMainDocument();
}

void WebViewImpl::Minimize() {
  DCHECK(local_main_frame_host_remote_);
  local_main_frame_host_remote_->Minimize();
}

void WebViewImpl::Maximize() {
  DCHECK(local_main_frame_host_remote_);
  local_main_frame_host_remote_->Maximize();
}

void WebViewImpl::Restore() {
  DCHECK(local_main_frame_host_remote_);
  local_main_frame_host_remote_->Restore();
}

void WebViewImpl::SetResizable(bool resizable) {
  DCHECK(local_main_frame_host_remote_);
  local_main_frame_host_remote_->SetResizable(resizable);
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
        target_url, WTF::BindOnce(&WebViewImpl::TargetURLUpdatedInBrowser,
                                  WTF::Unretained(this)));
  } else {
    DCHECK(remote_main_frame_host_remote_);
    remote_main_frame_host_remote_->UpdateTargetURL(
        target_url, WTF::BindOnce(&WebViewImpl::TargetURLUpdatedInBrowser,
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
  if (compositor_device_scale_factor_override_ != device_scale_factor) {
    compositor_device_scale_factor_override_ = device_scale_factor;
    UpdateWidgetZoomFactors();
    UpdateInspectorDeviceScaleFactorOverride();
  }
}

void WebViewImpl::SetDeviceEmulationTransform(const gfx::Transform& transform) {
  if (transform == device_emulation_transform_)
    return;
  device_emulation_transform_ = transform;
  UpdateDeviceEmulationTransform();
}

gfx::Transform WebViewImpl::GetDeviceEmulationTransform() const {
  return device_emulation_transform_;
}

void WebViewImpl::EnableDeviceEmulation(const DeviceEmulationParams& params) {
  web_widget_->EnableDeviceEmulation(params);
}

void WebViewImpl::ActivateDevToolsTransform(
    const DeviceEmulationParams& params) {
  gfx::Transform device_emulation_transform =
      dev_tools_emulator_->EnableDeviceEmulation(params);
  SetDeviceEmulationTransform(device_emulation_transform);
}

void WebViewImpl::DisableDeviceEmulation() {
  web_widget_->DisableDeviceEmulation();
}

void WebViewImpl::DeactivateDevToolsTransform() {
  dev_tools_emulator_->DisableDeviceEmulation();
  SetDeviceEmulationTransform(gfx::Transform());
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

SkColor WebViewImpl::BackgroundColor() const {
  if (background_color_override_for_fullscreen_controller_)
    return background_color_override_for_fullscreen_controller_.value();
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
  if (override_base_background_color_to_transparent_)
    return Color::kTransparent;
  // TODO(https://crbug.com/1351544): The base background color override should
  // be an SkColor4f or a Color.
  if (base_background_color_override_for_inspector_) {
    return Color::FromSkColor(
        base_background_color_override_for_inspector_.value());
  }
  // Use the page background color if this is the WebView of the main frame.
  if (MainFrameImpl())
    return Color::FromSkColor(page_base_background_color_);
  return Color::kWhite;
}

void WebViewImpl::SetPageBaseBackgroundColor(std::optional<SkColor> color) {
  SkColor new_color = color.value_or(SK_ColorWHITE);
  if (page_base_background_color_ == new_color)
    return;
  page_base_background_color_ = new_color;
  UpdateBaseBackgroundColor();
}

void WebViewImpl::UpdateColorProviders(
    const ColorProviderColorMaps& color_provider_colors) {
  bool color_providers_did_change =
      page_->UpdateColorProviders(color_provider_colors);
  if (color_providers_did_change) {
    Page::ForcedColorsChanged();
  }
}

void WebViewImpl::SetBaseBackgroundColorOverrideTransparent(
    bool override_to_transparent) {
  DCHECK(does_composite_);
  if (override_base_background_color_to_transparent_ == override_to_transparent)
    return;
  override_base_background_color_to_transparent_ = override_to_transparent;
  UpdateBaseBackgroundColor();
}

void WebViewImpl::SetBaseBackgroundColorOverrideForInspector(
    std::optional<SkColor> optional_color) {
  if (base_background_color_override_for_inspector_ == optional_color)
    return;
  base_background_color_override_for_inspector_ = optional_color;
  UpdateBaseBackgroundColor();
}

void WebViewImpl::UpdateBaseBackgroundColor() {
  if (MainFrameImpl()) {
    // Force lifecycle update to ensure we're good to call
    // LocalFrameView::setBaseBackgroundColor().
    MainFrameImpl()->GetFrame()->View()->UpdateAllLifecyclePhasesExceptPaint(
        DocumentUpdateReason::kBaseColor);
  }

  Color color = BaseBackgroundColor();
  if (auto* local_frame = DynamicTo<LocalFrame>(page_->MainFrame())) {
    LocalFrameView* view = local_frame->View();
    view->UpdateBaseBackgroundColorRecursively(color);
  }
}

void WebViewImpl::UpdateFontRenderingFromRendererPrefs() {
#if !BUILDFLAG(IS_MAC)
  skia::LegacyDisplayGlobals::SetCachedParams(
      gfx::FontRenderParams::SubpixelRenderingToSkiaPixelGeometry(
          renderer_preferences_.subpixel_rendering),
      renderer_preferences_.text_contrast, renderer_preferences_.text_gamma);
#if BUILDFLAG(IS_WIN)
  // Cache the system font metrics in blink.
  WebFontRendering::SetMenuFontMetrics(
      WebString::FromUTF16(renderer_preferences_.menu_font_family_name),
      renderer_preferences_.menu_font_height);
  WebFontRendering::SetSmallCaptionFontMetrics(
      WebString::FromUTF16(
          renderer_preferences_.small_caption_font_family_name),
      renderer_preferences_.small_caption_font_height);
  WebFontRendering::SetStatusFontMetrics(
      WebString::FromUTF16(renderer_preferences_.status_font_family_name),
      renderer_preferences_.status_font_height);
  WebFontRendering::SetAntialiasedTextEnabled(
      renderer_preferences_.should_antialias_text);
  WebFontRendering::SetLCDTextEnabled(
      renderer_preferences_.subpixel_rendering !=
      gfx::FontRenderParams::SUBPIXEL_RENDERING_NONE);
#else
  WebFontRenderStyle::SetHinting(
      RendererPreferencesToSkiaHinting(renderer_preferences_));
  WebFontRenderStyle::SetAutoHint(renderer_preferences_.use_autohinter);
  WebFontRenderStyle::SetUseBitmaps(renderer_preferences_.use_bitmaps);
  WebFontRenderStyle::SetAntiAlias(renderer_preferences_.should_antialias_text);
  WebFontRenderStyle::SetSubpixelRendering(
      renderer_preferences_.subpixel_rendering !=
      gfx::FontRenderParams::SUBPIXEL_RENDERING_NONE);
  WebFontRenderStyle::SetSubpixelPositioning(
      renderer_preferences_.use_subpixel_positioning);
// TODO(crbug.com/1052397): Revisit once build flag switch of lacros-chrome is
// complete.
#if (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)) && \
    !BUILDFLAG(IS_ANDROID)
  if (!renderer_preferences_.system_font_family_name.empty()) {
    WebFontRenderStyle::SetSystemFontFamily(blink::WebString::FromUTF8(
        renderer_preferences_.system_font_family_name));
  }
#endif  // (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)) &&
        // !BUILDFLAG(IS_ANDROID)
#endif  // BUILDFLAG(IS_WIN)
#endif  // !BUILDFLAG(IS_MAC)
}

void WebViewImpl::ActivatePrerenderedPage(
    mojom::blink::PrerenderPageActivationParamsPtr
        prerender_page_activation_params,
    ActivatePrerenderedPageCallback callback) {
  TRACE_EVENT0("navigation", "WebViewImpl::ActivatePrerenderedPage");

  // From here all new documents will have prerendering false.
  GetPage()->SetIsPrerendering(false);

  // Collect local documents. This is because we are about to run the
  // prerenderchange event and post-prerendering activation steps on each
  // document, which could mutate the frame tree and make iteration over it
  // complicated.
  HeapVector<Member<Document>> child_frame_documents;
  Member<Document> main_frame_document;
  if (auto* local_frame = DynamicTo<LocalFrame>(GetPage()->MainFrame())) {
    main_frame_document = local_frame->GetDocument();
  }
  if (main_frame_document) {
    RecordPrerenderActivationSignalDelay(GetPage()->PrerenderMetricSuffix());
  }

  for (Frame* frame = GetPage()->MainFrame(); frame;
       frame = frame->Tree().TraverseNext()) {
    if (auto* local_frame = DynamicTo<LocalFrame>(frame)) {
      if (local_frame->GetDocument() != main_frame_document) {
        child_frame_documents.push_back(local_frame->GetDocument());
      }
    }
  }

  // A null `activation_start` is sent to the WebViewImpl that does not host the
  // main frame, in which case we expect that it does not have any documents
  // since cross-origin documents are not loaded during prerendering.
  DCHECK((!main_frame_document && child_frame_documents.size() == 0) ||
         !prerender_page_activation_params->activation_start.is_null());
  // We also only send view_transition_state to the main frame.
  DCHECK(main_frame_document ||
         !prerender_page_activation_params->view_transition_state);

  if (main_frame_document) {
    main_frame_document->ActivateForPrerendering(
        *prerender_page_activation_params);
    prerender_page_activation_params->view_transition_state.reset();
  }

  // While the spec says to post a task on the networking task source for each
  // document, we don't post a task here for simplicity. This allows dispatching
  // the event on all documents without a chance for other IPCs from the browser
  // to arrive in the intervening time, resulting in an unclear state.
  for (auto& document : child_frame_documents) {
    document->ActivateForPrerendering(*prerender_page_activation_params);
  }

  std::move(callback).Run();
}

void WebViewImpl::RegisterRendererPreferenceWatcher(
    CrossVariantMojoRemote<mojom::RendererPreferenceWatcherInterfaceBase>
        watcher) {
  renderer_preference_watchers_.Add(std::move(watcher));
}

void WebViewImpl::SetRendererPreferences(
    const RendererPreferences& preferences) {
  UpdateRendererPreferences(preferences);
}

const RendererPreferences& WebViewImpl::GetRendererPreferences() const {
  return renderer_preferences_;
}

void WebViewImpl::UpdateRendererPreferences(
    const RendererPreferences& preferences) {
  std::string old_accept_languages = renderer_preferences_.accept_languages;
  renderer_preferences_ = preferences;

  for (auto& watcher : renderer_preference_watchers_)
    watcher->NotifyUpdate(renderer_preferences_);

  WebThemeEngineHelper::DidUpdateRendererPreferences(preferences);
  UpdateFontRenderingFromRendererPrefs();

  blink::SetCaretBlinkInterval(
      renderer_preferences_.caret_blink_interval.has_value()
          ? renderer_preferences_.caret_blink_interval.value()
          : base::Milliseconds(
                mojom::blink::kDefaultCaretBlinkIntervalInMilliseconds));

#if defined(USE_AURA)
  if (renderer_preferences_.use_custom_colors) {
    SetFocusRingColor(renderer_preferences_.focus_ring_color);
    SetSelectionColors(renderer_preferences_.active_selection_bg_color,
                       renderer_preferences_.active_selection_fg_color,
                       renderer_preferences_.inactive_selection_bg_color,
                       renderer_preferences_.inactive_selection_fg_color);
    ThemeChanged();
  }
#endif

  if (renderer_preferences_.use_custom_colors) {
    SetFocusRingColor(renderer_preferences_.focus_ring_color);
  }

  if (old_accept_languages != renderer_preferences_.accept_languages)
    AcceptLanguagesChanged();

  GetSettings()->SetCaretBrowsingEnabled(
      renderer_preferences_.caret_browsing_enabled);

#if BUILDFLAG(IS_OZONE)
  GetSettings()->SetSelectionClipboardBufferAvailable(
      renderer_preferences_.selection_clipboard_buffer_available);
#endif  // BUILDFLAG(IS_OZONE)

  SetExplicitlyAllowedPorts(
      renderer_preferences_.explicitly_allowed_network_ports);

  if (renderer_preferences_.prefixed_fullscreen_video_api_availability
          .has_value()) {
    WebRuntimeFeatures::EnableFeatureFromString(
        "PrefixedVideoFullscreen",
        renderer_preferences_.prefixed_fullscreen_video_api_availability
            .value());
  }

  MaybePreloadSystemFonts(GetPage());
}

void WebViewImpl::SetHistoryOffsetAndLength(int32_t history_offset,
                                            int32_t history_length) {
  // -1 <= history_offset < history_length <= kMaxSessionHistoryEntries.
  DCHECK_LE(-1, history_offset);
  DCHECK_LT(history_offset, history_length);
  DCHECK_LE(history_length, kMaxSessionHistoryEntries);

  history_list_offset_ = history_offset;
  history_list_length_ = history_length;
}

void WebViewImpl::SetHistoryListFromNavigation(
    int32_t history_offset,
    std::optional<int32_t> history_length) {
  if (!history_length.has_value()) {
    history_list_offset_ = history_offset;
    return;
  }

  SetHistoryOffsetAndLength(history_offset, *history_length);
}

void WebViewImpl::IncreaseHistoryListFromNavigation() {
  // Advance our offset in session history, applying the length limit.
  // There is now no forward history.
  history_list_offset_ =
      std::min(history_list_offset_ + 1, kMaxSessionHistoryEntries - 1);
  history_list_length_ = history_list_offset_ + 1;
}

int32_t WebViewImpl::HistoryBackListCount() const {
  return std::max(history_list_offset_, 0);
}

int32_t WebViewImpl::HistoryForwardListCount() const {
  return history_list_length_ - HistoryBackListCount() - 1;
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

  if (IsFencedFrameRoot()) {
    // The main frame of a fenced frame should not behave like a top level
    // frame in terms of viewport behavior. i.e. It shouldn't allow zooming,
    // either explicitly or to fit content, and it should not interpret the
    // viewport <meta> tag. Text autosizing is disabled since it is only
    // determined by the outermost page and having the outermost page pass
    // it into the fenced frame can create a communication channel.
    web_preferences_.viewport_enabled = false;
    web_preferences_.viewport_meta_enabled = false;
    web_preferences_.default_minimum_page_scale_factor = 1.f;
    web_preferences_.default_maximum_page_scale_factor = 1.f;
    web_preferences_.shrinks_viewport_contents_to_fit = false;
    web_preferences_.main_frame_resizes_are_orientation_changes = false;
    web_preferences_.text_autosizing_enabled = false;

    // Insecure content should not be allowed in a fenced frame.
    web_preferences_.allow_running_insecure_content = false;

#if BUILDFLAG(IS_ANDROID)
    // Reusing the global for unowned main frame is only used for
    // Android WebView. Since this is a fenced frame it is not the
    // outermost main frame so we can safely disable this feature.
    web_preferences_.reuse_global_for_unowned_main_frame = false;
#endif
  }

  if (MainFrameImpl()) {
    MainFrameImpl()->FrameWidgetImpl()->SetPrefersReducedMotion(
        web_preferences_.prefers_reduced_motion);
  }

  ApplyWebPreferences(web_preferences_, this);
  ApplyCommandLineToSettings(SettingsImpl());
}

void WebViewImpl::AddObserver(WebViewObserver* observer) {
  observers_.AddObserver(observer);
}

void WebViewImpl::RemoveObserver(WebViewObserver* observer) {
  observers_.RemoveObserver(observer);
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
    if (web_widget_)
      web_widget_->ResetMeaningfulLayoutStateForMainFrame();

    if (is_new_navigation)
      GetPageScaleConstraintsSet().SetNeedsReset(true);
  }

  // Give the visual viewport's scroll layer its initial size.
  GetPage()->GetVisualViewport().MainFrameDidChangeSize();
}

void WebViewImpl::DidCommitCompositorFrameForLocalMainFrame() {
  for (auto& observer : observers_)
    observer.DidCommitCompositorFrame();
}

void WebViewImpl::ResizeAfterLayout() {
  DCHECK(MainFrameImpl());

  if (!web_view_client_)
    return;

  if (should_auto_resize_) {
    LocalFrameView* view = MainFrameImpl()->GetFrame()->View();
    gfx::Size frame_size = view->Size();
    if (frame_size != size_) {
      size_ = frame_size;

      GetPage()->GetVisualViewport().SetSize(size_);
      GetPageScaleConstraintsSet().DidChangeInitialContainingBlockSize(size_);

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

  for (auto& observer : observers_)
    observer.DidUpdateMainFrameLayout();
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
  DCHECK(viewport.IsActiveViewport());
  MainFrameImpl()->FrameWidgetImpl()->SetPageScaleStateAndLimits(
      viewport.Scale(), viewport.IsPinchGestureActive(),
      MinimumPageScaleFactor(), MaximumPageScaleFactor());

  local_main_frame_host_remote_->ScaleFactorChanged(viewport.Scale());

  if (dev_tools_emulator_->HasViewportOverride()) {
    // TODO(bokan): Can HasViewportOverride be set on a nested main frame? If
    // not, we can enforce that when setting it and DCHECK IsOutermostMainFrame
    // instead.
    if (MainFrameImpl()->IsOutermostMainFrame()) {
      gfx::Transform device_emulation_transform =
          dev_tools_emulator_->OutermostMainFrameScrollOrScaleChanged();
      SetDeviceEmulationTransform(device_emulation_transform);
    }
  }
}

void WebViewImpl::OutermostMainFrameScrollOffsetChanged() {
  DCHECK(MainFrameImpl());
  DCHECK(MainFrameImpl()->IsOutermostMainFrame());
  if (dev_tools_emulator_->HasViewportOverride()) {
    gfx::Transform device_emulation_transform =
        dev_tools_emulator_->OutermostMainFrameScrollOrScaleChanged();
    SetDeviceEmulationTransform(device_emulation_transform);
  }
}

void WebViewImpl::TextAutosizerPageInfoChanged(
    const mojom::blink::TextAutosizerPageInfo& page_info) {
  DCHECK(MainFrameImpl());
  local_main_frame_host_remote_->TextAutosizerPageInfoChanged(
      page_info.Clone());
}

void WebViewImpl::SetBackgroundColorOverrideForFullscreenController(
    std::optional<SkColor> optional_color) {
  DCHECK(does_composite_);

  background_color_override_for_fullscreen_controller_ = optional_color;
  if (MainFrameImpl()) {
    MainFrameImpl()->FrameWidgetImpl()->SetBackgroundColor(BackgroundColor());
  }
}

void WebViewImpl::SetZoomFactorOverride(float zoom_factor) {
  zoom_factor_override_ = zoom_factor;
  // This only affects the local main frame, so no need to propagate to all
  // frame widgets.
  if (web_widget_) {
    web_widget_->SetZoomLevel(web_widget_->GetZoomLevel());
  }
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
  tap_event.SetPositionInWidget(gfx::PointF(tap_point_window_pos));
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

  result.SetToShadowHostIfInUAShadowRoot();
  return result;
}

void WebViewImpl::SetTabsToLinks(bool enable) {
  tabs_to_links_ = enable;
}

bool WebViewImpl::TabsToLinks() const {
  return tabs_to_links_;
}

void WebViewImpl::DidChangeRootLayer(bool root_layer_exists) {
  // The Layer is removed when the main frame's `Document` changes. It also is
  // removed when the whole `LocalFrame` goes away, in which case we don't
  // need to DeferMainFrameUpdate() as we will do so if a local MainFrame is
  // attached in the future.
  if (!MainFrameImpl()) {
    DCHECK(!root_layer_exists);
    return;
  }
  if (root_layer_exists) {
    if (!device_emulation_transform_.IsIdentity())
      UpdateDeviceEmulationTransform();
  } else if (!MainFrameImpl()->FrameWidgetImpl()->WillBeDestroyed()) {
    // When the document in an already-attached main frame is being replaced
    // by a navigation then DidChangeRootLayer(false) will be called. Since we
    // are navigating, defer BeginMainFrames until the new document is ready
    // for them.
    //
    // If WillBeDestroyed() is true, it means we're swapping the frame as well
    // as the document for this navigation. BeginMainFrames are instead
    // deferred for a newly attached frame via DidAttachLocalMainFrame(). See
    // crbug.com/936696.
    scoped_defer_main_frame_update_ =
        MainFrameImpl()->FrameWidgetImpl()->DeferMainFrameUpdate();
  }
}

void WebViewImpl::InvalidateContainer() {
  // This is only for non-composited WebViewPlugin.
  if (!does_composite_ && web_view_client_)
    web_view_client_->InvalidateContainer();
}

void WebViewImpl::ApplyViewportChanges(const ApplyViewportChangesArgs& args) {
  // TODO(https://crbug.com/1160652): Figure out if Page is null.
  CHECK(page_);

  VisualViewport& visual_viewport = GetPage()->GetVisualViewport();
  DCHECK(visual_viewport.IsActiveViewport());

  // Store the desired offsets the visual viewport before setting the top
  // controls ratio since doing so will change the bounds and move the
  // viewports to keep the offsets valid. The compositor may have already
  // done that so we don't want to double apply the deltas here.
  gfx::PointF visual_viewport_offset = visual_viewport.VisibleRect().origin();
  visual_viewport_offset.Offset(args.inner_delta.x(), args.inner_delta.y());

  GetBrowserControls().SetShownRatio(
      GetBrowserControls().TopShownRatio() + args.top_controls_delta,
      GetBrowserControls().BottomShownRatio() + args.bottom_controls_delta);

  SetPageScaleFactorAndLocation(PageScaleFactor() * args.page_scale_delta,
                                args.is_pinch_gesture_active,
                                visual_viewport_offset);

  if (args.page_scale_delta != 1) {
    double_tap_zoom_pending_ = false;
  }

  elastic_overscroll_ += args.elastic_overscroll_delta;
  UpdateBrowserControlsConstraint(args.browser_controls_constraint);

  if (args.scroll_gesture_did_end) {
    // TODO(https://crbug.com/1160652): Figure out if MainFrameImpl is null.
    CHECK(MainFrameImpl());
    MainFrameImpl()->GetFrame()->GetEventHandler().MarkHoverStateDirty();
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

void WebViewImpl::UpdateDeviceEmulationTransform() {
  if (GetPage()->GetVisualViewport().IsActiveViewport())
    GetPage()->GetVisualViewport().SetNeedsPaintPropertyUpdate();

  if (auto* main_frame = MainFrameImpl()) {
    // When the device emulation transform is updated, to avoid incorrect
    // scales and fuzzy raster from the compositor, force all content to
    // pick ideal raster scales.
    // TODO(wjmaclean): This is only done on the main frame's widget currently,
    // it should update all local frames.
    main_frame->FrameWidgetImpl()->SetNeedsRecalculateRasterScales();

    // Device emulation transform also affects the overriding visible rect
    // which is used as the overflow rect of the main frame layout view.
    if (auto* view = main_frame->GetFrameView())
      view->SetNeedsPaintPropertyUpdate();
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
  GetPage()->SetVisibilityState(visibility_state, is_initial_state);
  // Do not throttle if the page should be painting.
  bool is_visible =
      visibility_state == mojom::blink::PageVisibilityState::kVisible;
  if (RuntimeEnabledFeatures::DispatchHiddenVisibilityTransitionsEnabled()) {
    // Treat `kHiddenButPainting` as visible for page scheduling; we don't want
    // to throttle timers, etc.
    is_visible |= visibility_state ==
                  mojom::blink::PageVisibilityState::kHiddenButPainting;
  }
  GetPage()->GetPageScheduler()->SetPageVisible(is_visible);
  // Notify observers of the change.
  if (!is_initial_state) {
    for (auto& observer : observers_)
      observer.OnPageVisibilityChanged(visibility_state);
  }
}

mojom::blink::PageVisibilityState WebViewImpl::GetVisibilityState() {
  DCHECK(GetPage());
  return GetPage()->GetVisibilityState();
}

LocalFrame* WebViewImpl::FocusedLocalFrameInWidget() const {
  if (!MainFrameImpl())
    return nullptr;

  auto* focused_frame = To<LocalFrame>(FocusedCoreFrame());
  if (focused_frame->LocalFrameRoot() != MainFrameImpl()->GetFrame())
    return nullptr;
  return focused_frame;
}

void WebViewImpl::SetPageFrozen(bool frozen) {
  Scheduler()->SetPageFrozen(frozen);
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

int32_t WebViewImpl::AutoplayFlagsForTest() const {
  return page_->AutoplayFlags();
}

gfx::Size WebViewImpl::GetPreferredSizeForTest() {
  return preferred_size_in_dips_;
}

void WebViewImpl::StopDeferringMainFrameUpdate() {
  scoped_defer_main_frame_update_ = nullptr;
}

void WebViewImpl::SetDeviceColorSpaceForTesting(
    const gfx::ColorSpace& color_space) {
  web_widget_->SetDeviceColorSpaceForTesting(color_space);
}

const SessionStorageNamespaceId& WebViewImpl::GetSessionStorageNamespaceId() {
  CHECK(!session_storage_namespace_id_.empty());
  return session_storage_namespace_id_;
}

bool WebViewImpl::IsFencedFrameRoot() const {
  return GetPage()->IsMainFrameFencedFrameRoot();
}

void WebViewImpl::SetSupportsDraggableRegions(bool supports_draggable_regions) {
  supports_draggable_regions_ = supports_draggable_regions;
  if (!MainFrameImpl() || !MainFrameImpl()->GetFrame()) {
    return;
  }

  LocalFrame* local_frame = MainFrameImpl()->GetFrame();

  if (supports_draggable_regions_) {
    local_frame->View()->UpdateDocumentDraggableRegions();
  } else {
    local_frame->GetDocument()->SetDraggableRegions(
        Vector<DraggableRegionValue>());
    chrome_client_->DraggableRegionsChanged();
  }
}

bool WebViewImpl::SupportsDraggableRegions() {
  return supports_draggable_regions_;
}

void WebViewImpl::DraggableRegionsChanged() {
  if (!MainFrameImpl()) {
    return;
  }

  WebVector<WebDraggableRegion> web_regions =
      MainFrameImpl()->GetDocument().DraggableRegions();

  // If |supports_draggable_regions_| is false, the web view should only send
  // empty regions to reset a previously set draggable regions.
  DCHECK(supports_draggable_regions_ || web_regions.empty());

  auto regions = Vector<mojom::blink::DraggableRegionPtr>();
  for (WebDraggableRegion& web_region : web_regions) {
    auto converted_bounds =
        MainFrame()->ToWebLocalFrame()->FrameWidget()->BlinkSpaceToEnclosedDIPs(
            web_region.bounds);

    auto region = mojom::blink::DraggableRegion::New(converted_bounds,
                                                     web_region.draggable);
    regions.emplace_back(std::move(region));
  }

  local_main_frame_host_remote_->DraggableRegionsChanged(std::move(regions));
}

void WebViewImpl::MojoDisconnected() {
#if !(BUILDFLAG(IS_ANDROID) || \
      (BUILDFLAG(IS_CHROMEOS) && defined(ARCH_CPU_ARM64)))
  auto prev_close_task_trace = close_task_posted_stack_trace_;
  base::debug::Alias(&prev_close_task_trace);
  close_task_posted_stack_trace_.emplace();
  auto cur_close_task_trace = close_task_posted_stack_trace_;
  base::debug::Alias(&cur_close_task_trace);
  auto close_trace = close_called_stack_trace_;
  base::debug::Alias(&close_trace);
  auto close_window_trace = close_window_called_stack_trace_;
  base::debug::Alias(&close_window_trace);
#endif
  // This IPC can be called from re-entrant contexts. We can't destroy a
  // RenderViewImpl while references still exist on the stack, so we dispatch a
  // non-nestable task. This method is called exactly once by the browser
  // process, and is used to release ownership of the corresponding
  // RenderViewImpl instance. https://crbug.com/1000035.
  GetPage()->GetAgentGroupScheduler().DefaultTaskRunner()->PostNonNestableTask(
      FROM_HERE, WTF::BindOnce(&WebViewImpl::Close, WTF::Unretained(this)));
}

void WebViewImpl::CreateRemoteMainFrame(
    const RemoteFrameToken& frame_token,
    const std::optional<FrameToken>& opener_frame_token,
    mojom::blink::FrameReplicationStatePtr replicated_state,
    bool is_loading,
    const base::UnguessableToken& devtools_frame_token,
    mojom::blink::RemoteFrameInterfacesFromBrowserPtr remote_frame_interfaces,
    mojom::blink::RemoteMainFrameInterfacesPtr remote_main_frame_interfaces) {
  blink::WebFrame* opener = nullptr;
  if (opener_frame_token)
    opener = WebFrame::FromFrameToken(*opener_frame_token);
  // Create a top level WebRemoteFrame.
  WebRemoteFrameImpl::CreateMainFrame(
      this, frame_token, is_loading, devtools_frame_token, opener,
      std::move(remote_frame_interfaces->frame_host),
      std::move(remote_frame_interfaces->frame_receiver),
      std::move(replicated_state));
  // Root frame proxy has no ancestors to point to their RenderWidget.

  // The WebRemoteFrame created here was already attached to the Page as its
  // main frame, so we can call WebView's DidAttachRemoteMainFrame().
  DidAttachRemoteMainFrame(
      std::move(remote_main_frame_interfaces->main_frame_host),
      std::move(remote_main_frame_interfaces->main_frame));
}

scheduler::WebAgentGroupScheduler& WebViewImpl::GetWebAgentGroupScheduler() {
  return web_agent_group_scheduler_;
}

void WebViewImpl::UpdatePageBrowsingContextGroup(
    const BrowsingContextGroupInfo& browsing_context_group_info) {
  Page* page = GetPage();
  CHECK(page);

  page->UpdateBrowsingContextGroup(browsing_context_group_info);
}

void WebViewImpl::SetPageAttributionSupport(
    network::mojom::AttributionSupport support) {
  Page* page = GetPage();
  CHECK(page);

  page->SetAttributionSupport(support);
}
}  // namespace blink
