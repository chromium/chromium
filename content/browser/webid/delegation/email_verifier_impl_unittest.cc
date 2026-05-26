// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/delegation/email_verifier_impl.h"

#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
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
      : EmailVerificationRequest(
            nullptr,
            nullptr,
            nullptr,
            static_cast<RenderFrameHostImpl&>(rfh).GetSafeRef()) {}
  ~MockEmailVerificationRequest() override { Destroy(); }

  MOCK_METHOD(void, Destroy, (), ());
  MOCK_METHOD(void,
              Send,
              (const std::string&,
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
};

TEST_F(EmailVerifierImplTest, TestSingleRequest) {
  auto request =
      std::make_unique<StrictMock<MockEmailVerificationRequest>>(*main_rfh());
  auto* request_ptr = request.get();

  MockRequestBuilder builder;
  EXPECT_CALL(builder, Run).WillOnce(Return(ByMove(std::move(request))));

  EmailVerifierImpl verifier(base::BindRepeating(&MockRequestBuilder::Run,
                                                 base::Unretained(&builder)));

  EXPECT_CALL(*request_ptr, Send("test@example.com", "nonce", _))
      .WillOnce(
          WithArgs<2>([&](EmailVerifier::OnEmailVerifiedCallback callback) {
            std::move(callback).Run(EmailVerifier::Result{
                "token", net::SchemefulSite(GURL("https://example.com"))});
          }));
  EXPECT_CALL(*request_ptr, Destroy());

  base::MockCallback<EmailVerifier::OnEmailVerifiedCallback> cb;
  EXPECT_CALL(cb,
              Run(Optional(EmailVerifier::Result{
                  "token", net::SchemefulSite(GURL("https://example.com"))})));
  verifier.Verify("test@example.com", "nonce", cb.Get());
}

TEST_F(EmailVerifierImplTest, TestTwoConcurrentRequests) {
  auto request1 =
      std::make_unique<StrictMock<MockEmailVerificationRequest>>(*main_rfh());
  auto* request_ptr1 = request1.get();
  auto request2 =
      std::make_unique<StrictMock<MockEmailVerificationRequest>>(*main_rfh());
  auto* request_ptr2 = request2.get();

  MockRequestBuilder builder;
  EXPECT_CALL(builder, Run)
      .WillOnce(Return(ByMove(std::move(request1))))
      .WillOnce(Return(ByMove(std::move(request2))));

  EmailVerifierImpl verifier(base::BindRepeating(&MockRequestBuilder::Run,
                                                 base::Unretained(&builder)));

  // Set up expectations and capture callbacks for the two requests.
  EmailVerifier::OnEmailVerifiedCallback callback1;
  EXPECT_CALL(*request_ptr1, Send("test1@example.com", "nonce1", _))
      .WillOnce(
          WithArgs<2>([&](EmailVerifier::OnEmailVerifiedCallback callback) {
            callback1 = std::move(callback);
          }));

  EmailVerifier::OnEmailVerifiedCallback callback2;
  EXPECT_CALL(*request_ptr2, Send("test2@example.com", "nonce2", _))
      .WillOnce(
          WithArgs<2>([&](EmailVerifier::OnEmailVerifiedCallback callback) {
            callback2 = std::move(callback);
          }));

  // Make the concurrent calls to Verify.
  base::MockCallback<EmailVerifier::OnEmailVerifiedCallback> cb1;
  verifier.Verify("test1@example.com", "nonce1", cb1.Get());

  base::MockCallback<EmailVerifier::OnEmailVerifiedCallback> cb2;
  verifier.Verify("test2@example.com", "nonce2", cb2.Get());

  ASSERT_TRUE(callback1);
  ASSERT_TRUE(callback2);

  // Set up expectations for the final callbacks and object destruction.
  EXPECT_CALL(*request_ptr1, Destroy());
  EXPECT_CALL(*request_ptr2, Destroy());
  EXPECT_CALL(cb1,
              Run(Optional(EmailVerifier::Result{
                  "token1", net::SchemefulSite(GURL("https://example.com"))})));
  EXPECT_CALL(cb2,
              Run(Optional(EmailVerifier::Result{
                  "token2", net::SchemefulSite(GURL("https://example.com"))})));

  // Complete in reverse order to test concurrency.
  std::move(callback2).Run(EmailVerifier::Result{
      "token2", net::SchemefulSite(GURL("https://example.com"))});
  std::move(callback1).Run(EmailVerifier::Result{
      "token1", net::SchemefulSite(GURL("https://example.com"))});
}

}  // namespace content::webid
