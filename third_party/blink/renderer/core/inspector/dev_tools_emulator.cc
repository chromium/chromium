// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/inspector/dev_tools_emulator.h"

#include <algorithm>

#include "third_party/blink/public/web/web_settings.h"
#include "third_party/blink/renderer/core/events/web_input_event_conversion.h"
#include "third_party/blink/renderer/core/exported/web_view_impl.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/scroll/scrollbar_theme.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/geometry/float_rect.h"
#include "third_party/blink/renderer/platform/geometry/float_size.h"
#include "third_party/blink/renderer/platform/geometry/int_rect.h"
#include "third_party/blink/renderer/platform/geometry/int_size.h"
#include "third_party/blink/renderer/platform/graphics/graphics_layer.h"
#include "third_party/blink/renderer/platform/loader/fetch/memory_cache.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace {

static float calculateDeviceScaleAdjustment(int width,
                                            int height,
                                            float deviceScaleFactor) {
  // Chromium on Android uses a device scale adjustment for fonts used in text
  // autosizing for improved legibility. This function computes this adjusted
  // value for text autosizing.
  // For a description of the Android device scale adjustment algorithm, see:
  // chrome/browser/chrome_content_browser_client.cc,
  // GetDeviceScaleAdjustment(...)
  if (!width || !height || !deviceScaleFactor)
    return 1;

  static const float kMinFSM = 1.05f;
  static const int kWidthForMinFSM = 320;
  static const float kMaxFSM = 1.3f;
  static const int kWidthForMaxFSM = 800;

  float minWidth = std::min(width, height) / deviceScaleFactor;
  if (minWidth <= kWidthForMinFSM)
    return kMinFSM;
  if (minWidth >= kWidthForMaxFSM)
    return kMaxFSM;

  // The font scale multiplier varies linearly between kMinFSM and kMaxFSM.
  float ratio = static_cast<float>(minWidth - kWidthForMinFSM) /
                (kWidthForMaxFSM - kWidthForMinFSM);
  return ratio * (kMaxFSM - kMinFSM) + kMinFSM;
}

}  // namespace

