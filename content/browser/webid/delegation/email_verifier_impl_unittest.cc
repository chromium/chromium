// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/delegation/email_verifier_impl.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/webid/delegation/email_verification_request.h"
#include "content/public/browser/webid/email_verifier.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/webid/email_verification_request.mojom-shared.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content::webid {

using ::testing::_;
using ::testing::ByMove;
using ::testing::NiceMock;
using ::testing::Optional;
using ::testing::Return;
using ::testing::StrictMock;
using ::testing::WithArgs;

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
              (const std::string&,
               base::OnceClosure,
               EmailVerifier::IsVerifiableCallback));
  MOCK_METHOD(void,
              Verify,
              (const EmailVerifier::Result&,
               const std::string&,
               EmailVerifier::OnEmailVerifiedCallback),
              (override));
};

class MockRequestBuilder {
 public:
  MOCK_METHOD(std::unique_ptr<EmailVerificationRequest>, Run, ());
};

class EmailVerifierImplTest : public RenderViewHostTestHarness {
 public:
  EmailVerifierImplTest() = default;
  ~EmailVerifierImplTest() override = default;

 protected:
  static constexpr char kEmail[] = "test@example.com";
  static constexpr char kNonce[] = "nonce";
  static constexpr char kToken[] = "token";

  std::unique_ptr<StrictMock<MockEmailVerificationRequest>>
  CreateTrackedRequest(bool& destroyed) {
    auto request =
        std::make_unique<StrictMock<MockEmailVerificationRequest>>(*main_rfh());
    EXPECT_CALL(*request, Destroy()).WillOnce([&destroyed]() {
      destroyed = true;
    });
    return request;
  }
};

TEST_F(EmailVerifierImplTest, TestSingleRequest) {
  auto request_is_verifiable =
      std::make_unique<StrictMock<MockEmailVerificationRequest>>(*main_rfh());
  auto* request_ptr_is_verifiable = request_is_verifiable.get();
  EXPECT_CALL(*request_ptr_is_verifiable, Destroy());

  auto request_verify =
      std::make_unique<StrictMock<MockEmailVerificationRequest>>(*main_rfh());
  auto* request_ptr_verify = request_verify.get();
  EXPECT_CALL(*request_ptr_verify, Destroy());

  MockRequestBuilder builder;
  EXPECT_CALL(builder, Run)
      .WillOnce(Return(ByMove(std::move(request_is_verifiable))))
      .WillOnce(Return(ByMove(std::move(request_verify))));

  EmailVerifierImpl verifier(base::BindRepeating(&MockRequestBuilder::Run,
                                                 base::Unretained(&builder)));

  EmailVerifier::Result issuer;
  issuer.email = "test@example.com";
  issuer.issuer_origin = url::Origin::Create(GURL("https://example.com"));

  base::test::TestFuture<std::optional<EmailVerifier::Result>,
                         blink::mojom::EmailVerificationRequestResult,
                         base::TimeDelta>
      verifiable_cb;

  EXPECT_CALL(*request_ptr_is_verifiable,
              CheckIfVerifiable("test@example.com", _, _))
      .WillOnce(WithArgs<2>([&](EmailVerifier::IsVerifiableCallback callback) {
        std::move(callback).Run(
            issuer, blink::mojom::EmailVerificationRequestResult::kSuccess,
            base::Milliseconds(100));
      }));

  verifier.CheckIfVerifiable("test@example.com", base::DoNothing(),
                             verifiable_cb.GetCallback());

  EXPECT_EQ(verifiable_cb.Get<0>(), issuer);

  EXPECT_CALL(*request_ptr_verify, Verify(_, "nonce", _))
      .WillOnce(
          WithArgs<2>([&](EmailVerifier::OnEmailVerifiedCallback callback) {
            std::move(callback).Run(
                "token", blink::mojom::EmailVerificationRequestResult::kSuccess,
                base::Milliseconds(200));
          }));

  base::MockCallback<EmailVerifier::OnEmailVerifiedCallback> cb;
  EXPECT_CALL(cb,
              Run(Optional(std::string("token")),
                  blink::mojom::EmailVerificationRequestResult::kSuccess, _));
  verifier.Verify(issuer, "nonce", cb.Get());
}

