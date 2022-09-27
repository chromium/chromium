// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/main_thread/render_widget_signals.h"

#include "base/check_op.h"
#include "base/memory/ptr_util.h"
#include "third_party/perfetto/include/perfetto/tracing/traced_value.h"

namespace blink {
namespace scheduler {

RenderWidgetSignals::RenderWidgetSignals(Observer* observer)
    : observer_(observer),
      num_visible_render_widgets_(0),
      num_visible_render_widgets_with_touch_handlers_(0) {}

void RenderWidgetSignals::IncNumVisibleRenderWidgets() {
  num_visible_render_widgets_++;

  if (num_visible_render_widgets_ == 1)
    observer_->SetAllRenderWidgetsHidden(false);
}

void RenderWidgetSignals::DecNumVisibleRenderWidgets() {
  num_visible_render_widgets_--;
  DCHECK_GE(num_visible_render_widgets_, 0);

  if (num_visible_render_widgets_ == 0)
    observer_->SetAllRenderWidgetsHidden(true);
}

void RenderWidgetSignals::IncNumVisibleRenderWidgetsWithTouchHandlers() {
  num_visible_render_widgets_with_touch_handlers_++;

  if (num_visible_render_widgets_with_touch_handlers_ == 1)
    observer_->SetHasVisibleRenderWidgetWithTouchHandler(true);
}

void RenderWidgetSignals::DecNumVisibleRenderWidgetsWithTouchHandlers() {
  num_visible_render_widgets_with_touch_handlers_--;
  DCHECK_GE(num_visible_render_widgets_with_touch_handlers_, 0);

  if (num_visible_render_widgets_with_touch_handlers_ == 0)
    observer_->SetHasVisibleRenderWidgetWithTouchHandler(false);
}

void RenderWidgetSignals::WriteIntoTrace(perfetto::TracedValue context) const {
  auto dict = std::move(context).WriteDictionary();
  dict.Add("num_visible_render_widgets", num_visible_render_widgets_);
  dict.Add("num_visible_render_widgets_with_touch_handlers",
           num_visible_render_widgets_with_touch_handlers_);
}

}  // namespace scheduler
}  // namespace blink
