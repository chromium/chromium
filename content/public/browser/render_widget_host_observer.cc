// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/render_widget_host_observer.h"
#include "base/check.h"

namespace content {

RenderWidgetHostObserver::~RenderWidgetHostObserver() {
  DCHECK(!IsInObserverList());
}

}  // namespace content
