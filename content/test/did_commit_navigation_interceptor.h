// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_DID_COMMIT_NAVIGATION_INTERCEPTOR_H_
#define CONTENT_TEST_DID_COMMIT_NAVIGATION_INTERCEPTOR_H_

#include <map>
#include <memory>

#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "content/common/frame.mojom.h"
#include "content/public/browser/web_contents_observer.h"
#include "url/gurl.h"

namespace content {

class RenderFrameHost;
class NavigationRequest;

// Allows intercepting calls to RenderFrameHostImpl::DidCommitNavigation just
// before they are processed by the implementation. This enables unit/browser
// tests to scrutinize/alter the parameters, or simulate race conditions by
// triggering other calls just before processing DidCommitProvisionalLoad.
class DidCommitNavigationInterceptor : public WebContentsObserver {
 public:
  // Constructs an instance that will intercept DidCommitProvisionalLoad calls
  // in any frame of the |web_contents| while the instance is in scope.
  explicit DidCommitNavigationInterceptor(WebContents* web_contents);

  DidCommitNavigationInterceptor(const DidCommitNavigationInterceptor&) =
      delete;
  DidCommitNavigationInterceptor& operator=(
      const DidCommitNavigationInterceptor&) = delete;

  ~DidCommitNavigationInterceptor() override;

  // Called just before DidCommitNavigation with |navigation_request|, |params|
  // and |interface_provider_request| would be processed by |render_frame_host|.
  // Return false to cancel the processing of this call by |render_frame_host|.
  // |params| and |interface_params| can be modified. When returning false, they
  // can also be consumed, they won't be used anymore by the caller.
  virtual bool WillProcessDidCommitNavigation(
      RenderFrameHost* render_frame_host,
      NavigationRequest* navigation_request,
      mojom::DidCommitProvisionalLoadParamsPtr* params,
      mojom::DidCommitProvisionalLoadInterfaceParamsPtr* interface_params) = 0;

 private:
  class FrameAgent;

  // WebContentsObserver:
  void RenderFrameCreated(RenderFrameHost* render_frame_host) override;
  void RenderFrameDeleted(RenderFrameHost* render_frame_host) override;

  std::map<RenderFrameHost*, std::unique_ptr<FrameAgent>> frame_agents_;
};

}  // namespace content

#endif  // CONTENT_TEST_DID_COMMIT_NAVIGATION_INTERCEPTOR_H_
