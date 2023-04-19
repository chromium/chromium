// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/commit_message_delayer.h"

#include <memory>
#include <utility>

#include "base/run_loop.h"
#include "content/common/navigation_client.mojom.h"
#include "content/test/did_commit_navigation_interceptor.h"
#include "url/gurl.h"

namespace content {

class CommitMessageDelayer::Impl : public DidCommitNavigationInterceptor {
 public:
  Impl(WebContents* web_contents,
       const GURL& deferred_url,
       DidCommitCallback deferred_action)
      : DidCommitNavigationInterceptor(web_contents),
        deferred_url_(deferred_url),
        deferred_action_(std::move(deferred_action)) {}

  ~Impl() override = default;

  Impl(const Impl&) = delete;
  Impl& operator=(const Impl&) = delete;

  void Wait() {
    CHECK(deferred_action_)
        << "The deferred action was already run before calling Wait().";
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
    run_loop_.reset();
  }

 private:
  // DidCommitNavigationInterceptor override:
  bool WillProcessDidCommitNavigation(
      RenderFrameHost* render_frame_host,
      NavigationRequest* navigation_request,
      mojom::DidCommitProvisionalLoadParamsPtr* params,
      mojom::DidCommitProvisionalLoadInterfaceParamsPtr* interface_params)
      override {
    if ((**params).url == deferred_url_) {
      std::move(deferred_action_).Run(render_frame_host);
      if (run_loop_)
        run_loop_->Quit();
    }
    return true;
  }

  std::unique_ptr<base::RunLoop> run_loop_;

  const GURL deferred_url_;
  DidCommitCallback deferred_action_;
};

CommitMessageDelayer::CommitMessageDelayer(WebContents* web_contents,
                                           const GURL& deferred_url,
                                           DidCommitCallback deferred_action) {
  impl_ = std::make_unique<Impl>(web_contents, deferred_url,
                                 std::move(deferred_action));
}

CommitMessageDelayer::~CommitMessageDelayer() = default;

void CommitMessageDelayer::Wait() {
  impl_->Wait();
}

}  // namespace content
