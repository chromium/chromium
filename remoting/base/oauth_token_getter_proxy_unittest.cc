// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/oauth_token_getter_proxy.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "base/threading/thread_checker.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace {

OAuthTokenGetter::TokenCallback GetDoNothingTokenCallback() {
  return base::DoNothing();
}

class FakeOAuthTokenGetter : public OAuthTokenGetter {
 public:
  FakeOAuthTokenGetter();

  FakeOAuthTokenGetter(const FakeOAuthTokenGetter&) = delete;
  FakeOAuthTokenGetter& operator=(const FakeOAuthTokenGetter&) = delete;

  ~FakeOAuthTokenGetter() override;

  void ResolveCallback(Status status,
                       const std::string& user_email,
                       const std::string& access_token,
                       const std::string& scopes);

  void ExpectInvalidateCache();

  // OAuthTokenGetter overrides.
  void CallWithToken(TokenCallback on_access_token) override;
  void InvalidateCache() override;

  base::WeakPtr<FakeOAuthTokenGetter> GetWeakPtr();

 private:
  TokenCallback on_access_token_;
  bool invalidate_cache_expected_ = false;

  THREAD_CHECKER(thread_checker_);

  base::WeakPtrFactory<FakeOAuthTokenGetter> weak_factory_{this};
};

FakeOAuthTokenGetter::FakeOAuthTokenGetter() {
  DETACH_FROM_THREAD(thread_checker_);
}

FakeOAuthTokenGetter::~FakeOAuthTokenGetter() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!invalidate_cache_expected_);
}

void FakeOAuthTokenGetter::ResolveCallback(Status status,
                                           const std::string& user_email,
                                           const std::string& access_token,
                                           const std::string& scopes) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!on_access_token_.is_null());
  std::move(on_access_token_).Run(status, user_email, access_token, scopes);
}

void FakeOAuthTokenGetter::ExpectInvalidateCache() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  ASSERT_FALSE(invalidate_cache_expected_);
  invalidate_cache_expected_ = true;
}

void FakeOAuthTokenGetter::CallWithToken(TokenCallback on_access_token) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  on_access_token_ = std::move(on_access_token);
}

void FakeOAuthTokenGetter::InvalidateCache() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  ASSERT_TRUE(invalidate_cache_expected_);
  invalidate_cache_expected_ = false;
}

base::WeakPtr<FakeOAuthTokenGetter> FakeOAuthTokenGetter::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace

class OAuthTokenGetterProxyTest : public testing::Test {
 public:
  OAuthTokenGetterProxyTest() = default;

  OAuthTokenGetterProxyTest(const OAuthTokenGetterProxyTest&) = delete;
  OAuthTokenGetterProxyTest& operator=(const OAuthTokenGetterProxyTest&) =
      delete;

  ~OAuthTokenGetterProxyTest() override = default;

  // testing::Test overrides.
  void SetUp() override;
  void TearDown() override;

 protected:
  void TestCallWithTokenOnRunnerThread(OAuthTokenGetter::Status status,
                                       const std::string& user_email,
                                       const std::string& access_token,
                                       const std::string& scopes);

  void TestCallWithTokenOnMainThread(OAuthTokenGetter::Status status,
                                     const std::string& user_email,
                                     const std::string& access_token,
                                     const std::string& scopes);

  void ExpectInvalidateCache();

  void InvalidateTokenGetter();

  base::Thread runner_thread_{"runner_thread"};
  std::unique_ptr<FakeOAuthTokenGetter> token_getter_;
  std::unique_ptr<OAuthTokenGetterProxy> proxy_;

 private:
  struct TokenCallbackResult {
    OAuthTokenGetter::Status status;
    std::string user_email;
    std::string access_token;
    std::string scopes;
  };

  void TestCallWithTokenImpl(OAuthTokenGetter::Status status,
                             const std::string& user_email,
                             const std::string& access_token,
                             const std::string& scopes);

  void OnTokenReceived(OAuthTokenGetter::Status status,
                       const std::string& user_email,
                       const std::string& access_token,
                       const std::string& scopes);

  std::unique_ptr<TokenCallbackResult> expected_callback_result_;

  base::test::SingleThreadTaskEnvironment task_environment_;
};

void OAuthTokenGetterProxyTest::SetUp() {
  token_getter_ = std::make_unique<FakeOAuthTokenGetter>();
  runner_thread_.Start();
  proxy_ = std::make_unique<OAuthTokenGetterProxy>(
      token_getter_->GetWeakPtr(), runner_thread_.task_runner());
}

void OAuthTokenGetterProxyTest::TearDown() {
  InvalidateTokenGetter();
  proxy_.reset();
  runner_thread_.FlushForTesting();
  ASSERT_FALSE(expected_callback_result_);
}

void OAuthTokenGetterProxyTest::TestCallWithTokenOnRunnerThread(
    OAuthTokenGetter::Status status,
    const std::string& user_email,
    const std::string& access_token,
    const std::string& scopes) {
  runner_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&OAuthTokenGetterProxyTest::TestCallWithTokenImpl,
                     base::Unretained(this),
                     OAuthTokenGetter::Status::AUTH_ERROR, user_email,
                     access_token, scopes));
  runner_thread_.FlushForTesting();
}

