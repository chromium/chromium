// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/render_process_host_observer.h"
#include "base/check.h"

namespace content {

RenderProcessHostObserver::~RenderProcessHostObserver() {
  DCHECK(!IsInObserverList());
}

}  // namespace content
