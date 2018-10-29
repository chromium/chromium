// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/loader_io_thread_notifier.h"

#include "base/task/post_task.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/global_routing_id.h"

namespace content {

namespace {

void NotifyRenderFrameDeletedOnIO(const GlobalFrameRoutingId& id) {
  ResourceDispatcherHostImpl* rdhi = ResourceDispatcherHostImpl::Get();
  if (rdhi)
    rdhi->OnRenderFrameDeleted(id);
}

}  // namespace

LoaderIOThreadNotifier::LoaderIOThreadNotifier(WebContents* web_contents)
    : WebContentsObserver(web_contents) {}

LoaderIOThreadNotifier::~LoaderIOThreadNotifier() {}

void LoaderIOThreadNotifier::RenderFrameDeleted(
    RenderFrameHost* render_frame_host) {
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&NotifyRenderFrameDeletedOnIO,
                     static_cast<RenderFrameHostImpl*>(render_frame_host)
                         ->GetGlobalFrameRoutingId()));
}

}  // namespace content
