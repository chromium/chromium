// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/inspector/dev_tools_emulator.h"

#include <algorithm>

#include "third_party/blink/public/mojom/widget/device_emulation_params.mojom-blink.h"
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
#include "third_party/blink/renderer/platform/loader/fetch/memory_cache.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_f.h"

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

class DevToolsEmulator::ScopedGlobalOverrides
    : public WTF::RefCounted<ScopedGlobalOverrides> {
 public:
  static scoped_refptr<ScopedGlobalOverrides> AssureInstalled() {
    return g_instance_ ? g_instance_
                       : base::AdoptRef(new ScopedGlobalOverrides());
  }

 private:
  friend class WTF::RefCounted<ScopedGlobalOverrides>;

  ScopedGlobalOverrides()
      : overlay_scrollbars_enabled_(
            ScrollbarThemeSettings::OverlayScrollbarsEnabled()),
        orientation_event_enabled_(
            RuntimeEnabledFeatures::OrientationEventEnabled()),
        mobile_layout_theme_enabled_(
            RuntimeEnabledFeatures::MobileLayoutThemeEnabled()) {
    ScrollbarThemeSettings::SetOverlayScrollbarsEnabled(true);
    Page::UsesOverlayScrollbarsChanged();
    RuntimeEnabledFeatures::SetOrientationEventEnabled(true);
    RuntimeEnabledFeatures::SetMobileLayoutThemeEnabled(true);
    Page::PlatformColorsChanged();

    CHECK(!g_instance_);
    g_instance_ = this;
  }

  ~ScopedGlobalOverrides() {
    CHECK(g_instance_);
    g_instance_ = nullptr;

    ScrollbarThemeSettings::SetOverlayScrollbarsEnabled(
        overlay_scrollbars_enabled_);
    Page::UsesOverlayScrollbarsChanged();
    RuntimeEnabledFeatures::SetOrientationEventEnabled(
        orientation_event_enabled_);
    RuntimeEnabledFeatures::SetMobileLayoutThemeEnabled(
        mobile_layout_theme_enabled_);
    Page::PlatformColorsChanged();
  }

  static ScopedGlobalOverrides* g_instance_;

  const bool overlay_scrollbars_enabled_;
  const bool orientation_event_enabled_;
  const bool mobile_layout_theme_enabled_;
};

DevToolsEmulator::ScopedGlobalOverrides*
    DevToolsEmulator::ScopedGlobalOverrides::g_instance_ = nullptr;

DevToolsEmulator::DevToolsEmulator(WebViewImpl* web_view)
    : web_view_(web_view),
      device_metrics_enabled_(false),
      embedder_text_autosizing_enabled_(
          web_view->GetPage()->GetSettings().GetTextAutosizingEnabled()),
      embedder_device_scale_adjustment_(
          web_view->GetPage()->GetSettings().GetDeviceScaleAdjustment()),
      embedder_lcd_text_preference_(
          web_view->GetPage()->GetSettings().GetLCDTextPreference()),
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
      embedder_min_page_scale_(web_view->DefaultMinimumPageScaleFactor()),
      embedder_max_page_scale_(web_view->DefaultMaximumPageScaleFactor()),
      embedder_shrink_viewport_content_(
          web_view->GetPage()->GetSettings().GetShrinksViewportContentToFit()),
      embedder_viewport_enabled_(
          web_view->GetPage()->GetSettings().GetViewportEnabled()),
      embedder_viewport_meta_enabled_(
          web_view->GetPage()->GetSettings().GetViewportMetaEnabled()),
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
      document_cookie_disabled_(false),
      embedder_force_dark_mode_enabled_(
          web_view->GetPage()->GetSettings().GetForceDarkModeEnabled()),
      auto_dark_overriden_(false) {}

DevToolsEmulator::~DevToolsEmulator() {
  // This class is GarbageCollected, so desturctor may run at any time, hence
  // we need to ensure the RAII handle for global overrides did its business
  // before the destructor runs (i.e. Shutdown() has been called)
  CHECK(!global_overrides_);
  CHECK(is_shutdown_);
}

void DevToolsEmulator::Trace(Visitor* visitor) const {}

void DevToolsEmulator::Shutdown() {
  CHECK(!is_shutdown_);
  is_shutdown_ = true;
  // Restore global overrides, but do not restore any page overrides, since
  // the page may already be in an inconsistent state at this moment.
  global_overrides_.reset();
}

void DevToolsEmulator::SetTextAutosizingEnabled(bool enabled) {
  embedder_text_autosizing_enabled_ = enabled;
  if (!emulate_mobile_enabled()) {
    web_view_->GetPage()->GetSettings().SetTextAutosizingEnabled(enabled);
  }
}

