// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_widget_host_view_base_observer.h"

namespace content {

RenderWidgetHostViewBaseObserver::~RenderWidgetHostViewBaseObserver() {}

void RenderWidgetHostViewBaseObserver::OnRenderWidgetHostViewBaseDestroyed(
    RenderWidgetHostViewBase*) {}

}  // namespace content