TEST_F(EmailVerifierImplTest, TestStatefulFlow) {
  auto request_is_verifiable =
      std::make_unique<StrictMock<MockEmailVerificationRequest>>(*main_rfh());
  auto* request_ptr_is_verifiable = request_is_verifiable.get();
  EXPECT_CALL(*request_ptr_is_verifiable, Destroy());

  auto request_verify =
      std::make_unique<StrictMock<MockEmailVerificationRequest>>(*main_rfh());
  auto* request_ptr_verify = request_verify.get();
  EXPECT_CALL(*request_ptr_verify, Destroy());

  MockRequestBuilder builder;
  EXPECT_CALL(builder, Run)
      .WillOnce(Return(ByMove(std::move(request_is_verifiable))))
      .WillOnce(Return(ByMove(std::move(request_verify))));

  EmailVerifierImpl verifier(base::BindRepeating(&MockRequestBuilder::Run,
                                                 base::Unretained(&builder)));

  const std::string kEmail = "test@example.com";
  const GURL kIssuanceEndpoint = GURL("https://issuer.example.com/token");

  EmailVerifier::Result issuer;
  issuer.email = kEmail;
  issuer.issuer_origin = url::Origin::Create(GURL("https://example.com"));
  issuer.issuance_endpoint = kIssuanceEndpoint;

  EXPECT_CALL(*request_ptr_is_verifiable, CheckIfVerifiable(kEmail, _, _))
      .WillOnce(WithArgs<2>([&](EmailVerifier::IsVerifiableCallback callback) {
        std::move(callback).Run(
            issuer, blink::mojom::EmailVerificationRequestResult::kSuccess,
            base::Milliseconds(100));
      }));

  base::test::TestFuture<std::optional<EmailVerifier::Result>,
                         blink::mojom::EmailVerificationRequestResult,
                         base::TimeDelta>
      verifiable_cb;
  verifier.CheckIfVerifiable(kEmail, base::DoNothing(),
                             verifiable_cb.GetCallback());

  EXPECT_EQ(verifiable_cb.Get<0>(), issuer);

  EXPECT_CALL(*request_ptr_verify, Verify(_, "nonce", _))
      .WillOnce(
          WithArgs<2>([&](EmailVerifier::OnEmailVerifiedCallback callback) {
            std::move(callback).Run(
                "token", blink::mojom::EmailVerificationRequestResult::kSuccess,
                base::Milliseconds(200));
          }));

  base::MockCallback<EmailVerifier::OnEmailVerifiedCallback> verified_cb;
  EXPECT_CALL(verified_cb,
              Run(Optional(std::string("token")),
                  blink::mojom::EmailVerificationRequestResult::kSuccess, _));
  verifier.Verify(issuer, "nonce", verified_cb.Get());
}

