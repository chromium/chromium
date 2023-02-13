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
#include "content/public/test/content_browser_test_content_browser_client.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/content_mock_cert_verifier.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features_generated.h"
#include "third_party/blink/public/mojom/smart_card/smart_card.mojom.h"

using blink::mojom::SmartCardReaderInfo;
using blink::mojom::SmartCardReaderInfoPtr;
using blink::mojom::SmartCardReaderState;
using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Return;

namespace content {

namespace {

class MockSmartCardDelegate : public SmartCardDelegate {
 public:
  MOCK_METHOD(void, GetReaders, (GetReadersCallback), (override));
  MOCK_METHOD(bool,
              SupportsReaderAddedRemovedNotifications,
              (),
              (const, override));
};

class FakeSmartCardDelegate : public SmartCardDelegate {
 public:
  void GetReaders(GetReadersCallback) override;
  bool SupportsReaderAddedRemovedNotifications() const override { return true; }

  bool AddReader(const std::string& name) {
    std::vector<uint8_t> atr = {1, 2, 3, 4};
    SmartCardReaderInfoPtr reader =
        SmartCardReaderInfo::New(name, SmartCardReaderState::kEmpty, atr);
    return AddReader(std::move(reader));
  }
  bool AddReader(SmartCardReaderInfoPtr reader_info);

  bool RemoveReader(const std::string& name);

 private:
  std::unordered_map<std::string, SmartCardReaderInfoPtr> reader_infos_;
};

class SmartCardTestContentBrowserClient
    : public ContentBrowserTestContentBrowserClient {
 public:
  SmartCardTestContentBrowserClient();
  SmartCardTestContentBrowserClient(SmartCardTestContentBrowserClient&) =
      delete;
  SmartCardTestContentBrowserClient& operator=(
      SmartCardTestContentBrowserClient&) = delete;
  ~SmartCardTestContentBrowserClient() override;

  void SetSmartCardDelegate(std::unique_ptr<SmartCardDelegate>);

  // ContentBrowserClient:
  SmartCardDelegate* GetSmartCardDelegate(
      content::BrowserContext* browser_context) override;
  bool ShouldUrlUseApplicationIsolationLevel(BrowserContext* browser_context,
                                             const GURL& url) override;
  absl::optional<blink::ParsedPermissionsPolicy>
  GetPermissionsPolicyForIsolatedWebApp(
      content::BrowserContext* browser_context,
      const url::Origin& app_origin) override;

 private:
  std::unique_ptr<SmartCardDelegate> delegate_;
};

class SmartCardTest : public ContentBrowserTest {
 public:
  GURL GetIsolatedContextUrl() {
    return https_server_.GetURL(
        "a.com",
        "/set-header?Cross-Origin-Opener-Policy: same-origin&"
        "Cross-Origin-Embedder-Policy: require-corp&"
        "Permissions-Policy: smart-card%3D(self)");
  }

  FakeSmartCardDelegate* CreateFakeSmartCardDelegate() {
    auto unique_delegate = std::make_unique<FakeSmartCardDelegate>();
    FakeSmartCardDelegate* delegate = unique_delegate.get();
    test_client_->SetSmartCardDelegate(std::move(unique_delegate));
    return delegate;
  }

  MockSmartCardDelegate* CreateMockSmartCardDelegate() {
    auto unique_delegate = std::make_unique<MockSmartCardDelegate>();
    MockSmartCardDelegate* delegate = unique_delegate.get();
    test_client_->SetSmartCardDelegate(std::move(unique_delegate));
    ON_CALL(*delegate, SupportsReaderAddedRemovedNotifications)
        .WillByDefault(Return(true));
    return delegate;
  }

 private:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ContentBrowserTest::SetUpCommandLine(command_line);
    mock_cert_verifier_.SetUpCommandLine(command_line);
  }

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();

    test_client_ = std::make_unique<SmartCardTestContentBrowserClient>();

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

  std::unique_ptr<SmartCardTestContentBrowserClient> test_client_;

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
  return delegate_.get();
}

void SmartCardTestContentBrowserClient::SetSmartCardDelegate(
    std::unique_ptr<SmartCardDelegate> delegate) {
  delegate_ = std::move(delegate);
}

bool SmartCardTestContentBrowserClient::ShouldUrlUseApplicationIsolationLevel(
    BrowserContext* browser_context,
    const GURL& url) {
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

  std::move(callback).Run(
      blink::mojom::SmartCardGetReadersResult::NewReaders(std::move(readers)));
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
  FakeSmartCardDelegate* delegate = CreateFakeSmartCardDelegate();

  ASSERT_TRUE(delegate->AddReader("Fake Reader"));

  ASSERT_TRUE(NavigateToURL(shell(), GetIsolatedContextUrl()));

  EXPECT_EQ(true, EvalJs(shell(),
                         R"((async () => {
               let readers = await navigator.smartCard.getReaders();

               return readers.length == 1 && readers[0].name == "Fake Reader";
             })())"));
}

IN_PROC_BROWSER_TEST_F(SmartCardTest, ReaderAdd) {
  FakeSmartCardDelegate* delegate = CreateFakeSmartCardDelegate();

  ASSERT_TRUE(NavigateToURL(shell(), GetIsolatedContextUrl()));

  EXPECT_TRUE(ExecJs(shell(), R"((async () => {
    let observer = await navigator.smartCard.watchForReaders();
    window.promise = new Promise((resolve) => {
      observer.addEventListener('readeradd', (e) => {
        resolve(e.reader.name);
      }, { once: true });
    });
  })())"));

  ASSERT_TRUE(delegate->AddReader("New Fake Reader"));

  EXPECT_EQ("New Fake Reader", EvalJs(shell(), "window.promise"));
}

IN_PROC_BROWSER_TEST_F(SmartCardTest, ReaderRemove) {
  FakeSmartCardDelegate* delegate = CreateFakeSmartCardDelegate();
  const std::string reader_name = "Fake Reader";

  ASSERT_TRUE(delegate->AddReader(reader_name));

  ASSERT_TRUE(NavigateToURL(shell(), GetIsolatedContextUrl()));

  EXPECT_TRUE(ExecJs(shell(), R"((async () => {
    let observer = await navigator.smartCard.watchForReaders();
    window.promise = new Promise((resolve) => {
      observer.addEventListener('readerremove', (e) => {
        resolve(e.reader.name);
      }, { once: true });
    });
  })())"));

  ASSERT_TRUE(delegate->RemoveReader(reader_name));

  EXPECT_EQ(reader_name, EvalJs(shell(), "window.promise"));
}

IN_PROC_BROWSER_TEST_F(SmartCardTest, GetReadersFails) {
  MockSmartCardDelegate* delegate = CreateMockSmartCardDelegate();

  EXPECT_CALL(*delegate, SupportsReaderAddedRemovedNotifications);

  EXPECT_CALL(*delegate, GetReaders(_))
      .WillRepeatedly([&](SmartCardDelegate::GetReadersCallback cb) {
        std::move(cb).Run(
            blink::mojom::SmartCardGetReadersResult::NewResponseCode(
                blink::mojom::SmartCardResponseCode::kNoService));
      });

  ASSERT_TRUE(NavigateToURL(shell(), GetIsolatedContextUrl()));

  EXPECT_EQ("SmartCardError: no-service", EvalJs(shell(), R"(
    (async () => {
      try {
        let readers = await navigator.smartCard.getReaders();
      } catch (e) {
        return `${e.name}: ${e.responseCode}`;
      }
    })()
  )"));
}

}  // namespace content
