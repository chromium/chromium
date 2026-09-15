// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TIMING_WEB_VITALS_HUD_HELPER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TIMING_WEB_VITALS_HUD_HELPER_H_

#include "cc/layers/heads_up_display_layer.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace cc {
class HeadsUpDisplayLayer;
enum class WebVitalMetricType;
}  // namespace cc

namespace gfx {
class Rect;
}  // namespace gfx

namespace blink {
class FrameWidget;
class LocalFrameView;
class PaintTimingRecord;

// Helper for sending Web Vitals debug rects to the HUD layer.
class CORE_EXPORT WebVitalsHudHelper {
  STACK_ALLOCATED();

 public:
  WebVitalsHudHelper(cc::WebVitalMetricType metric_type,
                     LocalFrameView* frame_view);

  bool IsEnabled() const { return hud_layer_ && widget_; }

  void AddWebVitalsDebugRect(const PaintTimingRecord& record);
  void AddWebVitalsDebugRect(const gfx::Rect& rect);

 private:
  cc::HeadsUpDisplayLayer* hud_layer_ = nullptr;
  FrameWidget* widget_ = nullptr;
  cc::WebVitalMetricType metric_type_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TIMING_WEB_VITALS_HUD_HELPER_H_
