// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/oauth_token_getter_proxy.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
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

  void ResolveCallback(Status status, const OAuthTokenInfo& token_info);

  void ExpectInvalidateCache();

  // OAuthTokenGetter overrides.
  void CallWithToken(TokenCallback on_access_token) override;
  void InvalidateCache() override;
  base::WeakPtr<OAuthTokenGetter> GetWeakPtr() override;

  base::WeakPtr<FakeOAuthTokenGetter> GetFakeOAuthTokenGetterWeakPtr();

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
                                           const OAuthTokenInfo& token_info) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!on_access_token_.is_null());
  std::move(on_access_token_).Run(status, token_info);
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

base::WeakPtr<OAuthTokenGetter> FakeOAuthTokenGetter::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

base::WeakPtr<FakeOAuthTokenGetter>
FakeOAuthTokenGetter::GetFakeOAuthTokenGetterWeakPtr() {
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
                                       const OAuthTokenInfo& token_info);

  void TestCallWithTokenOnMainThread(OAuthTokenGetter::Status status,
                                     const OAuthTokenInfo& token_info);

  void ExpectInvalidateCache();

  void InvalidateTokenGetter();

  scoped_refptr<base::SingleThreadTaskRunner> runner_task_runner_;
  std::unique_ptr<FakeOAuthTokenGetter> token_getter_;
  std::unique_ptr<OAuthTokenGetterProxy> proxy_;

 private:
  struct TokenCallbackResult {
    OAuthTokenGetter::Status status;
    std::string user_email;
    std::string access_token;
  };

  void TestCallWithTokenImpl(OAuthTokenGetter::Status status,
                             const OAuthTokenInfo& token_info,
                             base::OnceClosure on_done);

  void OnTokenReceived(base::OnceClosure on_done,
                       OAuthTokenGetter::Status status,
                       const OAuthTokenInfo& token_info);

  std::unique_ptr<TokenCallbackResult> expected_callback_result_;

  base::test::TaskEnvironment task_environment_;
};

void OAuthTokenGetterProxyTest::SetUp() {
  token_getter_ = std::make_unique<FakeOAuthTokenGetter>();
  runner_task_runner_ = base::ThreadPool::CreateSingleThreadTaskRunner({});
  proxy_ = std::make_unique<OAuthTokenGetterProxy>(token_getter_->GetWeakPtr(),
                                                   runner_task_runner_);
}

void OAuthTokenGetterProxyTest::TearDown() {
  InvalidateTokenGetter();
  proxy_.reset();
  base::test::TestFuture<void> future;
  task_environment_.GetMainThreadTaskRunner()->PostTask(FROM_HERE,
                                                        future.GetCallback());
  EXPECT_TRUE(future.Wait());
  ASSERT_FALSE(expected_callback_result_);
}

void OAuthTokenGetterProxyTest::TestCallWithTokenOnRunnerThread(
    OAuthTokenGetter::Status status,
    const OAuthTokenInfo& token_info) {
  base::test::TestFuture<void> future;
  runner_task_runner_->PostTask(
      FROM_HERE,
      base::BindLambdaForTesting(
          [&, quit_cb = future.GetSequenceBoundCallback()]() mutable {
            TestCallWithTokenImpl(status, token_info, std::move(quit_cb));
          }));
  ASSERT_TRUE(future.Wait());
}

void OAuthTokenGetterProxyTest::TestCallWithTokenOnMainThread(
    OAuthTokenGetter::Status status,
    const OAuthTokenInfo& token_info) {
  base::test::TestFuture<void> future;
  TestCallWithTokenImpl(status, token_info, future.GetCallback());
  ASSERT_TRUE(future.Wait());
}

void OAuthTokenGetterProxyTest::ExpectInvalidateCache() {
  ASSERT_NE(token_getter_.get(), nullptr);
  runner_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&FakeOAuthTokenGetter::ExpectInvalidateCache,
                     token_getter_->GetFakeOAuthTokenGetterWeakPtr()));
}

void OAuthTokenGetterProxyTest::InvalidateTokenGetter() {
  if (token_getter_) {
    runner_task_runner_->DeleteSoon(FROM_HERE, token_getter_.release());
  }
}

void OAuthTokenGetterProxyTest::TestCallWithTokenImpl(
    OAuthTokenGetter::Status status,
    const OAuthTokenInfo& token_info,
    base::OnceClosure on_done) {
  ASSERT_FALSE(expected_callback_result_);
  expected_callback_result_ = std::make_unique<TokenCallbackResult>();
  expected_callback_result_->status = status;
  expected_callback_result_->user_email = token_info.user_email();
  expected_callback_result_->access_token = token_info.access_token();
  proxy_->CallWithToken(
      base::BindOnce(&OAuthTokenGetterProxyTest::OnTokenReceived,
                     base::Unretained(this), std::move(on_done)));
  runner_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&FakeOAuthTokenGetter::ResolveCallback,
                                token_getter_->GetFakeOAuthTokenGetterWeakPtr(),
                                status, token_info));
}

