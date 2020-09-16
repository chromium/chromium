// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_DID_COMMIT_NAVIGATION_INTERCEPTOR_H_
#define CONTENT_TEST_DID_COMMIT_NAVIGATION_INTERCEPTOR_H_

#include <map>
#include <memory>

#include "base/callback.h"
#include "base/macros.h"
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
  ~DidCommitNavigationInterceptor() override;

  // Called just before DidCommitNavigation with |navigation_request|, |params|
  // and |interface_provider_request| would be processed by
  // |render_frame_host|.
  // Return false to cancel the processing of this call by |render_frame_host|.
  virtual bool WillProcessDidCommitNavigation(
      RenderFrameHost* render_frame_host,
      NavigationRequest* navigation_request,
      ::FrameHostMsg_DidCommitProvisionalLoad_Params* params,
      mojom::DidCommitProvisionalLoadInterfaceParamsPtr* interface_params) = 0;

 private:
  class FrameAgent;

  // WebContentsObserver:
  void RenderFrameCreated(RenderFrameHost* render_frame_host) override;
  void RenderFrameDeleted(RenderFrameHost* render_frame_host) override;

  std::map<RenderFrameHost*, std::unique_ptr<FrameAgent>> frame_agents_;

  DISALLOW_COPY_AND_ASSIGN(DidCommitNavigationInterceptor);
};

// A helper class to run a predefined callback just before processing the
// DidCommitProvisionalLoad IPC for |deferred_url|.
class CommitMessageDelayer : public DidCommitNavigationInterceptor {
 public:
  using DidCommitCallback = base::OnceCallback<void(RenderFrameHost*)>;

  // Starts monitoring |web_contents| for DidCommit IPC and executes
  // |deferred_action| for each DidCommit IPC that matches |deferred_url|.
  explicit CommitMessageDelayer(WebContents* web_contents,
                                const GURL& deferred_url,
                                DidCommitCallback deferred_action);
  ~CommitMessageDelayer() override;

  // Waits until DidCommit IPC arrives for |deferred_url|, then calls
  // |deferred_action|, then handles the IPC, then returns.
  void Wait();

 private:
  // DidCommitNavigationInterceptor:
  bool WillProcessDidCommitNavigation(
      RenderFrameHost* render_frame_host,
      NavigationRequest* navigation_request,
      ::FrameHostMsg_DidCommitProvisionalLoad_Params* params,
      mojom::DidCommitProvisionalLoadInterfaceParamsPtr* interface_params)
      override;

  std::unique_ptr<base::RunLoop> run_loop_;

  const GURL deferred_url_;
  DidCommitCallback deferred_action_;

  DISALLOW_COPY_AND_ASSIGN(CommitMessageDelayer);
};

}  // namespace content

#endif  // CONTENT_TEST_DID_COMMIT_NAVIGATION_INTERCEPTOR_H_
