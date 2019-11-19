// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_DEV_TOOLS_EMULATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_DEV_TOOLS_EMULATOR_H_

#include <memory>
#include "base/optional.h"
#include "third_party/blink/public/platform/pointer_properties.h"
#include "third_party/blink/public/platform/web_float_point.h"
#include "third_party/blink/public/platform/web_viewport_style.h"
#include "third_party/blink/public/web/web_device_emulation_params.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/transforms/transformation_matrix.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

class IntRect;
class WebViewImpl;

class CORE_EXPORT DevToolsEmulator final
    : public GarbageCollected<DevToolsEmulator> {
 public:
  explicit DevToolsEmulator(WebViewImpl*);
  ~DevToolsEmulator();
  void Trace(blink::Visitor*);

  // Settings overrides.
  void SetTextAutosizingEnabled(bool);
  void SetDeviceScaleAdjustment(float);
  void SetPreferCompositingToLCDTextEnabled(bool);
  void SetViewportStyle(WebViewportStyle);
  void SetPluginsEnabled(bool);
  void SetScriptEnabled(bool);
  void SetHideScrollbars(bool);
  void SetCookieEnabled(bool);
  void SetDoubleTapToZoomEnabled(bool);
  bool DoubleTapToZoomEnabled() const;
  void SetAvailablePointerTypes(int);
  void SetPrimaryPointerType(PointerType);
  void SetAvailableHoverTypes(int);
  void SetPrimaryHoverType(HoverType);
  void SetMainFrameResizesAreOrientationChanges(bool);

  // Enables and/or sets the parameters for emulation. Returns the emulation
  // transform to be used as a result.
  TransformationMatrix EnableDeviceEmulation(const WebDeviceEmulationParams&);
  // Disables emulation.
  void DisableDeviceEmulation();

  bool ResizeIsDeviceSizeChange();
  void SetTouchEventEmulationEnabled(bool, int max_touch_points);
  void SetScriptExecutionDisabled(bool);
  void SetScrollbarsHidden(bool);
  void SetDocumentCookieDisabled(bool);

  bool HasViewportOverride() const { return !!viewport_override_; }

  // Notify the DevToolsEmulator about a scroll or scale change of the main
  // frame. Returns an updated emulation transform for a viewport override, and
  // should only be called when HasViewportOverride() is true.
  TransformationMatrix MainFrameScrollOrScaleChanged();

  // Rewrites the |visible_rect| to the area of the devtools custom viewport if
  // it is enabled. Otherwise, leaves |visible_rect| unchanged. Takes as input
  // the size of the viewport, which gives an upper bound on the size of the
  // area that is visible. The |viewport_size| is physical pixels if
  // UseZoomForDSF() is enabled, or DIP otherwise.
  void OverrideVisibleRect(const IntSize& viewport_size,
                           IntRect* visible_rect) const;

  // Returns the scale used to convert incoming input events while emulating
  // device metics.
  float InputEventsScaleForEmulation();

  TransformationMatrix ForceViewportForTesting(const WebFloatPoint& position,
                                               float scale) {
    return ForceViewport(position, scale);
  }
  TransformationMatrix ResetViewportForTesting() { return ResetViewport(); }

 private:
  void EnableMobileEmulation();
  void DisableMobileEmulation();

  // Enables viewport override and returns the emulation transform to be used.
  // The |position| is in CSS pixels, and |scale| is relative to a page scale of
  // 1.0.
  TransformationMatrix ForceViewport(const WebFloatPoint& position,
                                     float scale);
  // Disables viewport override and returns the emulation transform to be used.
  TransformationMatrix ResetViewport();

  // Returns the original device scale factor when overridden by DevTools, or
  // deviceScaleFactor() otherwise.
  float CompositorDeviceScaleFactor() const;

  void ApplyViewportOverride(TransformationMatrix*);
  TransformationMatrix ComputeRootLayerTransform();

  WebViewImpl* web_view_;

  bool device_metrics_enabled_;
  bool emulate_mobile_enabled_;
  WebDeviceEmulationParams emulation_params_;

  struct ViewportOverride {
    FloatPoint position;
    double scale;
  };
  base::Optional<ViewportOverride> viewport_override_;

  bool is_overlay_scrollbars_enabled_;
  bool is_orientation_event_enabled_;
  bool is_mobile_layout_theme_enabled_;
  float original_default_minimum_page_scale_factor_;
  float original_default_maximum_page_scale_factor_;
  bool embedder_text_autosizing_enabled_;
  float embedder_device_scale_adjustment_;
  bool embedder_prefer_compositing_to_lcd_text_enabled_;
  WebViewportStyle embedder_viewport_style_;
  bool embedder_plugins_enabled_;
  int embedder_available_pointer_types_;
  PointerType embedder_primary_pointer_type_;
  int embedder_available_hover_types_;
  HoverType embedder_primary_hover_type_;
  bool embedder_main_frame_resizes_are_orientation_changes_;

  bool touch_event_emulation_enabled_;
  bool double_tap_to_zoom_enabled_;
  int original_max_touch_points_;

  bool embedder_script_enabled_;
  bool script_execution_disabled_;

  bool embedder_hide_scrollbars_;
  bool scrollbars_hidden_;

  bool embedder_cookie_enabled_;
  bool document_cookie_disabled_;
};

}  // namespace blink

#endif
