// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_INPUT_RENDER_WIDGET_HOST_VIEW_INPUT_OBSERVER_H_
#define CONTENT_COMMON_INPUT_RENDER_WIDGET_HOST_VIEW_INPUT_OBSERVER_H_

#include "content/common/content_export.h"

namespace content {

class RenderWidgetHostViewInput;

class CONTENT_EXPORT RenderWidgetHostViewInputObserver {
 public:
  RenderWidgetHostViewInputObserver(const RenderWidgetHostViewInputObserver&) =
      delete;
  RenderWidgetHostViewInputObserver& operator=(
      const RenderWidgetHostViewInputObserver&) = delete;

  // All derived classes must de-register as observers when receiving this
  // notification.
  virtual void OnRenderWidgetHostViewInputDestroyed(
      RenderWidgetHostViewInput* view);

 protected:
  RenderWidgetHostViewInputObserver() = default;
  virtual ~RenderWidgetHostViewInputObserver();
};

}  // namespace content

#endif  // CONTENT_COMMON_INPUT_RENDER_WIDGET_HOST_VIEW_INPUT_OBSERVER_H_
