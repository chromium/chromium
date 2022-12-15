// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/render_frame_host_observer.h"
#include "base/check.h"

namespace content {

RenderFrameHostObserver::~RenderFrameHostObserver() {
  DCHECK(!IsInObserverList());
}

}  // namespace content