void OAuthTokenGetterProxyTest::TestCallWithTokenOnMainThread(
    OAuthTokenGetter::Status status,
    const std::string& user_email,
    const std::string& access_token,
    const std::string& scopes) {
  TestCallWithTokenImpl(status, user_email, access_token, scopes);
  runner_thread_.FlushForTesting();
  base::RunLoop().RunUntilIdle();
}

void OAuthTokenGetterProxyTest::ExpectInvalidateCache() {
  ASSERT_NE(nullptr, token_getter_.get());
  runner_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&FakeOAuthTokenGetter::ExpectInvalidateCache,
                                token_getter_->GetWeakPtr()));
}

void OAuthTokenGetterProxyTest::InvalidateTokenGetter() {
  if (token_getter_) {
    runner_thread_.task_runner()->DeleteSoon(FROM_HERE,
                                             token_getter_.release());
  }
}

void OAuthTokenGetterProxyTest::TestCallWithTokenImpl(
    OAuthTokenGetter::Status status,
    const std::string& user_email,
    const std::string& access_token,
    const std::string& scopes) {
  ASSERT_FALSE(expected_callback_result_);
  expected_callback_result_ = std::make_unique<TokenCallbackResult>();
  expected_callback_result_->status = status;
  expected_callback_result_->user_email = user_email;
  expected_callback_result_->access_token = access_token;
  expected_callback_result_->scopes = scopes;
  proxy_->CallWithToken(base::BindOnce(
      &OAuthTokenGetterProxyTest::OnTokenReceived, base::Unretained(this)));
  runner_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&FakeOAuthTokenGetter::ResolveCallback,
                                token_getter_->GetWeakPtr(), status, user_email,
                                access_token, scopes));
}

void OAuthTokenGetterProxyTest::OnTokenReceived(OAuthTokenGetter::Status status,
                                                const std::string& user_email,
                                                const std::string& access_token,
                                                const std::string& scopes) {
  ASSERT_TRUE(expected_callback_result_);
  EXPECT_EQ(expected_callback_result_->status, status);
  EXPECT_EQ(expected_callback_result_->user_email, user_email);
  EXPECT_EQ(expected_callback_result_->access_token, access_token);
  EXPECT_EQ(expected_callback_result_->scopes, scopes);
  expected_callback_result_.reset();
}

TEST_F(OAuthTokenGetterProxyTest, CallWithTokenOnMainThread) {
  TestCallWithTokenOnMainThread(OAuthTokenGetter::Status::SUCCESS, "email1",
                                "token1", "scope1");
  TestCallWithTokenOnMainThread(OAuthTokenGetter::Status::NETWORK_ERROR,
                                "email2", "token2", "scope2");
}

TEST_F(OAuthTokenGetterProxyTest, CallWithTokenOnRunnerThread) {
  TestCallWithTokenOnRunnerThread(OAuthTokenGetter::Status::AUTH_ERROR,
                                  "email3", "token3", "scope3");
  TestCallWithTokenOnRunnerThread(OAuthTokenGetter::Status::SUCCESS, "email4",
                                  "token4", "scope4");
}

TEST_F(OAuthTokenGetterProxyTest, InvalidateCacheOnMainThread) {
  ExpectInvalidateCache();
  proxy_->InvalidateCache();
  runner_thread_.FlushForTesting();
}

TEST_F(OAuthTokenGetterProxyTest, InvalidateCacheOnRunnerThread) {
  ExpectInvalidateCache();
  runner_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&OAuthTokenGetterProxy::InvalidateCache,
                                base::Unretained(proxy_.get())));
  runner_thread_.FlushForTesting();
}

TEST_F(
    OAuthTokenGetterProxyTest,
    CallWithTokenOnMainThreadAfterTokenGetterDestroyed_callsSilentlyDropped) {
  InvalidateTokenGetter();
  proxy_->CallWithToken(GetDoNothingTokenCallback());
  runner_thread_.FlushForTesting();
}

TEST_F(
    OAuthTokenGetterProxyTest,
    CallWithTokenOnRunnerThreadAfterTokenGetterDestroyed_callsSilentlyDropped) {
  InvalidateTokenGetter();
  runner_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&OAuthTokenGetterProxy::CallWithToken,
                                base::Unretained(proxy_.get()),
                                GetDoNothingTokenCallback()));
  runner_thread_.FlushForTesting();
}

TEST_F(
    OAuthTokenGetterProxyTest,
    InvalidateCacheOnMainThreadAfterTokenGetterDestroyed_callsSilentlyDropped) {
  InvalidateTokenGetter();
  proxy_->InvalidateCache();
  runner_thread_.FlushForTesting();
}

TEST_F(
    OAuthTokenGetterProxyTest,
    InvalidateCacheOnRunnerThreadAfterTokenGetterDestroyed_callsSilentlyDropped) {
  InvalidateTokenGetter();
  runner_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&OAuthTokenGetterProxy::InvalidateCache,
                                base::Unretained(proxy_.get())));
  runner_thread_.FlushForTesting();
}

}  // namespace remoting
