// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_RENDER_WIDGET_HOST_OBSERVER_H_
#define CONTENT_PUBLIC_BROWSER_RENDER_WIDGET_HOST_OBSERVER_H_

#include "base/observer_list_types.h"
#include "content/common/content_export.h"

namespace content {

class RenderWidgetHost;

// An observer API implemented by classes which are interested
// in RenderWidgetHost events.
class CONTENT_EXPORT RenderWidgetHostObserver : public base::CheckedObserver {
 public:
  // This method is invoked when the visibility of the RenderWidgetHost changes.
  virtual void RenderWidgetHostVisibilityChanged(RenderWidgetHost* widget_host,
                                                 bool became_visible) {}

  // This method is invoked when the observed RenderWidgetHost is destroyed.
  // This is guaranteed to be the last call made to the observer, so if the
  // observer is tied to the observed RenderWidgetHost, it is safe to delete it.
  virtual void RenderWidgetHostDestroyed(RenderWidgetHost* widget_host) {}

 protected:
  ~RenderWidgetHostObserver() override = default;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_RENDER_PROCESS_HOST_OBSERVER_H_
