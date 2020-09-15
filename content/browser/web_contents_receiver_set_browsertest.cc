// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/web_contents_receiver_set.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/web_contents_receiver_set_test_binder.h"
#include "content/shell/browser/shell.h"
#include "content/test/test_browser_associated_interfaces.mojom.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "net/dns/mock_host_resolver.h"

namespace content {

namespace {

const char kTestHost1[] = "foo.com";
const char kTestHost2[] = "bar.com";

class WebContentsReceiverSetBrowserTest : public ContentBrowserTest {
 public:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule(kTestHost1, "127.0.0.1");
    host_resolver()->AddRule(kTestHost2, "127.0.0.1");
  }
};

class TestInterfaceBinder : public WebContentsReceiverSetTestBinder<
                                mojom::BrowserAssociatedInterfaceTestDriver> {
 public:
  explicit TestInterfaceBinder(base::OnceClosure bind_callback)
      : bind_callback_(std::move(bind_callback)) {}
  ~TestInterfaceBinder() override {}

  void BindReceiver(
      RenderFrameHost* frame_host,
      mojo::PendingAssociatedReceiver<
          mojom::BrowserAssociatedInterfaceTestDriver> receiver) override {
    std::move(bind_callback_).Run();
  }

 private:
  base::OnceClosure bind_callback_;

  DISALLOW_COPY_AND_ASSIGN(TestInterfaceBinder);
};

class TestFrameInterfaceBinder : public mojom::WebContentsFrameReceiverSetTest {
 public:
  explicit TestFrameInterfaceBinder(WebContents* web_contents)
      : receivers_(web_contents, this) {}
  ~TestFrameInterfaceBinder() override {}

 private:
  // mojom::WebContentsFrameReceiverSetTest:
  void Ping(PingCallback callback) override { NOTREACHED(); }

  WebContentsFrameReceiverSet<mojom::WebContentsFrameReceiverSetTest>
      receivers_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(WebContentsReceiverSetBrowserTest, OverrideForTesting) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL("data:text/html,ho hum")));

  // Ensure that we can add a WebContentsFrameReceiverSet and then override its
  // request handler.
  auto* web_contents = shell()->web_contents();
  WebContentsFrameReceiverSet<mojom::BrowserAssociatedInterfaceTestDriver>
      frame_receivers(web_contents, nullptr);

  // Now override the binder for this interface. It quits |run_loop| whenever
  // an incoming pending receiver is received.
  base::RunLoop run_loop;
  auto* receiver_set = WebContentsReceiverSet::GetForWebContents<
      mojom::BrowserAssociatedInterfaceTestDriver>(web_contents);

  TestInterfaceBinder test_binder(run_loop.QuitClosure());
  receiver_set->SetBinder(&test_binder);

  // Simulate an inbound receiver for the test interface. This should get routed
  // to the overriding binder and allow the test to complete.
  mojo::AssociatedRemote<mojom::BrowserAssociatedInterfaceTestDriver>
      override_client;
  static_cast<WebContentsImpl*>(web_contents)
      ->OnAssociatedInterfaceRequest(
          web_contents->GetMainFrame(),
          mojom::BrowserAssociatedInterfaceTestDriver::Name_,
          override_client.BindNewEndpointAndPassDedicatedReceiver()
              .PassHandle());
  run_loop.Run();

  receiver_set->SetBinder(nullptr);
}

IN_PROC_BROWSER_TEST_F(WebContentsReceiverSetBrowserTest,
                       CloseOnFrameDeletion) {
  EXPECT_TRUE(embedded_test_server()->Start());
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL(kTestHost1, "/hello.html")));

  // Simulate an inbound receiver on the navigated main frame.
  auto* web_contents = shell()->web_contents();
  TestFrameInterfaceBinder binder(web_contents);
  mojo::AssociatedRemote<mojom::WebContentsFrameReceiverSetTest>
      override_client;
  static_cast<WebContentsImpl*>(web_contents)
      ->OnAssociatedInterfaceRequest(
          web_contents->GetMainFrame(),
          mojom::WebContentsFrameReceiverSetTest::Name_,
          override_client.BindNewEndpointAndPassDedicatedReceiver()
              .PassHandle());

  base::RunLoop run_loop;
  override_client.set_disconnect_handler(run_loop.QuitClosure());

  // Now navigate the WebContents elsewhere, eventually tearing down the old
  // main frame.
  RenderFrameDeletedObserver deleted_observer(web_contents->GetMainFrame());

  // Test expects the old frame to be deleted on navigation, but it doesn't
  // happen as it is stored in bfcache.
  DisableBackForwardCacheForTesting(shell()->web_contents(),
                                    BackForwardCache::TEST_ASSUMES_NO_CACHING);

  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL(kTestHost2, "/title2.html")));
  deleted_observer.WaitUntilDeleted();

  // Verify that this message never reaches the binding for the old frame. If it
  // does, the impl will hit a DCHECK. The RunLoop terminates when the client is
  // disconnected.
  override_client->Ping(base::BindOnce([] {}));
  run_loop.Run();
}

}  // namespace content
