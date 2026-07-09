// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_observation.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/worker_host/dedicated_worker_host.h"
#include "content/browser/worker_host/dedicated_worker_service_impl.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/dns/mock_host_resolver.h"
#include "third_party/blink/public/common/features.h"

namespace content {

class DedicatedWorkerHostNavigationTest : public ContentBrowserTest {
 public:
  DedicatedWorkerHostNavigationTest() {
    feature_list_.InitAndDisableFeature(features::kRenderDocument);
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
    shell()
        ->web_contents()
        ->GetController()
        .GetBackForwardCache()
        .DisableForTesting(BackForwardCache::DisableForTestingReason::
                               TEST_REQUIRES_NO_CACHING);
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

class DedicatedWorkerHostGrabber : public DedicatedWorkerService::Observer {
 public:
  explicit DedicatedWorkerHostGrabber(DedicatedWorkerService* service) {
    observation_.Observe(service);
  }

  void OnWorkerCreated(const blink::DedicatedWorkerToken& token,
                       ChildProcessId worker_process_id,
                       const url::Origin& security_origin,
                       DedicatedWorkerCreator creator) override {
    last_worker_token_ = token;
    if (run_loop_) {
      run_loop_->Quit();
    }
  }

  void OnBeforeWorkerDestroyed(const blink::DedicatedWorkerToken& token,
                               DedicatedWorkerCreator creator) override {}

  void OnFinalResponseURLDetermined(const blink::DedicatedWorkerToken& token,
                                    const GURL& url) override {}

  const std::optional<blink::DedicatedWorkerToken>& last_worker_token() const {
    return last_worker_token_;
  }

  void Wait() {
    if (last_worker_token_) {
      return;
    }
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
  }

 private:
  std::optional<blink::DedicatedWorkerToken> last_worker_token_;
  std::unique_ptr<base::RunLoop> run_loop_;
  base::ScopedObservation<DedicatedWorkerService,
                          DedicatedWorkerService::Observer>
      observation_{this};
};

IN_PROC_BROWSER_TEST_F(DedicatedWorkerHostNavigationTest,
                       AncestorLookupAfterSameSiteNavigation) {
  GURL url_a = embedded_test_server()->GetURL("a.test", "/title1.html");
  GURL url_b = embedded_test_server()->GetURL("a.test", "/title2.html");

  // 1. Navigate to Document A.
  ASSERT_TRUE(content::NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = static_cast<RenderFrameHostImpl*>(
      shell()->web_contents()->GetPrimaryMainFrame());
  GlobalRenderFrameHostId rfh_id = rfh_a->GetGlobalId();
  blink::DocumentToken token_a = rfh_a->GetDocumentToken();

  // 2. Create a DedicatedWorker and grab its host.
  StoragePartition* partition = rfh_a->GetStoragePartition();
  DedicatedWorkerServiceImpl* service =
      static_cast<DedicatedWorkerServiceImpl*>(
          partition->GetDedicatedWorkerService());
  DedicatedWorkerHostGrabber grabber(service);

  ASSERT_TRUE(content::ExecJs(
      rfh_a,
      "var worker = new Worker('data:text/javascript,console.log(\"worker "
      "running\");');"));
  grabber.Wait();
  ASSERT_TRUE(grabber.last_worker_token().has_value());

  DedicatedWorkerHost* host =
      service->GetDedicatedWorkerHostFromToken(*grabber.last_worker_token());
  ASSERT_TRUE(host);

  EXPECT_EQ(host->GetAncestorRenderFrameHostId(), rfh_id);
  EXPECT_EQ(host->GetAncestorRenderFrameHost(), rfh_a);

  // 4. Navigate to Document B (same-site).
  ASSERT_TRUE(content::NavigateToURL(shell(), url_b));
  RenderFrameHostImpl* rfh_b = static_cast<RenderFrameHostImpl*>(
      shell()->web_contents()->GetPrimaryMainFrame());

  // 5. Verify that GetAncestorRenderFrameHost() now returns nullptr if the
  // document changed and the worker host has not yet been asynchronously
  // destroyed by the renderer unloading Document A.
  DedicatedWorkerHost* host_after_nav =
      service->GetDedicatedWorkerHostFromToken(*grabber.last_worker_token());
  if (host_after_nav) {
    if (rfh_a == rfh_b) {
      // RFH was reused.
      ASSERT_NE(token_a, rfh_b->GetDocumentToken());
      EXPECT_EQ(host_after_nav->GetAncestorRenderFrameHost(), nullptr);
    } else {
      // RFH was not reused.
      EXPECT_EQ(host_after_nav->GetAncestorRenderFrameHost(), nullptr);
    }
  }
}

}  // namespace content
