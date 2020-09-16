// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/did_commit_navigation_interceptor.h"

#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/common/frame.mojom-test-utils.h"
#include "content/common/frame_messages.h"
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

  ~FrameAgent() override {
    rfhi_->SetCommitCallbackInterceptorForTesting(nullptr);
  }

  bool WillProcessDidCommitNavigation(
      NavigationRequest* navigation_request,
      ::FrameHostMsg_DidCommitProvisionalLoad_Params* params,
      mojom::DidCommitProvisionalLoadInterfaceParamsPtr* interface_params)
      override {
    return interceptor_->WillProcessDidCommitNavigation(
        rfhi_, navigation_request, params, interface_params);
  }

 private:
  DidCommitNavigationInterceptor* interceptor_;

  RenderFrameHostImpl* rfhi_;

  DISALLOW_COPY_AND_ASSIGN(FrameAgent);
};

DidCommitNavigationInterceptor::DidCommitNavigationInterceptor(
    WebContents* web_contents)
    : WebContentsObserver(web_contents) {
  for (auto* rfh : web_contents->GetAllFrames()) {
    if (rfh->IsRenderFrameLive())
      RenderFrameCreated(rfh);
  }
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

CommitMessageDelayer::CommitMessageDelayer(WebContents* web_contents,
                                           const GURL& deferred_url,
                                           DidCommitCallback deferred_action)
    : DidCommitNavigationInterceptor(web_contents),
      deferred_url_(deferred_url),
      deferred_action_(std::move(deferred_action)) {}

CommitMessageDelayer::~CommitMessageDelayer() = default;

void CommitMessageDelayer::Wait() {
  run_loop_ = std::make_unique<base::RunLoop>();
  run_loop_->Run();
  run_loop_.reset();
}

bool CommitMessageDelayer::WillProcessDidCommitNavigation(
    RenderFrameHost* render_frame_host,
    NavigationRequest* navigation_request,
    ::FrameHostMsg_DidCommitProvisionalLoad_Params* params,
    mojom::DidCommitProvisionalLoadInterfaceParamsPtr* interface_params) {
  if (params->url == deferred_url_) {
    std::move(deferred_action_).Run(render_frame_host);
    if (run_loop_)
      run_loop_->Quit();
  }
  return true;
}

}  // namespace content
