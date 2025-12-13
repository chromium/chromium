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
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/storage_access/storage_access_handle.mojom.h"

namespace {
class MockContentBrowserClient final
    : public content::ContentBrowserTestContentBrowserClient {
 public:
  explicit MockContentBrowserClient(bool is_full_cookie_access_allowed)
      : is_full_cookie_access_allowed_(is_full_cookie_access_allowed) {}

  bool IsFullCookieAccessAllowed(
      content::BrowserContext* browser_context,
      content::WebContents* web_contents,
      const GURL& url,
      const blink::StorageKey& storage_key,
      net::CookieSettingOverrides overrides) override {
    return is_full_cookie_access_allowed_;
  }

 private:
  const bool is_full_cookie_access_allowed_{false};
};
}  // namespace

namespace content {

class StorageAccessBrowserTest : public ContentBrowserTest,
                                 public testing::WithParamInterface<bool> {
 public:
  void SetUpOnMainThread() override {
    client_ =
        std::make_unique<MockContentBrowserClient>(is_cookie_access_allowed());
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_https_test_server().SetSSLConfig(
        net::EmbeddedTestServer::CERT_TEST_NAMES);
    embedded_https_test_server().ServeFilesFromSourceDirectory(
        "content/test/data");
    ASSERT_TRUE(embedded_https_test_server().Start());
  }

  void TearDownOnMainThread() override { client_.reset(); }

  bool is_cookie_access_allowed() const { return GetParam(); }

  base::expected<void, std::optional<std::string>> expected_handle_result()
      const {
    if (is_cookie_access_allowed()) {
      return base::ok();
    }

    if constexpr (DCHECK_IS_ON()) {
      return base::unexpected(
          "Binding a StorageAccessHandle requires third-party cookie access.");
    }
    return base::unexpected(std::nullopt);
  }

 protected:
  [[nodiscard]] base::expected<void, std::optional<std::string>>
  BindStorageAccessHandle() {
    // Setup message interceptor.
    std::optional<std::string> received_error;
    mojo::SetDefaultProcessErrorHandler(
        base::BindLambdaForTesting([&](const std::string& error) {
          ASSERT_EQ(received_error, std::nullopt);
          received_error = error;
        }));

    // Load website.
    EXPECT_TRUE(NavigateToURL(shell(), embedded_https_test_server().GetURL(
                                           "a.test", "/simple_page.html")));

    // We need access to the interface broker to test bad messages, so must
    // unbind the existing one and bind our own.
    EXPECT_TRUE(host()->ResetBrowserInterfaceBrokerReceiverForTesting());
    mojo::Remote<blink::mojom::BrowserInterfaceBroker> broker_remote;
    mojo::PendingReceiver<blink::mojom::BrowserInterfaceBroker>
        broker_receiver = broker_remote.BindNewPipeAndPassReceiver();
    host()->BindBrowserInterfaceBrokerReceiver(std::move(broker_receiver));

    // Try to bind our StorageAccessHandle.
    mojo::Remote<blink::mojom::StorageAccessHandle> storage_remote;
    broker_remote->GetInterface(storage_remote.BindNewPipeAndPassReceiver());
    broker_remote.FlushForTesting();

    // Cleanup message interceptor.
    mojo::SetDefaultProcessErrorHandler(base::NullCallback());

    if (received_error || !storage_remote.is_connected()) {
      return base::unexpected(received_error);
    }
    return base::ok();
  }

  [[nodiscard]] bool BindDomStorage() {
    // Load website with third-party iframe.
    CHECK(NavigateToURL(
        shell(),
        embedded_https_test_server().GetURL(
            "a.test", "/cross_site_iframe_factory.html?a.test(b.test)")));
    FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                              ->GetPrimaryFrameTree()
                              .root();
    CHECK_EQ(1U, root->child_count());
    FrameTreeNode* child = root->child_at(0);
    CHECK(child->current_frame_host()->GetStorageKey().IsThirdPartyContext());

    // We should always be able to load the area for the frame's storage key.
    mojo::Remote<blink::mojom::StorageArea> third_party_remote;
    child->current_frame_host()
        ->GetStoragePartition()
        ->GetDOMStorageContext()
        ->OpenLocalStorage(
            child->current_frame_host()->GetStorageKey(),
            child->current_frame_host()->GetFrameToken(),
            third_party_remote.BindNewPipeAndPassReceiver(),
            ChildProcessSecurityPolicyImpl::GetInstance()->CreateHandle(
                child->current_frame_host()->GetProcess()->GetDeprecatedID()),
            base::DoNothing());
    third_party_remote.FlushForTesting();
    EXPECT_TRUE(third_party_remote.is_connected());

    // We might be able to bind a first-party storage area too.
    mojo::Remote<blink::mojom::StorageArea> first_party_remote;
    child->current_frame_host()
        ->GetStoragePartition()
        ->GetDOMStorageContext()
        ->OpenLocalStorage(
            blink::StorageKey::CreateFirstParty(
                child->current_frame_host()->GetStorageKey().origin()),
            child->current_frame_host()->GetFrameToken(),
            first_party_remote.BindNewPipeAndPassReceiver(),
            ChildProcessSecurityPolicyImpl::GetInstance()->CreateHandle(
                child->current_frame_host()->GetProcess()->GetDeprecatedID()),
            base::DoNothing());
    first_party_remote.FlushForTesting();
    return first_party_remote.is_connected();
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

IN_PROC_BROWSER_TEST_P(StorageAccessBrowserTest, BindStorageAccessHandle) {
  EXPECT_EQ(BindStorageAccessHandle(), expected_handle_result());
}

IN_PROC_BROWSER_TEST_P(StorageAccessBrowserTest, BindDomStorage) {
  EXPECT_EQ(BindDomStorage(), is_cookie_access_allowed());
}

INSTANTIATE_TEST_SUITE_P(, StorageAccessBrowserTest, testing::Bool());

}  // namespace content