namespace blink {

DevToolsEmulator::DevToolsEmulator(WebViewImpl* web_view)
    : web_view_(web_view),
      device_metrics_enabled_(false),
      emulate_mobile_enabled_(false),
      is_overlay_scrollbars_enabled_(false),
      is_orientation_event_enabled_(false),
      is_mobile_layout_theme_enabled_(false),
      original_default_minimum_page_scale_factor_(0),
      original_default_maximum_page_scale_factor_(0),
      embedder_text_autosizing_enabled_(
          web_view->GetPage()->GetSettings().TextAutosizingEnabled()),
      embedder_device_scale_adjustment_(
          web_view->GetPage()->GetSettings().GetDeviceScaleAdjustment()),
      embedder_prefer_compositing_to_lcd_text_enabled_(
          web_view->GetPage()
              ->GetSettings()
              .GetPreferCompositingToLCDTextEnabled()),
      embedder_viewport_style_(
          web_view->GetPage()->GetSettings().GetViewportStyle()),
      embedder_plugins_enabled_(
          web_view->GetPage()->GetSettings().GetPluginsEnabled()),
      embedder_available_pointer_types_(
          web_view->GetPage()->GetSettings().GetAvailablePointerTypes()),
      embedder_primary_pointer_type_(
          web_view->GetPage()->GetSettings().GetPrimaryPointerType()),
      embedder_available_hover_types_(
          web_view->GetPage()->GetSettings().GetAvailableHoverTypes()),
      embedder_primary_hover_type_(
          web_view->GetPage()->GetSettings().GetPrimaryHoverType()),
      embedder_main_frame_resizes_are_orientation_changes_(
          web_view->GetPage()
              ->GetSettings()
              .GetMainFrameResizesAreOrientationChanges()),
      touch_event_emulation_enabled_(false),
      double_tap_to_zoom_enabled_(false),
      original_max_touch_points_(0),
      embedder_script_enabled_(
          web_view->GetPage()->GetSettings().GetScriptEnabled()),
      script_execution_disabled_(false),
      embedder_hide_scrollbars_(
          web_view->GetPage()->GetSettings().GetHideScrollbars()),
      scrollbars_hidden_(false),
      embedder_cookie_enabled_(
          web_view->GetPage()->GetSettings().GetCookieEnabled()),
      document_cookie_disabled_(false) {}

DevToolsEmulator::~DevToolsEmulator() = default;

void DevToolsEmulator::Trace(blink::Visitor* visitor) {}

void DevToolsEmulator::SetTextAutosizingEnabled(bool enabled) {
  embedder_text_autosizing_enabled_ = enabled;
  bool emulate_mobile_enabled =
      device_metrics_enabled_ && emulate_mobile_enabled_;
  if (!emulate_mobile_enabled)
    web_view_->GetPage()->GetSettings().SetTextAutosizingEnabled(enabled);
}

void DevToolsEmulator::SetDeviceScaleAdjustment(float device_scale_adjustment) {
  embedder_device_scale_adjustment_ = device_scale_adjustment;
  bool emulate_mobile_enabled =
      device_metrics_enabled_ && emulate_mobile_enabled_;
  if (!emulate_mobile_enabled) {
    web_view_->GetPage()->GetSettings().SetDeviceScaleAdjustment(
        device_scale_adjustment);
  }
}

void DevToolsEmulator::SetPreferCompositingToLCDTextEnabled(bool enabled) {
  if (embedder_prefer_compositing_to_lcd_text_enabled_ == enabled)
    return;

  embedder_prefer_compositing_to_lcd_text_enabled_ = enabled;
  bool emulate_mobile_enabled =
      device_metrics_enabled_ && emulate_mobile_enabled_;
  if (!emulate_mobile_enabled) {
    web_view_->GetPage()->GetSettings().SetPreferCompositingToLCDTextEnabled(
        enabled);
  }
}

void DevToolsEmulator::SetViewportStyle(WebViewportStyle style) {
  embedder_viewport_style_ = style;
  bool emulate_mobile_enabled =
      device_metrics_enabled_ && emulate_mobile_enabled_;
  if (!emulate_mobile_enabled)
    web_view_->GetPage()->GetSettings().SetViewportStyle(style);
}

void DevToolsEmulator::SetPluginsEnabled(bool enabled) {
  embedder_plugins_enabled_ = enabled;
  bool emulate_mobile_enabled =
      device_metrics_enabled_ && emulate_mobile_enabled_;
  if (!emulate_mobile_enabled)
    web_view_->GetPage()->GetSettings().SetPluginsEnabled(enabled);
}

void DevToolsEmulator::SetScriptEnabled(bool enabled) {
  embedder_script_enabled_ = enabled;
  if (!script_execution_disabled_)
    web_view_->GetPage()->GetSettings().SetScriptEnabled(enabled);
}

void DevToolsEmulator::SetHideScrollbars(bool hide) {
  embedder_hide_scrollbars_ = hide;
  if (!scrollbars_hidden_)
    web_view_->GetPage()->GetSettings().SetHideScrollbars(hide);
}

void DevToolsEmulator::SetCookieEnabled(bool enabled) {
  embedder_cookie_enabled_ = enabled;
  if (!document_cookie_disabled_)
    web_view_->GetPage()->GetSettings().SetCookieEnabled(enabled);
}

void DevToolsEmulator::SetDoubleTapToZoomEnabled(bool enabled) {
  double_tap_to_zoom_enabled_ = enabled;
}

bool DevToolsEmulator::DoubleTapToZoomEnabled() const {
  return touch_event_emulation_enabled_ ? true : double_tap_to_zoom_enabled_;
}

void DevToolsEmulator::SetMainFrameResizesAreOrientationChanges(bool value) {
  embedder_main_frame_resizes_are_orientation_changes_ = value;
  bool emulate_mobile_enabled =
      device_metrics_enabled_ && emulate_mobile_enabled_;
  if (!emulate_mobile_enabled) {
    web_view_->GetPage()
        ->GetSettings()
        .SetMainFrameResizesAreOrientationChanges(value);
  }
}

void DevToolsEmulator::SetAvailablePointerTypes(int types) {
  embedder_available_pointer_types_ = types;
  if (!touch_event_emulation_enabled_)
    web_view_->GetPage()->GetSettings().SetAvailablePointerTypes(types);
}

void DevToolsEmulator::SetPrimaryPointerType(PointerType pointer_type) {
  embedder_primary_pointer_type_ = pointer_type;
  if (!touch_event_emulation_enabled_)
    web_view_->GetPage()->GetSettings().SetPrimaryPointerType(pointer_type);
}

void DevToolsEmulator::SetAvailableHoverTypes(int types) {
  embedder_available_hover_types_ = types;
  if (!touch_event_emulation_enabled_)
    web_view_->GetPage()->GetSettings().SetAvailableHoverTypes(types);
}

void DevToolsEmulator::SetPrimaryHoverType(HoverType hover_type) {
  embedder_primary_hover_type_ = hover_type;
  if (!touch_event_emulation_enabled_)
    web_view_->GetPage()->GetSettings().SetPrimaryHoverType(hover_type);
}

TransformationMatrix DevToolsEmulator::EnableDeviceEmulation(
    const WebDeviceEmulationParams& params) {
  if (device_metrics_enabled_ &&
      emulation_params_.view_size == params.view_size &&
      emulation_params_.screen_position == params.screen_position &&
      emulation_params_.device_scale_factor == params.device_scale_factor &&
      emulation_params_.scale == params.scale &&
      emulation_params_.viewport_offset == params.viewport_offset &&
      emulation_params_.viewport_scale == params.viewport_scale) {
    return ComputeRootLayerTransform();
  }
  if (emulation_params_.device_scale_factor != params.device_scale_factor ||
      !device_metrics_enabled_)
    GetMemoryCache()->EvictResources();

  emulation_params_ = params;
  device_metrics_enabled_ = true;

  web_view_->GetPage()->GetSettings().SetDeviceScaleAdjustment(
      calculateDeviceScaleAdjustment(params.view_size.width,
                                     params.view_size.height,
                                     params.device_scale_factor));

  if (params.screen_position == WebDeviceEmulationParams::kMobile)
    EnableMobileEmulation();
  else
    DisableMobileEmulation();

  web_view_->SetCompositorDeviceScaleFactorOverride(params.device_scale_factor);

  // TODO(wjmaclean): Tell all local frames in the WebView's frame tree, not
  // just a local main frame.
  if (web_view_->MainFrameImpl()) {
    if (Document* document =
            web_view_->MainFrameImpl()->GetFrame()->GetDocument())
      document->MediaQueryAffectingValueChanged();
  }

  if (params.viewport_offset.x >= 0)
    return ForceViewport(params.viewport_offset, params.viewport_scale);
  else
    return ResetViewport();
}

void DevToolsEmulator::DisableDeviceEmulation() {
  if (!device_metrics_enabled_)
    return;

  GetMemoryCache()->EvictResources();
  device_metrics_enabled_ = false;
  web_view_->GetPage()->GetSettings().SetDeviceScaleAdjustment(
      embedder_device_scale_adjustment_);
  DisableMobileEmulation();
  web_view_->SetCompositorDeviceScaleFactorOverride(0.f);
  web_view_->SetPageScaleFactor(1.f);

  // TODO(wjmaclean): Tell all local frames in the WebView's frame tree, not
  // just a local main frame.
  if (web_view_->MainFrameImpl()) {
    if (Document* document =
            web_view_->MainFrameImpl()->GetFrame()->GetDocument())
      document->MediaQueryAffectingValueChanged();
  }

  TransformationMatrix matrix = ResetViewport();
  DCHECK(matrix.IsIdentity());
}

void DevToolsEmulator::EnableMobileEmulation() {
  if (emulate_mobile_enabled_)
    return;
  emulate_mobile_enabled_ = true;
  is_overlay_scrollbars_enabled_ =
      ScrollbarThemeSettings::OverlayScrollbarsEnabled();
  ScrollbarThemeSettings::SetOverlayScrollbarsEnabled(true);
  is_orientation_event_enabled_ =
      RuntimeEnabledFeatures::OrientationEventEnabled();
  RuntimeEnabledFeatures::SetOrientationEventEnabled(true);
  is_mobile_layout_theme_enabled_ =
      RuntimeEnabledFeatures::MobileLayoutThemeEnabled();
  RuntimeEnabledFeatures::SetMobileLayoutThemeEnabled(true);
  ComputedStyle::InvalidateInitialStyle();
  web_view_->GetPage()->GetSettings().SetForceAndroidOverlayScrollbar(true);
  web_view_->GetPage()->GetSettings().SetViewportStyle(
      WebViewportStyle::kMobile);
  web_view_->GetPage()->GetSettings().SetViewportEnabled(true);
  web_view_->GetPage()->GetSettings().SetViewportMetaEnabled(true);
  web_view_->GetPage()->GetVisualViewport().InitializeScrollbars();
  web_view_->GetSettings()->SetShrinksViewportContentToFit(true);
  web_view_->GetPage()->GetSettings().SetTextAutosizingEnabled(true);
  web_view_->GetPage()->GetSettings().SetPreferCompositingToLCDTextEnabled(
      true);
  web_view_->GetPage()->GetSettings().SetPluginsEnabled(false);
  web_view_->GetPage()->GetSettings().SetMainFrameResizesAreOrientationChanges(
      true);
  web_view_->SetZoomFactorOverride(1);

  original_default_minimum_page_scale_factor_ =
      web_view_->DefaultMinimumPageScaleFactor();
  original_default_maximum_page_scale_factor_ =
      web_view_->DefaultMaximumPageScaleFactor();
  web_view_->SetDefaultPageScaleLimits(0.25f, 5);

  // TODO(wjmaclean): Update all local frames in the WebView's frame tree, not
  // just a local main frame.
  if (web_view_->MainFrameImpl())
    web_view_->MainFrameImpl()->GetFrameView()->UpdateLayout();
}

void DevToolsEmulator::DisableMobileEmulation() {
  if (!emulate_mobile_enabled_)
    return;
  ScrollbarThemeSettings::SetOverlayScrollbarsEnabled(
      is_overlay_scrollbars_enabled_);
  RuntimeEnabledFeatures::SetOrientationEventEnabled(
      is_orientation_event_enabled_);
  RuntimeEnabledFeatures::SetMobileLayoutThemeEnabled(
      is_mobile_layout_theme_enabled_);
  ComputedStyle::InvalidateInitialStyle();
  web_view_->GetPage()->GetSettings().SetForceAndroidOverlayScrollbar(false);
  web_view_->GetPage()->GetSettings().SetViewportEnabled(false);
  web_view_->GetPage()->GetSettings().SetViewportMetaEnabled(false);
  web_view_->GetPage()->GetVisualViewport().InitializeScrollbars();
  web_view_->GetSettings()->SetShrinksViewportContentToFit(false);
  web_view_->GetPage()->GetSettings().SetTextAutosizingEnabled(
      embedder_text_autosizing_enabled_);
  web_view_->GetPage()->GetSettings().SetPreferCompositingToLCDTextEnabled(
      embedder_prefer_compositing_to_lcd_text_enabled_);
  web_view_->GetPage()->GetSettings().SetViewportStyle(
      embedder_viewport_style_);
  web_view_->GetPage()->GetSettings().SetPluginsEnabled(
      embedder_plugins_enabled_);
  web_view_->GetPage()->GetSettings().SetMainFrameResizesAreOrientationChanges(
      embedder_main_frame_resizes_are_orientation_changes_);
  web_view_->SetZoomFactorOverride(0);
  emulate_mobile_enabled_ = false;
  web_view_->SetDefaultPageScaleLimits(
      original_default_minimum_page_scale_factor_,
      original_default_maximum_page_scale_factor_);
  // MainFrameImpl() could be null during cleanup or remote <-> local swap.
  if (web_view_->MainFrameImpl())
    web_view_->MainFrameImpl()->GetFrameView()->UpdateLayout();
}

TransformationMatrix DevToolsEmulator::ForceViewport(
    const WebFloatPoint& position,
    float scale) {
  if (!viewport_override_)
    viewport_override_ = ViewportOverride();

  viewport_override_->position = FloatPoint(position.x, position.y);
  viewport_override_->scale = scale;

  // Move the correct (scaled) content area to show in the top left of the
  // CompositorFrame via the root transform.
  return ComputeRootLayerTransform();
}

TransformationMatrix DevToolsEmulator::ResetViewport() {
  viewport_override_ = base::nullopt;
  return ComputeRootLayerTransform();
}

TransformationMatrix DevToolsEmulator::MainFrameScrollOrScaleChanged() {
  // Viewport override has to take current page scale and scroll offset into
  // account. Update the transform if override is active.
  DCHECK(viewport_override_);
  return ComputeRootLayerTransform();
}

void DevToolsEmulator::ApplyViewportOverride(TransformationMatrix* transform) {
  if (!viewport_override_)
    return;

  // Transform operations follow in reverse application.
  // Last, scale positioned area according to override.
  transform->Scale(viewport_override_->scale);

  // Translate while taking into account current scroll offset.
  // TODO(lukasza): https://crbug.com/734201: Add OOPIF support.
  WebSize scroll_offset =
      web_view_->MainFrame()->IsWebLocalFrame()
          ? web_view_->MainFrame()->ToWebLocalFrame()->GetScrollOffset()
          : WebSize();
  WebFloatPoint visual_offset = web_view_->VisualViewportOffset();
  float scroll_x = scroll_offset.width + visual_offset.x;
  float scroll_y = scroll_offset.height + visual_offset.y;
  transform->Translate(-viewport_override_->position.X() + scroll_x,
                       -viewport_override_->position.Y() + scroll_y);

  // First, reverse page scale, so we don't have to take it into account for
  // calculation of the translation.
  transform->Scale(1. / web_view_->PageScaleFactor());
}

TransformationMatrix DevToolsEmulator::ComputeRootLayerTransform() {
  TransformationMatrix transform;
  // Apply device emulation transform first, so that it is affected by the
  // viewport override.
  ApplyViewportOverride(&transform);
  if (device_metrics_enabled_)
    transform.Scale(emulation_params_.scale);
  return transform;
}

void DevToolsEmulator::OverrideVisibleRect(const IntSize& viewport_size,
                                           IntRect* visible_rect) const {
  if (!viewport_override_)
    return;

  FloatSize scaled_viewport_size(viewport_size);
  scaled_viewport_size.Scale(1. / viewport_override_->scale);
  *visible_rect = EnclosingIntRect(
      FloatRect(viewport_override_->position, scaled_viewport_size));
}

float DevToolsEmulator::InputEventsScaleForEmulation() {
  return device_metrics_enabled_ ? emulation_params_.scale : 1.0;
}

void DevToolsEmulator::SetTouchEventEmulationEnabled(bool enabled,
                                                     int max_touch_points) {
  if (!touch_event_emulation_enabled_) {
    original_max_touch_points_ =
        web_view_->GetPage()->GetSettings().GetMaxTouchPoints();
  }
  touch_event_emulation_enabled_ = enabled;
  web_view_->GetPage()
      ->GetSettings()
      .SetForceTouchEventFeatureDetectionForInspector(enabled);
  web_view_->GetPage()->GetSettings().SetMaxTouchPoints(
      enabled ? max_touch_points : original_max_touch_points_);
  web_view_->GetPage()->GetSettings().SetAvailablePointerTypes(
      enabled ? kPointerTypeCoarse : embedder_available_pointer_types_);
  web_view_->GetPage()->GetSettings().SetPrimaryPointerType(
      enabled ? kPointerTypeCoarse : embedder_primary_pointer_type_);
  web_view_->GetPage()->GetSettings().SetAvailableHoverTypes(
      enabled ? kHoverTypeNone : embedder_available_hover_types_);
  web_view_->GetPage()->GetSettings().SetPrimaryHoverType(
      enabled ? kHoverTypeNone : embedder_primary_hover_type_);
  WebLocalFrameImpl* frame = web_view_->MainFrameImpl();
  if (enabled && frame)
    frame->GetFrame()->GetEventHandler().ClearMouseEventManager();
}

void DevToolsEmulator::SetScriptExecutionDisabled(
    bool script_execution_disabled) {
  script_execution_disabled_ = script_execution_disabled;
  web_view_->GetPage()->GetSettings().SetScriptEnabled(
      script_execution_disabled_ ? false : embedder_script_enabled_);
}

void DevToolsEmulator::SetScrollbarsHidden(bool hidden) {
  if (scrollbars_hidden_ == hidden)
    return;
  scrollbars_hidden_ = hidden;
  web_view_->GetPage()->GetSettings().SetHideScrollbars(
      scrollbars_hidden_ ? true : embedder_hide_scrollbars_);
}

void DevToolsEmulator::SetDocumentCookieDisabled(bool disabled) {
  if (document_cookie_disabled_ == disabled)
    return;
  document_cookie_disabled_ = disabled;
  web_view_->GetPage()->GetSettings().SetCookieEnabled(
      document_cookie_disabled_ ? false : embedder_cookie_enabled_);
}

}  // namespace blink
