// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/delegation/email_verifier_impl.h"

#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/test_future.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/webid/delegation/email_verification_request.h"
#include "content/public/browser/webid/email_verifier.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_renderer_host.h"
#include "net/base/schemeful_site.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using ::testing::_;
using ::testing::ByMove;
using ::testing::NiceMock;
using ::testing::Optional;
using ::testing::Return;
using ::testing::StrictMock;
using ::testing::WithArgs;

namespace content::webid {

class MockEmailVerificationRequest : public EmailVerificationRequest {
 public:
  explicit MockEmailVerificationRequest(RenderFrameHost& rfh)
      : EmailVerificationRequest(nullptr,
                                 nullptr,
                                 nullptr,
                                 static_cast<RenderFrameHostImpl&>(rfh)) {}
  ~MockEmailVerificationRequest() override { Destroy(); }

  MOCK_METHOD(void, Destroy, (), ());
  MOCK_METHOD(void,
              Send,
              (const std::string&,
               const std::string&,
               EmailVerifier::OnEmailVerifiedCallback));
  MOCK_METHOD(void,
              CheckIfVerifiable,
              (const std::string&, EmailVerifier::IsVerifiableCallback));
  MOCK_METHOD(void,
              Verify,
              (const EmailVerifier::Result&,
               const std::string&,
               EmailVerifier::OnEmailVerifiedCallback),
              (override));
  void AddObserver(Observer* observer) override {
    captured_observer_ = observer;
    EmailVerificationRequest::AddObserver(observer);
  }

  Observer* captured_observer() { return captured_observer_; }

