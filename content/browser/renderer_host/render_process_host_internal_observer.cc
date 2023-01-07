// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_process_host_internal_observer.h"

#include "base/check.h"
#include "base/observer_list_types.h"

namespace content {

RenderProcessHostInternalObserver::~RenderProcessHostInternalObserver() {
  DCHECK(!IsInObserverList());
}

}  // namespace content