TEST_F(EmailVerifierImplTest, TestTwoConcurrentRequests) {
  auto request_is_verifiable1 =
      std::make_unique<StrictMock<MockEmailVerificationRequest>>(*main_rfh());
  auto* request_ptr_is_verifiable1 = request_is_verifiable1.get();
  EXPECT_CALL(*request_ptr_is_verifiable1, Destroy());

  auto request_is_verifiable2 =
      std::make_unique<StrictMock<MockEmailVerificationRequest>>(*main_rfh());
  auto* request_ptr_is_verifiable2 = request_is_verifiable2.get();
  EXPECT_CALL(*request_ptr_is_verifiable2, Destroy());

  auto request_verify1 =
      std::make_unique<StrictMock<MockEmailVerificationRequest>>(*main_rfh());
  auto* request_ptr_verify1 = request_verify1.get();
  EXPECT_CALL(*request_ptr_verify1, Destroy());

  auto request_verify2 =
      std::make_unique<StrictMock<MockEmailVerificationRequest>>(*main_rfh());
  auto* request_ptr_verify2 = request_verify2.get();
  EXPECT_CALL(*request_ptr_verify2, Destroy());

  MockRequestBuilder builder;
  EXPECT_CALL(builder, Run)
      .WillOnce(Return(ByMove(std::move(request_is_verifiable1))))
      .WillOnce(Return(ByMove(std::move(request_is_verifiable2))))
      .WillOnce(Return(ByMove(std::move(request_verify1))))
      .WillOnce(Return(ByMove(std::move(request_verify2))));

  EmailVerifierImpl verifier(base::BindRepeating(&MockRequestBuilder::Run,
                                                 base::Unretained(&builder)));

  EmailVerifier::Result issuer1;
  issuer1.email = "test1@example.com";
  issuer1.issuer_origin = url::Origin::Create(GURL("https://example.com"));

  EmailVerifier::Result issuer2;
  issuer2.email = "test2@example.com";
  issuer2.issuer_origin = url::Origin::Create(GURL("https://example.com"));

  // Set up expectations and capture callbacks for the two requests.
  EXPECT_CALL(*request_ptr_is_verifiable1,
              CheckIfVerifiable("test1@example.com", _, _))
      .WillOnce(WithArgs<2>([&](EmailVerifier::IsVerifiableCallback callback) {
        std::move(callback).Run(
            issuer1, blink::mojom::EmailVerificationRequestResult::kSuccess,
            base::Milliseconds(100));
      }));

  EXPECT_CALL(*request_ptr_is_verifiable2,
              CheckIfVerifiable("test2@example.com", _, _))
      .WillOnce(WithArgs<2>([&](EmailVerifier::IsVerifiableCallback callback) {
        std::move(callback).Run(
            issuer2, blink::mojom::EmailVerificationRequestResult::kSuccess,
            base::Milliseconds(100));
      }));

  base::test::TestFuture<std::optional<EmailVerifier::Result>,
                         blink::mojom::EmailVerificationRequestResult,
                         base::TimeDelta>
      verifiable_cb1;
  verifier.CheckIfVerifiable("test1@example.com", base::DoNothing(),
                             verifiable_cb1.GetCallback());

  base::test::TestFuture<std::optional<EmailVerifier::Result>,
                         blink::mojom::EmailVerificationRequestResult,
                         base::TimeDelta>
      verifiable_cb2;
  verifier.CheckIfVerifiable("test2@example.com", base::DoNothing(),
                             verifiable_cb2.GetCallback());

  EXPECT_EQ(verifiable_cb1.Get<0>(), issuer1);
  EXPECT_EQ(verifiable_cb2.Get<0>(), issuer2);

  // Now we need to expect the Verify calls!
  EmailVerifier::OnEmailVerifiedCallback callback1;
  EXPECT_CALL(*request_ptr_verify1, Verify(_, "nonce1", _))
      .WillOnce(
          WithArgs<2>([&](EmailVerifier::OnEmailVerifiedCallback callback) {
            callback1 = std::move(callback);
          }));

  EmailVerifier::OnEmailVerifiedCallback callback2;
  EXPECT_CALL(*request_ptr_verify2, Verify(_, "nonce2", _))
      .WillOnce(
          WithArgs<2>([&](EmailVerifier::OnEmailVerifiedCallback callback) {
            callback2 = std::move(callback);
          }));

  base::MockCallback<EmailVerifier::OnEmailVerifiedCallback> cb1;
  EXPECT_CALL(cb1,
              Run(Optional(std::string("token1")),
                  blink::mojom::EmailVerificationRequestResult::kSuccess, _));

  base::MockCallback<EmailVerifier::OnEmailVerifiedCallback> cb2;
  EXPECT_CALL(cb2,
              Run(Optional(std::string("token2")),
                  blink::mojom::EmailVerificationRequestResult::kSuccess, _));

  // Make the concurrent calls to Verify.
  verifier.Verify(issuer1, "nonce1", cb1.Get());
  verifier.Verify(issuer2, "nonce2", cb2.Get());

  ASSERT_TRUE(callback1);
  ASSERT_TRUE(callback2);

  // Complete in reverse order to test concurrency.
  std::move(callback2).Run(
      "token2", blink::mojom::EmailVerificationRequestResult::kSuccess,
      base::Milliseconds(200));
  std::move(callback1).Run(
      "token1", blink::mojom::EmailVerificationRequestResult::kSuccess,
      base::Milliseconds(200));
}

