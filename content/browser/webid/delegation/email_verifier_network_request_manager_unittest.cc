// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/delegation/email_verifier_network_request_manager.h"

#include "base/check.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "content/public/browser/weak_document_ptr.h"
#include "content/public/test/test_renderer_host.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/client_security_state.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content::webid {

class EmailVerifierNetworkRequestManagerTest
    : public RenderViewHostTestHarness {
 public:
  EmailVerifierNetworkRequestManagerTest() = default;
  ~EmailVerifierNetworkRequestManagerTest() override = default;

 protected:
  void SetUp() override {
    RenderViewHostTestHarness::SetUp();
    CHECK(main_rfh());
    url::Origin rp_origin = url::Origin::Create(GURL("https://rp.example"));
    manager_ = std::make_unique<EmailVerifierNetworkRequestManager>(
        rp_origin,
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_),
        network::mojom::ClientSecurityState::New(),
        main_rfh()->GetFrameTreeNodeId(), main_rfh()->GetWeakDocumentPtr());
  }

  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<EmailVerifierNetworkRequestManager> manager_;
};

TEST_F(EmailVerifierNetworkRequestManagerTest,
       FetchWellKnownRequestDestination) {
  base::RunLoop run_loop;
  bool called = false;
  auto interceptor =
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        called = true;
        EXPECT_EQ(network::mojom::RequestDestination::kEmailVerification,
                  request.destination);
        run_loop.Quit();
      });
  test_url_loader_factory_.SetInterceptor(interceptor);
  manager_->FetchWellKnown(GURL("https://idp.example"), base::DoNothing());
  run_loop.Run();
  EXPECT_TRUE(called);
}

TEST_F(EmailVerifierNetworkRequestManagerTest, SendTokenRequestDestination) {
  base::RunLoop run_loop;
  bool called = false;
  auto interceptor =
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        called = true;
        EXPECT_EQ(network::mojom::RequestDestination::kEmailVerification,
                  request.destination);
        std::optional<std::string> content_type =
            request.headers.GetHeader(net::HttpRequestHeaders::kContentType);
        ASSERT_TRUE(content_type.has_value());
        EXPECT_EQ("application/json", *content_type);
        run_loop.Quit();
      });
  test_url_loader_factory_.SetInterceptor(interceptor);
  manager_->SendTokenRequest(GURL("https://idp.example/token"), "data",
                             net::HttpRequestHeaders(), base::DoNothing());
  run_loop.Run();
  EXPECT_TRUE(called);
}

// If the frame is invalidated during the request initiation, the request is
// aborted.
TEST_F(EmailVerifierNetworkRequestManagerTest, RequestAbortedOnInvalidFrame) {
  url::Origin rp_origin = url::Origin::Create(GURL("https://rp.example"));
  network::TestURLLoaderFactory test_url_loader_factory;

  // Simulate the frame being invalidated by passing a `WeakDocumentPtr()` to
  // `EmailVerifierNetworkRequestManager`'s constructor.
  auto manager = std::make_unique<EmailVerifierNetworkRequestManager>(
      rp_origin,
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &test_url_loader_factory),
      network::mojom::ClientSecurityState::New(), FrameTreeNodeId(),
      WeakDocumentPtr());

  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting(
      [&](FetchStatus fetch_status,
          EmailVerifierNetworkRequestManager::WellKnown well_known) {
        // The request should be aborted.
        EXPECT_EQ(ParseStatus::kNoResponseError, fetch_status.parse_status);
        EXPECT_EQ(net::ERR_ABORTED, fetch_status.response_code);
        run_loop.Quit();
      });

  manager->FetchWellKnown(GURL("https://idp.example"), std::move(callback));
  run_loop.Run();

  EXPECT_EQ(0, test_url_loader_factory.NumPending());
}

// EVP-COMPLIANCE: EVP-3.3-01
// The metadata document's `issuer` must be byte-for-byte the issuer
// identifier it was fetched under. A missing `issuer` is still accepted for
// now (see the TODO in OnWellKnownParsed).
TEST_F(EmailVerifierNetworkRequestManagerTest, FetchWellKnownChecksIssuer) {
  constexpr char kWellKnownUrl[] =
      "https://idp.example/.well-known/email-verification";
  struct {
    const char* issuer_member;  // Raw JSON, or nullptr to omit `issuer`.
    ParseStatus expected;
  } kCases[] = {
      {R"("https://idp.example")", ParseStatus::kSuccess},
      {nullptr, ParseStatus::kSuccess},
      {R"("https://evil.example")", ParseStatus::kInvalidResponseError},
      // No normalization is applied: these all name the same origin, but none
      // is the identifier byte-for-byte.
      {R"("https://idp.example/")", ParseStatus::kInvalidResponseError},
      {R"("https://idp.example:443")", ParseStatus::kInvalidResponseError},
      {R"("idp.example")", ParseStatus::kInvalidResponseError},
      {R"("")", ParseStatus::kInvalidResponseError},
      {"1", ParseStatus::kInvalidResponseError},
  };

  for (const auto& test : kCases) {
    SCOPED_TRACE(test.issuer_member ? test.issuer_member : "(absent)");
    std::string body = R"({"issuance_endpoint": "https://idp.example/token")";
    if (test.issuer_member) {
      body += base::StrCat({R"(, "issuer": )", test.issuer_member});
    }
    body += "}";

    auto head = network::CreateURLResponseHead(net::HTTP_OK);
    head->headers->SetHeader("Content-Type", "application/json");
    test_url_loader_factory_.AddResponse(
        GURL(kWellKnownUrl), std::move(head), body,
        network::URLLoaderCompletionStatus(net::OK));

    base::test::TestFuture<FetchStatus,
                           EmailVerifierNetworkRequestManager::WellKnown>
        future;
    manager_->FetchWellKnown(GURL("https://idp.example"), future.GetCallback());
    EXPECT_EQ(test.expected, future.Get<0>().parse_status);
  }
}

}  // namespace content::webid
