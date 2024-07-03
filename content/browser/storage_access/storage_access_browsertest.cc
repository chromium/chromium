// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/bind.h"
#include "content/browser/permissions/permission_controller_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_content_browser_client.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/functions.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/blink/public/mojom/storage_access/storage_access_handle.mojom.h"

namespace {
class MockContentBrowserClient final
    : public content::ContentBrowserTestContentBrowserClient {
 public:
  bool IsFullCookieAccessAllowed(
      content::BrowserContext* browser_context,
      content::WebContents* web_contents,
      const GURL& url,
      const blink::StorageKey& storage_key) override {
    return is_full_cookie_access_allowed_;
  }

  void set_is_full_cookie_access_allowed(bool enabled) {
    is_full_cookie_access_allowed_ = enabled;
  }

 private:
  bool is_full_cookie_access_allowed_{false};
};
}  // namespace

namespace content {

class StorageAccessBrowserTest : public ContentBrowserTest {
 public:
  void SetUpOnMainThread() override {
    client_ = std::make_unique<MockContentBrowserClient>();
  }

  void TearDownOnMainThread() override { client_.reset(); }

 protected:
  void BindStorageAccessHandleAndExpect(bool is_connected,
                                        std::string expected_error) {
    // Setup message interceptor.
    std::string received_error;
    mojo::SetDefaultProcessErrorHandler(
        base::BindLambdaForTesting([&](const std::string& error) {
          ASSERT_EQ(received_error, "");
          received_error = error;
        }));

    // Load website.
    EXPECT_TRUE(embedded_test_server()->Start());
    EXPECT_TRUE(NavigateToURL(
        shell(), embedded_test_server()->GetURL("/simple_page.html")));

    // We need access to the interface broker to test bad messages, so must
    // unbind the existing one and bind our own.
    EXPECT_TRUE(
        host()->browser_interface_broker_receiver_for_testing().Unbind());
    mojo::Remote<blink::mojom::BrowserInterfaceBroker> broker_remote;
    mojo::PendingReceiver<blink::mojom::BrowserInterfaceBroker>
        broker_receiver = broker_remote.BindNewPipeAndPassReceiver();
    host()->BindBrowserInterfaceBrokerReceiver(std::move(broker_receiver));

    // Try to bind our StorageAccessHandle.
    mojo::Remote<blink::mojom::StorageAccessHandle> storage_remote;
    broker_remote->GetInterface(storage_remote.BindNewPipeAndPassReceiver());
    broker_remote.FlushForTesting();
    EXPECT_EQ(storage_remote.is_connected(), is_connected);
    EXPECT_EQ(received_error, expected_error);

    // Cleanup message interceptor.
    mojo::SetDefaultProcessErrorHandler(base::NullCallback());
  }

  void set_is_full_cookie_access_allowed(bool is_full_cookie_access_allowed) {
    client_->set_is_full_cookie_access_allowed(is_full_cookie_access_allowed);
  }

  void set_storage_access_permission_status(
      const blink::mojom::PermissionStatus& status) {
    static_cast<PermissionControllerImpl*>(
        host()->GetBrowserContext()->GetPermissionController())
        ->SetPermissionOverride(
            std::nullopt, blink::PermissionType::STORAGE_ACCESS_GRANT, status);
  }

  RenderFrameHostImpl* host() {
    return static_cast<RenderFrameHostImpl*>(
        static_cast<WebContentsImpl*>(shell()->web_contents())
            ->GetPrimaryFrameTree()
            .root()
            ->current_frame_host());
  }

 private:
  std::unique_ptr<MockContentBrowserClient> client_;
};

IN_PROC_BROWSER_TEST_F(StorageAccessBrowserTest, WithCookiesWithPermission) {
  set_is_full_cookie_access_allowed(true);
  set_storage_access_permission_status(blink::mojom::PermissionStatus::GRANTED);
  BindStorageAccessHandleAndExpect(/*is_connected=*/true, "");
}

IN_PROC_BROWSER_TEST_F(StorageAccessBrowserTest, WithCookiesWithoutPermission) {
  set_is_full_cookie_access_allowed(true);
  set_storage_access_permission_status(blink::mojom::PermissionStatus::DENIED);
  BindStorageAccessHandleAndExpect(/*is_connected=*/true, "");
}

IN_PROC_BROWSER_TEST_F(StorageAccessBrowserTest, WithoutCookiesWithPermission) {
  set_is_full_cookie_access_allowed(false);
  set_storage_access_permission_status(blink::mojom::PermissionStatus::GRANTED);
  BindStorageAccessHandleAndExpect(/*is_connected=*/true, "");
}

IN_PROC_BROWSER_TEST_F(StorageAccessBrowserTest,
                       WithoutCookiesWithoutPermission) {
  set_is_full_cookie_access_allowed(false);
  set_storage_access_permission_status(blink::mojom::PermissionStatus::DENIED);
#if DCHECK_IS_ON()
  BindStorageAccessHandleAndExpect(
      /*is_connected=*/false,
      "Binding a StorageAccessHandle requires third-party cookie access or "
      "permission access.");
#else
  BindStorageAccessHandleAndExpect(/*is_connected=*/false, "");
#endif
}

}  // namespace content
