// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/base_switches.h"
#include "base/test/scoped_feature_list.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/smart_card_delegate.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/content_mock_cert_verifier.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/device/public/mojom/smart_card.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features_generated.h"

using device::mojom::SmartCardReaderInfo;
using device::mojom::SmartCardReaderInfoPtr;
using device::mojom::SmartCardReaderState;

namespace content {

namespace {

class FakeSmartCardDelegate : public SmartCardDelegate {
 public:
  void GetReaders(GetReadersCallback) override;
  bool SupportsReaderAddedRemovedNotifications() const override { return true; }

  bool AddReader(SmartCardReaderInfoPtr reader_info);
  bool RemoveReader(const std::string& name);

 private:
  std::unordered_map<std::string, SmartCardReaderInfoPtr> reader_infos_;
};

class SmartCardTestContentBrowserClient : public ContentBrowserClient {
 public:
  SmartCardTestContentBrowserClient();
  SmartCardTestContentBrowserClient(SmartCardTestContentBrowserClient&) =
      delete;
  SmartCardTestContentBrowserClient& operator=(
      SmartCardTestContentBrowserClient&) = delete;
  ~SmartCardTestContentBrowserClient() override;

  FakeSmartCardDelegate& delegate() { return delegate_; }

  // ContentBrowserClient:
  SmartCardDelegate* GetSmartCardDelegate(
      content::BrowserContext* browser_context) override;
  bool ShouldUrlUseApplicationIsolationLevel(BrowserContext* browser_context,
                                             const GURL& url,
                                             bool origin_matches_flag) override;
  absl::optional<blink::ParsedPermissionsPolicy>
  GetPermissionsPolicyForIsolatedWebApp(
      content::BrowserContext* browser_context,
      const url::Origin& app_origin) override;

 private:
  FakeSmartCardDelegate delegate_;
};

class SmartCardTest : public ContentBrowserTest {
 public:
  FakeSmartCardDelegate& delegate() { return test_client_.delegate(); }

  GURL GetIsolatedContextUrl() {
    return https_server_.GetURL(
        "a.com",
        "/set-header?Cross-Origin-Opener-Policy: same-origin&"
        "Cross-Origin-Embedder-Policy: require-corp&"
        "Permissions-Policy: smart-card%3D(self)");
  }

  bool AddReader(const std::string& name) {
    std::vector<uint8_t> atr = {1, 2, 3, 4};
    SmartCardReaderInfoPtr reader =
        SmartCardReaderInfo::New(name, SmartCardReaderState::kEmpty, atr);
    return delegate().AddReader(std::move(reader));
  }

  bool RemoveReader(const std::string& name) {
    return delegate().RemoveReader(name);
  }

 private:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ContentBrowserTest::SetUpCommandLine(command_line);
    mock_cert_verifier_.SetUpCommandLine(command_line);
  }

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();

    scoped_setting_ =
        std::make_unique<content::ScopedContentBrowserClientSetting>(
            &test_client_);

    mock_cert_verifier_.mock_cert_verifier()->set_default_result(net::OK);

    // Serve a.com (and any other domain).
    host_resolver()->AddRule("*", "127.0.0.1");

    // Add a handler for the "/set-header" page (among others)
    https_server_.AddDefaultHandlers(GetTestDataFilePath());

    ASSERT_TRUE(https_server_.Start());
  }

  void SetUpInProcessBrowserTestFixture() override {
    ContentBrowserTest::SetUpInProcessBrowserTestFixture();
    mock_cert_verifier_.SetUpInProcessBrowserTestFixture();
  }

  void TearDownInProcessBrowserTestFixture() override {
    ContentBrowserTest::TearDownInProcessBrowserTestFixture();
    mock_cert_verifier_.TearDownInProcessBrowserTestFixture();
  }

  void TearDown() override {
    ASSERT_TRUE(https_server_.ShutdownAndWaitUntilComplete());
    ContentBrowserTest::TearDown();
  }

  SmartCardTestContentBrowserClient test_client_;
  std::unique_ptr<ScopedContentBrowserClientSetting> scoped_setting_;

  // Need a mock CertVerifier for HTTPS connections to succeed with the test
  // server.
  ContentMockCertVerifier mock_cert_verifier_;

  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};

  base::test::ScopedFeatureList scoped_feature_list_{
      blink::features::kSmartCard};
};
}  // namespace