 private:
  raw_ptr<Observer> captured_observer_ = nullptr;
};

class MockRequestBuilder {
 public:
  MOCK_METHOD(std::unique_ptr<EmailVerificationRequest>, Run, ());
};

class EmailVerifierImplTest : public RenderViewHostTestHarness {
 public:
  EmailVerifierImplTest() = default;
  ~EmailVerifierImplTest() override = default;
};

TEST_F(EmailVerifierImplTest, TestSingleRequest) {
  auto request_iv =
      std::make_unique<StrictMock<MockEmailVerificationRequest>>(*main_rfh());
  auto* request_ptr_iv = request_iv.get();
  EXPECT_CALL(*request_ptr_iv, Destroy());

  auto request_v =
      std::make_unique<StrictMock<MockEmailVerificationRequest>>(*main_rfh());
  auto* request_ptr_v = request_v.get();
  EXPECT_CALL(*request_ptr_v, Destroy());

  MockRequestBuilder builder;
  EXPECT_CALL(builder, Run)
      .WillOnce(Return(ByMove(std::move(request_iv))))
      .WillOnce(Return(ByMove(std::move(request_v))));

  EmailVerifierImpl verifier(base::BindRepeating(&MockRequestBuilder::Run,
                                                 base::Unretained(&builder)));

  EmailVerifier::Result issuer;
  issuer.email = "test@example.com";
  issuer.issuer_site = net::SchemefulSite(GURL("https://example.com"));

  base::test::TestFuture<std::optional<EmailVerifier::Result>> verifiable_cb;

  EXPECT_CALL(*request_ptr_iv, CheckIfVerifiable("test@example.com", _))
      .WillOnce(WithArgs<1>([&](EmailVerifier::IsVerifiableCallback callback) {
        std::move(callback).Run(issuer);
      }));

  verifier.CheckIfVerifiable("test@example.com", verifiable_cb.GetCallback());

  EXPECT_EQ(verifiable_cb.Get(), issuer);

  EXPECT_CALL(*request_ptr_v, Verify(_, "nonce", _))
      .WillOnce(
          WithArgs<2>([&](EmailVerifier::OnEmailVerifiedCallback callback) {
            std::move(callback).Run("token");
          }));

  base::MockCallback<EmailVerifier::OnEmailVerifiedCallback> cb;
  EXPECT_CALL(cb, Run(Optional(std::string("token"))));
  verifier.Verify(issuer, "nonce", cb.Get());
}

TEST_F(EmailVerifierImplTest, TestStatefulFlow) {
  auto request_iv =
      std::make_unique<StrictMock<MockEmailVerificationRequest>>(*main_rfh());
  auto* request_ptr_iv = request_iv.get();
  EXPECT_CALL(*request_ptr_iv, Destroy());

  auto request_v =
      std::make_unique<StrictMock<MockEmailVerificationRequest>>(*main_rfh());
  auto* request_ptr_v = request_v.get();
  EXPECT_CALL(*request_ptr_v, Destroy());

  MockRequestBuilder builder;
  EXPECT_CALL(builder, Run)
      .WillOnce(Return(ByMove(std::move(request_iv))))
      .WillOnce(Return(ByMove(std::move(request_v))));

  EmailVerifierImpl verifier(base::BindRepeating(&MockRequestBuilder::Run,
                                                 base::Unretained(&builder)));

  const std::string kEmail = "test@example.com";
  const GURL kIssuanceEndpoint = GURL("https://issuer.example.com/token");

  EmailVerifier::Result issuer;
  issuer.email = kEmail;
  issuer.issuer_site = net::SchemefulSite(GURL("https://example.com"));
  issuer.issuance_endpoint = kIssuanceEndpoint;

  EXPECT_CALL(*request_ptr_iv, CheckIfVerifiable(kEmail, _))
      .WillOnce(WithArgs<1>([&](EmailVerifier::IsVerifiableCallback callback) {
        std::move(callback).Run(issuer);
      }));

  base::test::TestFuture<std::optional<EmailVerifier::Result>> verifiable_cb;
  verifier.CheckIfVerifiable(kEmail, verifiable_cb.GetCallback());

  EXPECT_EQ(verifiable_cb.Get(), issuer);

  EXPECT_CALL(*request_ptr_v, Verify(_, "nonce", _))
      .WillOnce(
          WithArgs<2>([&](EmailVerifier::OnEmailVerifiedCallback callback) {
            std::move(callback).Run("token");
          }));

  base::MockCallback<EmailVerifier::OnEmailVerifiedCallback> verified_cb;
  EXPECT_CALL(verified_cb, Run(Optional(std::string("token"))));
  verifier.Verify(issuer, "nonce", verified_cb.Get());
}

TEST_F(EmailVerifierImplTest, TestTwoConcurrentRequests) {
  auto request_iv1 =
      std::make_unique<StrictMock<MockEmailVerificationRequest>>(*main_rfh());
  auto* request_ptr_iv1 = request_iv1.get();
  EXPECT_CALL(*request_ptr_iv1, Destroy());

  auto request_iv2 =
      std::make_unique<StrictMock<MockEmailVerificationRequest>>(*main_rfh());
  auto* request_ptr_iv2 = request_iv2.get();
  EXPECT_CALL(*request_ptr_iv2, Destroy());

  auto request_v1 =
      std::make_unique<StrictMock<MockEmailVerificationRequest>>(*main_rfh());
  auto* request_ptr_v1 = request_v1.get();
  EXPECT_CALL(*request_ptr_v1, Destroy());

  auto request_v2 =
      std::make_unique<StrictMock<MockEmailVerificationRequest>>(*main_rfh());
  auto* request_ptr_v2 = request_v2.get();
  EXPECT_CALL(*request_ptr_v2, Destroy());

  MockRequestBuilder builder;
  EXPECT_CALL(builder, Run)
      .WillOnce(Return(ByMove(std::move(request_iv1))))
      .WillOnce(Return(ByMove(std::move(request_iv2))))
      .WillOnce(Return(ByMove(std::move(request_v1))))
      .WillOnce(Return(ByMove(std::move(request_v2))));

  EmailVerifierImpl verifier(base::BindRepeating(&MockRequestBuilder::Run,
                                                 base::Unretained(&builder)));

  EmailVerifier::Result issuer1;
  issuer1.email = "test1@example.com";
  issuer1.issuer_site = net::SchemefulSite(GURL("https://example.com"));

  EmailVerifier::Result issuer2;
  issuer2.email = "test2@example.com";
  issuer2.issuer_site = net::SchemefulSite(GURL("https://example.com"));

  // Set up expectations and capture callbacks for the two requests.
  EXPECT_CALL(*request_ptr_iv1, CheckIfVerifiable("test1@example.com", _))
      .WillOnce(WithArgs<1>([&](EmailVerifier::IsVerifiableCallback callback) {
        std::move(callback).Run(issuer1);
      }));

  EXPECT_CALL(*request_ptr_iv2, CheckIfVerifiable("test2@example.com", _))
      .WillOnce(WithArgs<1>([&](EmailVerifier::IsVerifiableCallback callback) {
        std::move(callback).Run(issuer2);
      }));

  base::test::TestFuture<std::optional<EmailVerifier::Result>> verifiable_cb1;
  verifier.CheckIfVerifiable("test1@example.com", verifiable_cb1.GetCallback());

  base::test::TestFuture<std::optional<EmailVerifier::Result>> verifiable_cb2;
  verifier.CheckIfVerifiable("test2@example.com", verifiable_cb2.GetCallback());

  EXPECT_EQ(verifiable_cb1.Get(), issuer1);
  EXPECT_EQ(verifiable_cb2.Get(), issuer2);

  // Now we need to expect the Verify calls!
  EmailVerifier::OnEmailVerifiedCallback callback1;
  EXPECT_CALL(*request_ptr_v1, Verify(_, "nonce1", _))
      .WillOnce(
          WithArgs<2>([&](EmailVerifier::OnEmailVerifiedCallback callback) {
            callback1 = std::move(callback);
          }));

  EmailVerifier::OnEmailVerifiedCallback callback2;
  EXPECT_CALL(*request_ptr_v2, Verify(_, "nonce2", _))
      .WillOnce(
          WithArgs<2>([&](EmailVerifier::OnEmailVerifiedCallback callback) {
            callback2 = std::move(callback);
          }));

  base::MockCallback<EmailVerifier::OnEmailVerifiedCallback> cb1;
  EXPECT_CALL(cb1, Run(Optional(std::string("token1"))));

  base::MockCallback<EmailVerifier::OnEmailVerifiedCallback> cb2;
  EXPECT_CALL(cb2, Run(Optional(std::string("token2"))));

  // Make the concurrent calls to Verify.
  verifier.Verify(issuer1, "nonce1", cb1.Get());
  verifier.Verify(issuer2, "nonce2", cb2.Get());

  ASSERT_TRUE(callback1);
  ASSERT_TRUE(callback2);

  // Complete in reverse order to test concurrency.
  std::move(callback2).Run("token2");
  std::move(callback1).Run("token1");
}

TEST_F(EmailVerifierImplTest, TimingHistograms) {
  base::HistogramTester histogram_tester;

  std::unique_ptr<StrictMock<MockEmailVerificationRequest>> request_iv =
      std::make_unique<StrictMock<MockEmailVerificationRequest>>(*main_rfh());
  StrictMock<MockEmailVerificationRequest>* request_ptr_iv = request_iv.get();
  EXPECT_CALL(*request_ptr_iv, Destroy());

  std::unique_ptr<StrictMock<MockEmailVerificationRequest>> request_v =
      std::make_unique<StrictMock<MockEmailVerificationRequest>>(*main_rfh());
  StrictMock<MockEmailVerificationRequest>* request_ptr_v = request_v.get();
  EXPECT_CALL(*request_ptr_v, Destroy());

  MockRequestBuilder builder;
  EXPECT_CALL(builder, Run)
      .WillOnce(Return(ByMove(std::move(request_iv))))
      .WillOnce(Return(ByMove(std::move(request_v))));

  EmailVerifierImpl verifier(base::BindRepeating(&MockRequestBuilder::Run,
                                                 base::Unretained(&builder)));

  const std::string kEmail = "test@example.com";
  const GURL kIssuanceEndpoint = GURL("https://issuer.example.com/token");

  EmailVerifier::Result issuer;
  issuer.email = kEmail;
  issuer.issuer_site = net::SchemefulSite(GURL("https://example.com"));
  issuer.issuance_endpoint = kIssuanceEndpoint;

  EXPECT_CALL(*request_ptr_iv, CheckIfVerifiable(kEmail, _))
      .WillOnce(WithArgs<1>([&](EmailVerifier::IsVerifiableCallback callback) {
        request_ptr_iv->captured_observer()->OnIsVerifiableStart(
            request_ptr_iv);
        request_ptr_iv->captured_observer()->OnIsVerifiableComplete(
            request_ptr_iv,
            blink::mojom::EmailVerificationRequestResult::kSuccess);
        std::move(callback).Run(issuer);
      }));

  base::test::TestFuture<std::optional<EmailVerifier::Result>> verifiable_cb;
  verifier.CheckIfVerifiable(kEmail, verifiable_cb.GetCallback());
  EXPECT_EQ(verifiable_cb.Get(), issuer);

  EXPECT_CALL(*request_ptr_v, Verify(_, "nonce", _))
      .WillOnce(
          WithArgs<2>([&](EmailVerifier::OnEmailVerifiedCallback callback) {
            request_ptr_v->captured_observer()->OnVerifyStart(request_ptr_v);
            request_ptr_v->captured_observer()->OnVerifyComplete(
                request_ptr_v,
                blink::mojom::EmailVerificationRequestResult::kSuccess);
            std::move(callback).Run("token");
          }));

  base::MockCallback<EmailVerifier::OnEmailVerifiedCallback> verified_cb;
  EXPECT_CALL(verified_cb, Run(Optional(std::string("token"))));
  verifier.Verify(issuer, "nonce", verified_cb.Get());

  histogram_tester.ExpectTotalCount("Blink.Evp.Timing.IsVerifiable", 1);
  histogram_tester.ExpectTotalCount("Blink.Evp.Timing.Verify", 1);
}

}  // namespace content::webid
