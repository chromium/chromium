// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/stored_page.h"

#include "base/containers/contains.h"
#include "base/trace_event/typed_macros.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_frame_proxy_host.h"
#include "content/browser/renderer_host/render_view_host_impl.h"

namespace content {
namespace {
using perfetto::protos::pbzero::ChromeTrackEvent;
}

StoredPage::StoredPage(std::unique_ptr<RenderFrameHostImpl> rfh,
                       RenderFrameProxyHostMap proxy_hosts,
                       RenderViewHostImplSafeRefSet render_view_hosts)
    : render_frame_host_(std::move(rfh)),
      proxy_hosts_(std::move(proxy_hosts)),
      render_view_hosts_(std::move(render_view_hosts)) {
  for (const auto& rvh : render_view_hosts_) {
    rvh->site_instance_group()->AddObserver(this);
  }
  for (const auto& pair : proxy_hosts_) {
    TRACE_EVENT_INSTANT("navigation", "StoredPage::StoredPage_Proxy",
                        ChromeTrackEvent::kRenderFrameProxyHost, *pair.second);
  }
}

StoredPage::~StoredPage() {
  if (!cleared_observers_)
    ClearAllObservers();
}

void StoredPage::SetDelegate(Delegate* delegate) {
  delegate_ = delegate;
}

void StoredPage::ClearAllObservers() {
  DCHECK(!cleared_observers_);
  cleared_observers_ = true;
  for (const auto& rvh : render_view_hosts_) {
    rvh->site_instance_group()->RemoveObserver(this);
  }
}

void StoredPage::ActiveFrameCountIsZero(
    SiteInstanceGroup* site_instance_group) {
  // It is possible that this class was constructed with `RenderViewHost`
  // that may disappear when a outstanding subframe navigation completes.
  // `ActivateFrameCountIsZero` will be called before that happens so we detect
  // that a `RenderViewHost` is going to be destroyed and remove it from our
  // lists. Likewise if a `SiteInstanceGroup` has zero frames it means that we
  // don't require any proxies to it anymore.
  auto site_instance_group_id = site_instance_group->GetId();
  // Remove any proxies.
  auto proxy_iter = proxy_hosts_.find(site_instance_group_id);
  if (proxy_iter != proxy_hosts_.end())
    proxy_hosts_.erase(proxy_iter);

  for (auto iter = render_view_hosts_.begin();
       iter != render_view_hosts_.end();) {
    if ((*iter)->site_instance_group() == site_instance_group) {
      if (delegate_)
        delegate_->RenderViewHostNoLongerStored(&*(*iter));
      iter = render_view_hosts_.erase(iter);
    } else {
      ++iter;
    }
  }

  site_instance_group->RemoveObserver(this);
}

void StoredPage::PrepareToRestore() {
  ClearAllObservers();
}

std::unique_ptr<RenderFrameHostImpl> StoredPage::TakeRenderFrameHost() {
  return std::move(render_frame_host_);
}

StoredPage::RenderFrameProxyHostMap StoredPage::TakeProxyHosts() {
  DCHECK(cleared_observers_);
  return std::move(proxy_hosts_);
}

StoredPage::RenderViewHostImplSafeRefSet StoredPage::TakeRenderViewHosts() {
  DCHECK(cleared_observers_);
  return std::move(render_view_hosts_);
}

void StoredPage::SetViewTransitionState(
    std::optional<blink::ViewTransitionState> view_transition_state) {
  DCHECK(!view_transition_state_);
  view_transition_state_ = std::move(view_transition_state);
}

std::optional<blink::ViewTransitionState>
StoredPage::TakeViewTransitionState() {
  return std::exchange(view_transition_state_, {});
}

}  // namespace content