TEST_F(EmailVerifierImplTest, ForwardsDurationAndStatus) {
  std::unique_ptr<StrictMock<MockEmailVerificationRequest>>
      request_is_verifiable =
          std::make_unique<StrictMock<MockEmailVerificationRequest>>(
              *main_rfh());
  StrictMock<MockEmailVerificationRequest>* request_ptr_is_verifiable =
      request_is_verifiable.get();
  EXPECT_CALL(*request_ptr_is_verifiable, Destroy());

  std::unique_ptr<StrictMock<MockEmailVerificationRequest>> request_verify =
      std::make_unique<StrictMock<MockEmailVerificationRequest>>(*main_rfh());
  StrictMock<MockEmailVerificationRequest>* request_ptr_verify =
      request_verify.get();
  EXPECT_CALL(*request_ptr_verify, Destroy());

  MockRequestBuilder builder;
  EXPECT_CALL(builder, Run)
      .WillOnce(Return(ByMove(std::move(request_is_verifiable))))
      .WillOnce(Return(ByMove(std::move(request_verify))));

  EmailVerifierImpl verifier(base::BindRepeating(&MockRequestBuilder::Run,
                                                 base::Unretained(&builder)));

  const std::string kEmail = "test@example.com";
  const GURL kIssuanceEndpoint = GURL("https://issuer.example.com/token");

  EmailVerifier::Result issuer;
  issuer.email = kEmail;
  issuer.issuer_origin = url::Origin::Create(GURL("https://example.com"));
  issuer.issuance_endpoint = kIssuanceEndpoint;

  EXPECT_CALL(*request_ptr_is_verifiable, CheckIfVerifiable(kEmail, _, _))
      .WillOnce(WithArgs<2>([&](EmailVerifier::IsVerifiableCallback callback) {
        std::move(callback).Run(
            issuer, blink::mojom::EmailVerificationRequestResult::kSuccess,
            base::Milliseconds(100));
      }));

  base::test::TestFuture<std::optional<EmailVerifier::Result>,
                         blink::mojom::EmailVerificationRequestResult,
                         base::TimeDelta>
      verifiable_cb;
  verifier.CheckIfVerifiable(kEmail, base::DoNothing(),
                             verifiable_cb.GetCallback());
  EXPECT_EQ(verifiable_cb.Get<0>(), issuer);
  EXPECT_EQ(verifiable_cb.Get<1>(),
            blink::mojom::EmailVerificationRequestResult::kSuccess);
  EXPECT_EQ(verifiable_cb.Get<2>(), base::Milliseconds(100));

  EXPECT_CALL(*request_ptr_verify, Verify(_, "nonce", _))
      .WillOnce(
          WithArgs<2>([&](EmailVerifier::OnEmailVerifiedCallback callback) {
            std::move(callback).Run(
                "token", blink::mojom::EmailVerificationRequestResult::kSuccess,
                base::Milliseconds(200));
          }));

  base::MockCallback<EmailVerifier::OnEmailVerifiedCallback> verified_cb;
  EXPECT_CALL(verified_cb,
              Run(Optional(std::string("token")),
                  blink::mojom::EmailVerificationRequestResult::kSuccess,
                  base::Milliseconds(200)));
  verifier.Verify(issuer, "nonce", verified_cb.Get());
}