void DevToolsEmulator::SetDeviceScaleAdjustment(float device_scale_adjustment) {
  embedder_device_scale_adjustment_ = device_scale_adjustment;
  if (!emulate_mobile_enabled()) {
    web_view_->GetPage()->GetSettings().SetDeviceScaleAdjustment(
        device_scale_adjustment);
  }
}

void DevToolsEmulator::SetLCDTextPreference(LCDTextPreference preference) {
  if (embedder_lcd_text_preference_ == preference) {
    return;
  }

  embedder_lcd_text_preference_ = preference;
  if (!emulate_mobile_enabled()) {
    web_view_->GetPage()->GetSettings().SetLCDTextPreference(preference);
  }
}

void DevToolsEmulator::SetViewportStyle(mojom::blink::ViewportStyle style) {
  embedder_viewport_style_ = style;
  if (!emulate_mobile_enabled()) {
    web_view_->GetPage()->GetSettings().SetViewportStyle(style);
  }
}

void DevToolsEmulator::SetPluginsEnabled(bool enabled) {
  embedder_plugins_enabled_ = enabled;
  if (!emulate_mobile_enabled()) {
    web_view_->GetPage()->GetSettings().SetPluginsEnabled(enabled);
  }
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
  if (!emulate_mobile_enabled()) {
    web_view_->GetPage()
        ->GetSettings()
        .SetMainFrameResizesAreOrientationChanges(value);
  }
}

void DevToolsEmulator::SetDefaultPageScaleLimits(float min_scale,
                                                 float max_scale) {
  embedder_min_page_scale_ = min_scale;
  embedder_max_page_scale_ = max_scale;
  if (!emulate_mobile_enabled()) {
    web_view_->GetPage()->SetDefaultPageScaleLimits(min_scale, max_scale);
  }
}

void DevToolsEmulator::SetShrinksViewportContentToFit(
    bool shrink_viewport_content) {
  embedder_shrink_viewport_content_ = shrink_viewport_content;
  if (!emulate_mobile_enabled()) {
    web_view_->GetPage()->GetSettings().SetShrinksViewportContentToFit(
        shrink_viewport_content);
  }
}

void DevToolsEmulator::SetViewportEnabled(bool enabled) {
  embedder_viewport_enabled_ = enabled;
  if (!emulate_mobile_enabled()) {
    web_view_->GetPage()->GetSettings().SetViewportEnabled(enabled);
  }
}

void DevToolsEmulator::SetViewportMetaEnabled(bool enabled) {
  embedder_viewport_meta_enabled_ = enabled;
  if (!emulate_mobile_enabled()) {
    web_view_->GetPage()->GetSettings().SetViewportMetaEnabled(enabled);
  }
}

void DevToolsEmulator::SetAvailablePointerTypes(int types) {
  embedder_available_pointer_types_ = types;
  if (!touch_event_emulation_enabled_)
    web_view_->GetPage()->GetSettings().SetAvailablePointerTypes(types);
}

void DevToolsEmulator::SetPrimaryPointerType(
    mojom::blink::PointerType pointer_type) {
  embedder_primary_pointer_type_ = pointer_type;
  if (!touch_event_emulation_enabled_)
    web_view_->GetPage()->GetSettings().SetPrimaryPointerType(pointer_type);
}

void DevToolsEmulator::SetAvailableHoverTypes(int types) {
  embedder_available_hover_types_ = types;
  if (!touch_event_emulation_enabled_)
    web_view_->GetPage()->GetSettings().SetAvailableHoverTypes(types);
}

void DevToolsEmulator::SetPrimaryHoverType(mojom::blink::HoverType hover_type) {
  embedder_primary_hover_type_ = hover_type;
  if (!touch_event_emulation_enabled_)
    web_view_->GetPage()->GetSettings().SetPrimaryHoverType(hover_type);
}

void DevToolsEmulator::SetOutputDeviceUpdateAbilityType(
    mojom::blink::OutputDeviceUpdateAbilityType type) {
  embedder_output_device_update_ability_type_ = type;
  web_view_->GetPage()->GetSettings().SetOutputDeviceUpdateAbilityType(type);
}

