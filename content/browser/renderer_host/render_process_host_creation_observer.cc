// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/render_process_host_creation_observer.h"

#include "content/browser/renderer_host/render_process_host_impl.h"

namespace content {

RenderProcessHostCreationObserver::RenderProcessHostCreationObserver() {
  RenderProcessHostImpl::RegisterCreationObserver(this);
}

RenderProcessHostCreationObserver::~RenderProcessHostCreationObserver() {
  RenderProcessHostImpl::UnregisterCreationObserver(this);
}

void RenderProcessHostCreationObserver::OnRenderProcessHostCreationFailed(
    content::RenderProcessHost* host,
    const content::ChildProcessTerminationInfo& info) {}

}  // namespace content