// Regression test for https://crbug.com/533251262 and
// https://crbug.com/534621885. Verifies that destroying `EmailVerifierImpl`
// synchronously destroys in-flight `CheckIfVerifiable()` and `Verify()`
// requests and drops their callbacks.
TEST_F(EmailVerifierImplTest, DestroyingVerifierCancelsPendingRequests) {
  bool is_verifiable_destroyed = false;
  auto is_verifiable_request = CreateTrackedRequest(is_verifiable_destroyed);
  StrictMock<MockEmailVerificationRequest>* is_verifiable_request_ptr =
      is_verifiable_request.get();

  bool verify_destroyed = false;
  auto verify_request = CreateTrackedRequest(verify_destroyed);
  StrictMock<MockEmailVerificationRequest>* verify_request_ptr =
      verify_request.get();

  MockRequestBuilder builder;
  EXPECT_CALL(builder, Run)
      .WillOnce(Return(ByMove(std::move(is_verifiable_request))))
      .WillOnce(Return(ByMove(std::move(verify_request))));

  std::optional<EmailVerifierImpl> verifier;
  verifier.emplace(base::BindRepeating(&MockRequestBuilder::Run,
                                       base::Unretained(&builder)));

  base::OnceClosure captured_dns_resolved_cb;
  EmailVerifier::IsVerifiableCallback captured_is_verifiable_cb;
  EXPECT_CALL(*is_verifiable_request_ptr, CheckIfVerifiable(kEmail, _, _))
      .WillOnce(WithArgs<1, 2>(
          [&captured_dns_resolved_cb, &captured_is_verifiable_cb](
              base::OnceClosure dns_cb,
              EmailVerifier::IsVerifiableCallback cb) {
            captured_dns_resolved_cb = std::move(dns_cb);
            captured_is_verifiable_cb = std::move(cb);
          }));

  StrictMock<base::MockCallback<base::OnceClosure>> dns_resolved_cb;
  EXPECT_CALL(dns_resolved_cb, Run).Times(0);
  StrictMock<base::MockCallback<EmailVerifier::IsVerifiableCallback>>
      verifiable_cb;
  EXPECT_CALL(verifiable_cb, Run).Times(0);
  verifier->CheckIfVerifiable(kEmail, dns_resolved_cb.Get(),
                              verifiable_cb.Get());

  EmailVerifier::Result issuer;
  issuer.email = kEmail;
  issuer.issuer_origin = url::Origin::Create(GURL("https://example.com"));

  EmailVerifier::OnEmailVerifiedCallback captured_verify_cb;
  EXPECT_CALL(*verify_request_ptr, Verify(_, kNonce, _))
      .WillOnce(WithArgs<2>(
          [&captured_verify_cb](EmailVerifier::OnEmailVerifiedCallback cb) {
            captured_verify_cb = std::move(cb);
          }));

  StrictMock<base::MockCallback<EmailVerifier::OnEmailVerifiedCallback>>
      verified_cb;
  EXPECT_CALL(verified_cb, Run).Times(0);
  verifier->Verify(issuer, kNonce, verified_cb.Get());

  ASSERT_TRUE(captured_dns_resolved_cb);
  ASSERT_TRUE(captured_is_verifiable_cb);
  ASSERT_TRUE(captured_verify_cb);
  EXPECT_FALSE(is_verifiable_destroyed);
  EXPECT_FALSE(verify_destroyed);

  // Destroy `EmailVerifierImpl` while both requests are pending.
  verifier.reset();
  EXPECT_TRUE(is_verifiable_destroyed);
  EXPECT_TRUE(verify_destroyed);

  // Running the captured callbacks after `EmailVerifierImpl` was destroyed is a
  // safe no-op and does not invoke the caller callbacks.
  std::move(captured_dns_resolved_cb).Run();
  std::move(captured_is_verifiable_cb)
      .Run(std::nullopt,
           blink::mojom::EmailVerificationRequestResult::kUserLoggedOut,
           base::Milliseconds(100));
  std::move(captured_verify_cb)
      .Run(kToken, blink::mojom::EmailVerificationRequestResult::kSuccess,
           base::Milliseconds(200));
}

