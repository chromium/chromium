// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/did_commit_navigation_interceptor.h"

#include "base/memory/raw_ptr.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/common/frame.mojom-test-utils.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

namespace content {

// Responsible for intercepting DidCommitNavigation's being processed by
// a given RenderFrameHostImpl.
class DidCommitNavigationInterceptor::FrameAgent
    : public RenderFrameHostImpl::CommitCallbackInterceptor {
 public:
  FrameAgent(DidCommitNavigationInterceptor* interceptor, RenderFrameHost* rfh)
      : interceptor_(interceptor),
        rfhi_(static_cast<RenderFrameHostImpl*>(rfh)) {
    rfhi_->SetCommitCallbackInterceptorForTesting(this);
  }

  FrameAgent(const FrameAgent&) = delete;
  FrameAgent& operator=(const FrameAgent&) = delete;

  ~FrameAgent() override {
    rfhi_->SetCommitCallbackInterceptorForTesting(nullptr);
  }

  bool WillProcessDidCommitNavigation(
      NavigationRequest* navigation_request,
      mojom::DidCommitProvisionalLoadParamsPtr* params,
      mojom::DidCommitProvisionalLoadInterfaceParamsPtr* interface_params)
      override {
    return interceptor_->WillProcessDidCommitNavigation(
        rfhi_, navigation_request, params, interface_params);
  }

 private:
  raw_ptr<DidCommitNavigationInterceptor> interceptor_;

  raw_ptr<RenderFrameHostImpl> rfhi_;
};

DidCommitNavigationInterceptor::DidCommitNavigationInterceptor(
    WebContents* web_contents)
    : WebContentsObserver(web_contents) {
  web_contents->ForEachRenderFrameHost(
      [this](RenderFrameHost* render_frame_host) {
        if (render_frame_host->IsRenderFrameLive())
          RenderFrameCreated(render_frame_host);
      });
}

DidCommitNavigationInterceptor::~DidCommitNavigationInterceptor() = default;

void DidCommitNavigationInterceptor::RenderFrameCreated(
    RenderFrameHost* render_frame_host) {
  bool did_insert;
  std::tie(std::ignore, did_insert) = frame_agents_.emplace(
      render_frame_host, std::make_unique<FrameAgent>(this, render_frame_host));
  DCHECK(did_insert);
}

void DidCommitNavigationInterceptor::RenderFrameDeleted(
    RenderFrameHost* render_frame_host) {
  bool did_remove = !!frame_agents_.erase(render_frame_host);
  DCHECK(did_remove);
}

}  // namespace content