SmartCardTestContentBrowserClient::SmartCardTestContentBrowserClient() =
    default;

SmartCardTestContentBrowserClient::~SmartCardTestContentBrowserClient() =
    default;

SmartCardDelegate* SmartCardTestContentBrowserClient::GetSmartCardDelegate(
    content::BrowserContext* browser_context) {
  return &delegate_;
}

bool SmartCardTestContentBrowserClient::ShouldUrlUseApplicationIsolationLevel(
    BrowserContext* browser_context,
    const GURL& url,
    bool origin_matches_flag) {
  return true;
}

absl::optional<blink::ParsedPermissionsPolicy>
SmartCardTestContentBrowserClient::GetPermissionsPolicyForIsolatedWebApp(
    content::BrowserContext* browser_context,
    const url::Origin& app_origin) {
  blink::ParsedPermissionsPolicy out;
  blink::ParsedPermissionsPolicyDeclaration decl(
      blink::mojom::PermissionsPolicyFeature::kSmartCard,
      /*allowed_origins=*/
      {blink::OriginWithPossibleWildcards(app_origin,
                                          /*has_subdomain_wildcard=*/false)},
      /*matches_all_origins=*/false, /*matches_opaque_src=*/false);
  out.push_back(decl);
  return out;
}

void FakeSmartCardDelegate::GetReaders(GetReadersCallback callback) {
  std::vector<SmartCardReaderInfoPtr> readers;
  readers.reserve(reader_infos_.size());

  for (auto& reader : reader_infos_) {
    readers.push_back(reader.second->Clone());
  }

  std::move(callback).Run(std::move(readers));
}

bool FakeSmartCardDelegate::AddReader(SmartCardReaderInfoPtr reader_info) {
  if (reader_infos_.count(reader_info->name) > 0) {
    return false;
  }

  for (auto& observer : observer_list_) {
    observer.OnReaderAdded(*reader_info);
  }
  std::string name = reader_info->name;
  reader_infos_[name] = std::move(reader_info);

  return true;
}

bool FakeSmartCardDelegate::RemoveReader(const std::string& name) {
  auto node_handle = reader_infos_.extract(name);
  if (node_handle.empty()) {
    return false;
  }

  for (auto& observer : observer_list_) {
    observer.OnReaderRemoved(*node_handle.mapped());
  }

  return true;
}

IN_PROC_BROWSER_TEST_F(SmartCardTest, GetReaders) {
  ASSERT_TRUE(AddReader("Fake Reader"));

  ASSERT_TRUE(NavigateToURL(shell(), GetIsolatedContextUrl()));

  EXPECT_EQ(true, EvalJs(shell(),
                         R"((async () => {
               let readers = await navigator.smartCard.getReaders();

               return readers.length == 1 && readers[0].name == "Fake Reader";
             })())"));
}

IN_PROC_BROWSER_TEST_F(SmartCardTest, ReaderAdd) {
  ASSERT_TRUE(NavigateToURL(shell(), GetIsolatedContextUrl()));

  EXPECT_TRUE(ExecJs(shell(), R"((async () => {
    let observer = await navigator.smartCard.watchForReaders();
    window.promise = new Promise((resolve) => {
      observer.addEventListener('readeradd', (e) => {
        resolve(e.reader.name);
      }, { once: true });
    });
  })())"));

  ASSERT_TRUE(AddReader("New Fake Reader"));

  EXPECT_EQ("New Fake Reader", EvalJs(shell(), "window.promise"));
}

IN_PROC_BROWSER_TEST_F(SmartCardTest, ReaderRemove) {
  const std::string reader_name = "Fake Reader";
  ASSERT_TRUE(AddReader(reader_name));

  ASSERT_TRUE(NavigateToURL(shell(), GetIsolatedContextUrl()));

  EXPECT_TRUE(ExecJs(shell(), R"((async () => {
    let observer = await navigator.smartCard.watchForReaders();
    window.promise = new Promise((resolve) => {
      observer.addEventListener('readerremove', (e) => {
        resolve(e.reader.name);
      }, { once: true });
    });
  })())"));

  ASSERT_TRUE(RemoveReader(reader_name));

  EXPECT_EQ(reader_name, EvalJs(shell(), "window.promise"));
}

}  // namespace content
