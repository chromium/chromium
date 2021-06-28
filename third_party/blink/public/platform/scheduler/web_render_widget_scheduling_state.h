// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_SCHEDULER_WEB_RENDER_WIDGET_SCHEDULING_STATE_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_SCHEDULER_WEB_RENDER_WIDGET_SCHEDULING_STATE_H_

#include "third_party/blink/public/platform/web_common.h"

namespace blink {
namespace scheduler {

class RenderWidgetSignals;

class BLINK_PLATFORM_EXPORT WebRenderWidgetSchedulingState {
 public:
  void SetHidden(bool hidden);
  void SetHasTouchHandler(bool has_touch_handler);

  ~WebRenderWidgetSchedulingState();

 private:
  friend class RenderWidgetSignals;

  explicit WebRenderWidgetSchedulingState(
      RenderWidgetSignals* render_widget_scheduling_signals);

  RenderWidgetSignals* render_widget_signals_;  // NOT OWNED
  bool hidden_;
  bool has_touch_handler_;
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_SCHEDULER_WEB_RENDER_WIDGET_SCHEDULING_STATE_H_