// Verifies that completed requests are destroyed before invoking caller
// callbacks rather than kept until `EmailVerifierImpl` is destroyed, and that
// destroying `EmailVerifierImpl` from inside any of the three callbacks
// (`on_dns_resolved_callback`, `IsVerifiableCallback`, and
// `OnEmailVerifiedCallback`) is safe (exercised preventatively in unit tests
// today).
TEST_F(EmailVerifierImplTest, CompletedRequestsAndReentrantDestruction) {
  bool dns_request_destroyed = false;
  auto dns_request = CreateTrackedRequest(dns_request_destroyed);
  StrictMock<MockEmailVerificationRequest>* dns_request_ptr = dns_request.get();

  bool is_verifiable_destroyed = false;
  auto is_verifiable_request = CreateTrackedRequest(is_verifiable_destroyed);
  StrictMock<MockEmailVerificationRequest>* is_verifiable_request_ptr =
      is_verifiable_request.get();

  bool verify_destroyed = false;
  auto verify_request = CreateTrackedRequest(verify_destroyed);
  StrictMock<MockEmailVerificationRequest>* verify_request_ptr =
      verify_request.get();

  MockRequestBuilder builder;
  EXPECT_CALL(builder, Run)
      .WillOnce(Return(ByMove(std::move(dns_request))))
      .WillOnce(Return(ByMove(std::move(is_verifiable_request))))
      .WillOnce(Return(ByMove(std::move(verify_request))));

  std::optional<EmailVerifierImpl> verifier;
  verifier.emplace(base::BindRepeating(&MockRequestBuilder::Run,
                                       base::Unretained(&builder)));

  // 1. Destroying `EmailVerifierImpl` inside `on_dns_resolved_callback` cancels
  // the in-flight request safely.
  EmailVerifier::IsVerifiableCallback captured_dns_is_verifiable_cb;
  EXPECT_CALL(*dns_request_ptr, CheckIfVerifiable(kEmail, _, _))
      .WillOnce(
          WithArgs<1, 2>([&captured_dns_is_verifiable_cb](
                             base::OnceClosure dns_cb,
                             EmailVerifier::IsVerifiableCallback callback) {
            captured_dns_is_verifiable_cb = std::move(callback);
            std::move(dns_cb).Run();
          }));

  StrictMock<base::MockCallback<EmailVerifier::IsVerifiableCallback>>
      dns_verifiable_cb;
  EXPECT_CALL(dns_verifiable_cb, Run).Times(0);
  bool dns_callback_ran = false;
  verifier->CheckIfVerifiable(
      kEmail,
      base::BindLambdaForTesting(
          [&verifier, &dns_request_destroyed, &dns_callback_ran]() {
            EXPECT_FALSE(dns_request_destroyed);
            verifier.reset();
            EXPECT_TRUE(dns_request_destroyed);
            dns_callback_ran = true;
          }),
      dns_verifiable_cb.Get());
  EXPECT_TRUE(dns_callback_ran);
  ASSERT_TRUE(captured_dns_is_verifiable_cb);
  std::move(captured_dns_is_verifiable_cb)
      .Run(std::nullopt,
           blink::mojom::EmailVerificationRequestResult::kUserLoggedOut,
           base::Milliseconds(100));

  // 2. Completing `CheckIfVerifiable()` destroys the request before running
  // `IsVerifiableCallback`, so destroying `verifier` inside it is safe.
  verifier.emplace(base::BindRepeating(&MockRequestBuilder::Run,
                                       base::Unretained(&builder)));

  EmailVerifier::Result issuer;
  issuer.email = kEmail;
  issuer.issuer_origin = url::Origin::Create(GURL("https://example.com"));

  EXPECT_CALL(*is_verifiable_request_ptr, CheckIfVerifiable(kEmail, _, _))
      .WillOnce(
          WithArgs<2>([&issuer](EmailVerifier::IsVerifiableCallback callback) {
            std::move(callback).Run(
                issuer, blink::mojom::EmailVerificationRequestResult::kSuccess,
                base::Milliseconds(100));
          }));

  bool is_verifiable_callback_ran = false;
  verifier->CheckIfVerifiable(
      kEmail, base::DoNothing(),
      base::BindLambdaForTesting(
          [&verifier, &issuer, &is_verifiable_destroyed,
           &is_verifiable_callback_ran](
              std::optional<EmailVerifier::Result> result,
              blink::mojom::EmailVerificationRequestResult status,
              base::TimeDelta duration) {
            EXPECT_EQ(result, issuer);
            EXPECT_EQ(status,
                      blink::mojom::EmailVerificationRequestResult::kSuccess);
            EXPECT_EQ(duration, base::Milliseconds(100));
            EXPECT_TRUE(is_verifiable_destroyed);
            verifier.reset();
            is_verifiable_callback_ran = true;
          }));
  EXPECT_TRUE(is_verifiable_callback_ran);

  // 3. Completing `Verify()` destroys the request before running
  // `OnEmailVerifiedCallback`, so destroying `verifier` inside it is safe.
  verifier.emplace(base::BindRepeating(&MockRequestBuilder::Run,
                                       base::Unretained(&builder)));

  EXPECT_CALL(*verify_request_ptr, Verify(_, kNonce, _))
      .WillOnce(
          WithArgs<2>([](EmailVerifier::OnEmailVerifiedCallback callback) {
            std::move(callback).Run(
                kToken, blink::mojom::EmailVerificationRequestResult::kSuccess,
                base::Milliseconds(200));
          }));

  bool verify_callback_ran = false;
  verifier->Verify(
      issuer, kNonce,
      base::BindLambdaForTesting(
          [&verifier, &verify_destroyed, &verify_callback_ran](
              std::optional<std::string> token,
              blink::mojom::EmailVerificationRequestResult status,
              base::TimeDelta duration) {
            EXPECT_EQ(token, kToken);
            EXPECT_EQ(status,
                      blink::mojom::EmailVerificationRequestResult::kSuccess);
            EXPECT_EQ(duration, base::Milliseconds(200));
            EXPECT_TRUE(verify_destroyed);
            verifier.reset();
            verify_callback_ran = true;
          }));
  EXPECT_TRUE(verify_callback_ran);
}