gfx::Transform DevToolsEmulator::EnableDeviceEmulation(
    const DeviceEmulationParams& params) {
  if (device_metrics_enabled_ &&
      emulation_params_.view_size == params.view_size &&
      emulation_params_.screen_type == params.screen_type &&
      emulation_params_.device_scale_factor == params.device_scale_factor &&
      emulation_params_.scale == params.scale &&
      emulation_params_.viewport_offset == params.viewport_offset &&
      emulation_params_.viewport_scale == params.viewport_scale) {
    return ComputeRootLayerTransform();
  }
  if (emulation_params_.device_scale_factor != params.device_scale_factor ||
      !device_metrics_enabled_)
    MemoryCache::Get()->EvictResources();

  emulation_params_ = params;
  device_metrics_enabled_ = true;

  web_view_->GetPage()->GetSettings().SetDeviceScaleAdjustment(
      calculateDeviceScaleAdjustment(params.view_size.width(),
                                     params.view_size.height(),
                                     params.device_scale_factor));

  if (params.screen_type == mojom::blink::EmulatedScreenType::kMobile)
    EnableMobileEmulation();
  else
    DisableMobileEmulation();

  web_view_->SetCompositorDeviceScaleFactorOverride(params.device_scale_factor);

  // TODO(wjmaclean): Tell all local frames in the WebView's frame tree, not
  // just a local main frame.
  if (web_view_->MainFrameImpl()) {
    if (Document* document =
            web_view_->MainFrameImpl()->GetFrame()->GetDocument())
      document->MediaQueryAffectingValueChanged(MediaValueChange::kOther);
  }

  if (params.viewport_offset.x() >= 0)
    return ForceViewport(params.viewport_offset, params.viewport_scale);
  else
    return ResetViewport();
}

void DevToolsEmulator::DisableDeviceEmulation() {
  CHECK(!is_shutdown_);
  if (!device_metrics_enabled_)
    return;

  MemoryCache::Get()->EvictResources();
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
      document->MediaQueryAffectingValueChanged(MediaValueChange::kOther);
  }

  gfx::Transform matrix = ResetViewport();
  DCHECK(matrix.IsIdentity());
}

void DevToolsEmulator::EnableMobileEmulation() {
  if (global_overrides_) {
    return;
  }
  CHECK(!is_shutdown_);
  CHECK(!emulate_mobile_enabled());
  global_overrides_ = ScopedGlobalOverrides::AssureInstalled();
  web_view_->GetPage()->GetSettings().SetForceAndroidOverlayScrollbar(true);
  web_view_->GetPage()->GetSettings().SetViewportStyle(
      mojom::blink::ViewportStyle::kMobile);
  web_view_->GetPage()->GetSettings().SetViewportEnabled(true);
  web_view_->GetPage()->GetSettings().SetViewportMetaEnabled(true);
  web_view_->GetPage()->GetSettings().SetShrinksViewportContentToFit(true);
  web_view_->GetPage()->GetSettings().SetTextAutosizingEnabled(true);
  web_view_->GetPage()->GetSettings().SetLCDTextPreference(
      LCDTextPreference::kIgnored);
  web_view_->GetPage()->GetSettings().SetPluginsEnabled(false);
  web_view_->GetPage()->GetSettings().SetMainFrameResizesAreOrientationChanges(
      true);
  web_view_->SetZoomFactorOverride(1);
  web_view_->GetPage()->SetDefaultPageScaleLimits(0.25f, 5);

  // If the viewport is active, refresh the scrollbar layers to reflect the
  // emulated viewport style. If it's not active, either we're in an embedded
  // frame and we don't have visual viewport scrollbars or the scrollbars will
  // initialize as part of their regular lifecycle.
  if (web_view_->GetPage()->GetVisualViewport().IsActiveViewport())
    web_view_->GetPage()->GetVisualViewport().InitializeScrollbars();

  if (web_view_->MainFrameImpl()) {
    web_view_->MainFrameImpl()->GetFrameView()->UpdateLifecycleToLayoutClean(
        DocumentUpdateReason::kInspector);
  }
}

