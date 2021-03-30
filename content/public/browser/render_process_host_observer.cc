// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/render_process_host_observer.h"
#include "base/check.h"

namespace content {

RenderProcessHostObserver::~RenderProcessHostObserver() {
  DCHECK(!IsInObserverList());
}

}  // namespace content