// Verifies that `EmailVerifierImpl::CheckIfVerifiable()` forwards both non-null
// and null `on_dns_resolved_callback` closures to `EmailVerificationRequest`.
TEST_F(EmailVerifierImplTest, ForwardsDnsResolvedCallback) {
  auto request =
      std::make_unique<NiceMock<MockEmailVerificationRequest>>(*main_rfh());
  MockEmailVerificationRequest* request_ptr = request.get();
  auto null_cb_request =
      std::make_unique<NiceMock<MockEmailVerificationRequest>>(*main_rfh());
  MockEmailVerificationRequest* null_cb_request_ptr = null_cb_request.get();

  MockRequestBuilder builder;
  EXPECT_CALL(builder, Run)
      .WillOnce(Return(ByMove(std::move(request))))
      .WillOnce(Return(ByMove(std::move(null_cb_request))));

  EmailVerifierImpl verifier(base::BindRepeating(&MockRequestBuilder::Run,
                                                 base::Unretained(&builder)));

  base::MockCallback<base::OnceClosure> dns_resolved_cb;
  EXPECT_CALL(*request_ptr, CheckIfVerifiable(kEmail, _, _))
      .WillOnce(WithArgs<1>([](base::OnceClosure cb) { std::move(cb).Run(); }));

  EXPECT_CALL(dns_resolved_cb, Run).Times(1);

  verifier.CheckIfVerifiable(kEmail, dns_resolved_cb.Get(), base::DoNothing());

  bool checked_null_cb = false;
  EXPECT_CALL(*null_cb_request_ptr, CheckIfVerifiable(kEmail, _, _))
      .WillOnce(WithArgs<1>([&checked_null_cb](base::OnceClosure cb) {
        EXPECT_FALSE(cb);
        checked_null_cb = true;
      }));

  verifier.CheckIfVerifiable(kEmail, base::NullCallback(), base::DoNothing());
  EXPECT_TRUE(checked_null_cb);
}

}  // namespace content::webid