void OAuthTokenGetterProxyTest::OnTokenReceived(
    base::OnceClosure on_done,
    OAuthTokenGetter::Status status,
    const OAuthTokenInfo& token_info) {
  ASSERT_TRUE(expected_callback_result_);
  EXPECT_EQ(status, expected_callback_result_->status);
  EXPECT_EQ(token_info.user_email(), expected_callback_result_->user_email);
  EXPECT_EQ(token_info.access_token(), expected_callback_result_->access_token);
  expected_callback_result_.reset();
  std::move(on_done).Run();
}

TEST_F(OAuthTokenGetterProxyTest, CallWithTokenOnMainThread) {
  TestCallWithTokenOnMainThread(OAuthTokenGetter::Status::SUCCESS,
                                OAuthTokenInfo("token1", "email1"));
  TestCallWithTokenOnMainThread(OAuthTokenGetter::Status::NETWORK_ERROR,
                                OAuthTokenInfo("token2", "email2"));
}

TEST_F(OAuthTokenGetterProxyTest, CallWithTokenOnRunnerThread) {
  TestCallWithTokenOnRunnerThread(OAuthTokenGetter::Status::AUTH_ERROR,
                                  OAuthTokenInfo("token3", "email3"));
  TestCallWithTokenOnRunnerThread(OAuthTokenGetter::Status::SUCCESS,
                                  OAuthTokenInfo("token4", "email4"));
}

TEST_F(OAuthTokenGetterProxyTest, InvalidateCacheOnMainThread) {
  ExpectInvalidateCache();
  proxy_->InvalidateCache();
  base::test::TestFuture<void> future;
  runner_task_runner_->PostTask(FROM_HERE, future.GetSequenceBoundCallback());
  ASSERT_TRUE(future.Wait());
}

TEST_F(OAuthTokenGetterProxyTest, InvalidateCacheOnRunnerThread) {
  ExpectInvalidateCache();
  runner_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&OAuthTokenGetterProxy::InvalidateCache,
                                base::Unretained(proxy_.get())));
  base::test::TestFuture<void> future;
  runner_task_runner_->PostTask(FROM_HERE, future.GetSequenceBoundCallback());
  ASSERT_TRUE(future.Wait());
}

TEST_F(
    OAuthTokenGetterProxyTest,
    CallWithTokenOnMainThreadAfterTokenGetterDestroyed_callsSilentlyDropped) {
  InvalidateTokenGetter();
  proxy_->CallWithToken(GetDoNothingTokenCallback());
  base::test::TestFuture<void> future;
  runner_task_runner_->PostTask(FROM_HERE, future.GetSequenceBoundCallback());
  ASSERT_TRUE(future.Wait());
}

TEST_F(
    OAuthTokenGetterProxyTest,
    CallWithTokenOnRunnerThreadAfterTokenGetterDestroyed_callsSilentlyDropped) {
  InvalidateTokenGetter();
  runner_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&OAuthTokenGetterProxy::CallWithToken,
                                base::Unretained(proxy_.get()),
                                GetDoNothingTokenCallback()));
  base::test::TestFuture<void> future;
  runner_task_runner_->PostTask(FROM_HERE, future.GetSequenceBoundCallback());
  ASSERT_TRUE(future.Wait());
}

TEST_F(
    OAuthTokenGetterProxyTest,
    InvalidateCacheOnMainThreadAfterTokenGetterDestroyed_callsSilentlyDropped) {
  InvalidateTokenGetter();
  proxy_->InvalidateCache();
  base::test::TestFuture<void> future;
  runner_task_runner_->PostTask(FROM_HERE, future.GetSequenceBoundCallback());
  ASSERT_TRUE(future.Wait());
}

TEST_F(
    OAuthTokenGetterProxyTest,
    InvalidateCacheOnRunnerThreadAfterTokenGetterDestroyed_callsSilentlyDropped) {
  InvalidateTokenGetter();
  runner_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&OAuthTokenGetterProxy::InvalidateCache,
                                base::Unretained(proxy_.get())));
  base::test::TestFuture<void> future;
  runner_task_runner_->PostTask(FROM_HERE, future.GetSequenceBoundCallback());
  ASSERT_TRUE(future.Wait());
}

}  // namespace remoting