void DevToolsEmulator::DisableMobileEmulation() {
  if (!global_overrides_) {
    return;
  }
  global_overrides_.reset();
  web_view_->GetPage()->GetSettings().SetForceAndroidOverlayScrollbar(false);
  web_view_->GetPage()->GetSettings().SetViewportEnabled(
      embedder_viewport_enabled_);
  web_view_->GetPage()->GetSettings().SetViewportMetaEnabled(
      embedder_viewport_meta_enabled_);
  web_view_->GetPage()->GetVisualViewport().InitializeScrollbars();
  web_view_->GetSettings()->SetShrinksViewportContentToFit(
      embedder_shrink_viewport_content_);
  web_view_->GetPage()->GetSettings().SetTextAutosizingEnabled(
      embedder_text_autosizing_enabled_);
  web_view_->GetPage()->GetSettings().SetLCDTextPreference(
      embedder_lcd_text_preference_);
  web_view_->GetPage()->GetSettings().SetViewportStyle(
      embedder_viewport_style_);
  web_view_->GetPage()->GetSettings().SetPluginsEnabled(
      embedder_plugins_enabled_);
  web_view_->GetPage()->GetSettings().SetMainFrameResizesAreOrientationChanges(
      embedder_main_frame_resizes_are_orientation_changes_);
  web_view_->SetZoomFactorOverride(0);
  web_view_->GetPage()->SetDefaultPageScaleLimits(embedder_min_page_scale_,
                                                  embedder_max_page_scale_);
  // MainFrameImpl() could be null during cleanup or remote <-> local swap.
  if (web_view_->MainFrameImpl()) {
    web_view_->MainFrameImpl()->GetFrameView()->UpdateLifecycleToLayoutClean(
        DocumentUpdateReason::kInspector);
  }
}

gfx::Transform DevToolsEmulator::ForceViewport(const gfx::PointF& position,
                                               float scale) {
  if (!viewport_override_)
    viewport_override_ = ViewportOverride();

  viewport_override_->position = position;
  viewport_override_->scale = scale;

  // Move the correct (scaled) content area to show in the top left of the
  // CompositorFrame via the root transform.
  return ComputeRootLayerTransform();
}

gfx::Transform DevToolsEmulator::ResetViewport() {
  viewport_override_ = std::nullopt;
  return ComputeRootLayerTransform();
}

gfx::Transform DevToolsEmulator::OutermostMainFrameScrollOrScaleChanged() {
  // Viewport override has to take current page scale and scroll offset into
  // account. Update the transform if override is active.
  DCHECK(viewport_override_);
  return ComputeRootLayerTransform();
}

void DevToolsEmulator::ApplyViewportOverride(gfx::Transform* transform) {
  if (!viewport_override_)
    return;

  // Transform operations follow in reverse application.
  // Last, scale positioned area according to override.
  transform->Scale(viewport_override_->scale);

  // Translate while taking into account current scroll offset.
  // TODO(lukasza): https://crbug.com/734201: Add OOPIF support.
  gfx::PointF scroll_offset =
      web_view_->MainFrame()->IsWebLocalFrame()
          ? web_view_->MainFrame()->ToWebLocalFrame()->GetScrollOffset()
          : gfx::PointF();
  gfx::PointF visual_offset = web_view_->VisualViewportOffset();
  float scroll_x = scroll_offset.x() + visual_offset.x();
  float scroll_y = scroll_offset.y() + visual_offset.y();
  transform->Translate(-viewport_override_->position.x() + scroll_x,
                       -viewport_override_->position.y() + scroll_y);

  // First, reverse page scale, so we don't have to take it into account for
  // calculation of the translation.
  transform->Scale(1. / web_view_->PageScaleFactor());
}

gfx::Transform DevToolsEmulator::ComputeRootLayerTransform() {
  gfx::Transform transform;
  // Apply device emulation transform first, so that it is affected by the
  // viewport override.
  ApplyViewportOverride(&transform);
  if (device_metrics_enabled_)
    transform.Scale(emulation_params_.scale);
  return transform;
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
      enabled ? static_cast<int>(mojom::blink::PointerType::kPointerCoarseType)
              : embedder_available_pointer_types_);
  web_view_->GetPage()->GetSettings().SetPrimaryPointerType(
      enabled ? mojom::blink::PointerType::kPointerCoarseType
              : embedder_primary_pointer_type_);
  web_view_->GetPage()->GetSettings().SetAvailableHoverTypes(
      enabled ? static_cast<int>(mojom::blink::HoverType::kHoverNone)
              : embedder_available_hover_types_);
  web_view_->GetPage()->GetSettings().SetPrimaryHoverType(
      enabled ? mojom::blink::HoverType::kHoverNone
              : embedder_primary_hover_type_);
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

void DevToolsEmulator::SetAutoDarkModeOverride(bool enabled) {
  if (!auto_dark_overriden_) {
    auto_dark_overriden_ = true;
    embedder_force_dark_mode_enabled_ =
        web_view_->GetPage()->GetSettings().GetForceDarkModeEnabled();
  }
  web_view_->GetPage()->GetSettings().SetForceDarkModeEnabled(enabled);
}

void DevToolsEmulator::ResetAutoDarkModeOverride() {
  if (auto_dark_overriden_) {
    web_view_->GetPage()->GetSettings().SetForceDarkModeEnabled(
        embedder_force_dark_mode_enabled_);
    auto_dark_overriden_ = false;
  }
}

}  // namespace blink
