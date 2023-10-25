// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/ranges/algorithm.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/task_environment.h"
#import "components/password_manager/core/browser/leak_detection/bulk_leak_check.h"
#import "components/password_manager/core/browser/leak_detection/bulk_leak_check_service.h"
#import "components/password_manager/core/browser/leak_detection/leak_detection_check_factory.h"
#import "components/password_manager/core/browser/leak_detection/leak_detection_request_utils.h"
#import "components/password_manager/core/browser/leak_detection/mock_leak_detection_check_factory.h"
#import "components/signin/public/identity_manager/identity_test_environment.h"
#import "ios/web_view/internal/passwords/cwv_leak_check_credential_internal.h"
#import "ios/web_view/internal/passwords/cwv_leak_check_service_internal.h"
#import "ios/web_view/public/cwv_leak_check_service_observer.h"
#import "services/network/test/test_shared_url_loader_factory.h"

#import "testing/gtest/include/gtest/gtest.h"

#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

using password_manager::BulkLeakCheck;
using password_manager::BulkLeakCheckDelegateInterface;
using password_manager::BulkLeakCheckService;
using password_manager::IsLeaked;
using password_manager::LeakCheckCredential;
using password_manager::LeakDetectionInitiator;
using password_manager::MockLeakDetectionCheckFactory;

namespace ios_web_view {

namespace {
constexpr char16_t kUsername1[] = u"bob";
constexpr char16_t kPassword1[] = u"password";
constexpr char16_t kUsername2[] = u"alice";
constexpr char16_t kPassword2[] = u"secret";
}  // namespace

// A Fake BulkLeakCheck to avoid going to the network.
class FakeBulkLeakCheck : public BulkLeakCheck {
 public:
  FakeBulkLeakCheck(BulkLeakCheckDelegateInterface* delegate)
      : delegate_(delegate) {}

  void CheckCredentials(LeakDetectionInitiator initiator,
                        std::vector<LeakCheckCredential> checks) override {
    std::move(checks.begin(), checks.end(), std::back_inserter(queue_));
  }

  size_t GetPendingChecksCount() const override { return queue_.size(); }

  // Test helper to see what credentials have been queued.
  std::deque<LeakCheckCredential> const& queue() const { return queue_; }

  // Test helper to finish and notify the next check has completed.
  // |is_leaked| The fake leak check result for the check.
  void FinishNext(IsLeaked is_leaked) {
    DCHECK(!queue_.empty());
    auto check = std::move(queue_.front());
    queue_.pop_front();
    delegate_->OnFinishedCredential(std::move(check), is_leaked);
  }

 private:
  BulkLeakCheckDelegateInterface* delegate_;
  std::deque<LeakCheckCredential> queue_;
};

// A stub for MockLeakDetectionCheckFactory::TryCreateBulkLeakCheck that creates
// a FakeBulkLeakCheck and assigns its raw pointer to *out for the test to
// control.
ACTION_P(CreateFakeBulkLeakCheck, out) {
  auto value = std::make_unique<FakeBulkLeakCheck>(arg0);
  *out = value.get();
  return std::move(value);
}

MATCHER_P(CredentialsAre, credentials, "") {
  return base::ranges::equal(arg, credentials.get(),
                             [](const auto& lhs, const auto& rhs) {
                               return lhs.username() == rhs.username() &&
                                      lhs.password() == rhs.password();
                             });
}

class CWVLeakCheckServiceTest : public PlatformTest {
 public:
  CWVLeakCheckServiceTest() {
    // Use a mock leak detection factory so we can inject our fake.
    auto mock_leak_factory = std::make_unique<MockLeakDetectionCheckFactory>();
    mock_leak_factory_ = mock_leak_factory.get();
    service_interface_.set_leak_factory(std::move(mock_leak_factory));
    service_ = [[CWVLeakCheckService alloc]
        initWithBulkLeakCheckService:&service_interface_];
  }

 protected:
  MockLeakDetectionCheckFactory& mock_leak_factory() {
    return *mock_leak_factory_;
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  signin::IdentityTestEnvironment identity_test_env_;
  BulkLeakCheckService service_interface_{
      identity_test_env_.identity_manager(),
      base::MakeRefCounted<network::TestSharedURLLoaderFactory>()};
  raw_ptr<MockLeakDetectionCheckFactory> mock_leak_factory_ = nullptr;
  CWVLeakCheckService* service_;
};

// Tests that state initializes to idle and changes to running
// when processing credentials.
TEST_F(CWVLeakCheckServiceTest, State) {
  CWVLeakCheckCredential* credential = [[CWVLeakCheckCredential alloc]
      initWithCredential:std::make_unique<LeakCheckCredential>(kUsername1,
                                                               kPassword1)];

  EXPECT_EQ(CWVLeakCheckServiceStateIdle, service_.state);

  FakeBulkLeakCheck* leak_check;
  EXPECT_CALL(mock_leak_factory(), TryCreateBulkLeakCheck)
      .WillOnce(CreateFakeBulkLeakCheck(&leak_check));
  [service_ checkCredentials:@[ credential ]];

  EXPECT_EQ(CWVLeakCheckServiceStateRunning, service_.state);
}

// Tests that cancel changes the state to canceled.
TEST_F(CWVLeakCheckServiceTest, Cancel) {
  CWVLeakCheckCredential* credential = [[CWVLeakCheckCredential alloc]
      initWithCredential:std::make_unique<LeakCheckCredential>(kUsername1,
                                                               kPassword1)];

  EXPECT_EQ(CWVLeakCheckServiceStateIdle, service_.state);

  FakeBulkLeakCheck* leak_check;
  EXPECT_CALL(mock_leak_factory(), TryCreateBulkLeakCheck)
      .WillOnce(CreateFakeBulkLeakCheck(&leak_check));
  [service_ checkCredentials:@[ credential ]];
  [service_ cancel];

  EXPECT_EQ(CWVLeakCheckServiceStateCanceled, service_.state);
}

// Tests that credentials are converted to internal LeakCheckCredential.
TEST_F(CWVLeakCheckServiceTest, PreparesCredentials) {
  CWVLeakCheckCredential* credential1 = [[CWVLeakCheckCredential alloc]
      initWithCredential:std::make_unique<LeakCheckCredential>(kUsername1,
                                                               kPassword1)];
  CWVLeakCheckCredential* credential2 = [[CWVLeakCheckCredential alloc]
      initWithCredential:std::make_unique<LeakCheckCredential>(kUsername2,
                                                               kPassword2)];

  std::vector<LeakCheckCredential> expected;
  expected.emplace_back(kUsername1, kPassword1);
  expected.emplace_back(kUsername2, kPassword2);

  FakeBulkLeakCheck* leak_check;
  EXPECT_CALL(mock_leak_factory(), TryCreateBulkLeakCheck)
      .WillOnce(CreateFakeBulkLeakCheck(&leak_check));
  [service_ checkCredentials:@[ credential1, credential2 ]];

  EXPECT_THAT(leak_check->queue(), CredentialsAre(std::cref(expected)));
}

// Tests that observers are notified of state changes.
TEST_F(CWVLeakCheckServiceTest, DidChangeState) {
  CWVLeakCheckCredential* credential = [[CWVLeakCheckCredential alloc]
      initWithCredential:std::make_unique<LeakCheckCredential>(kUsername1,
                                                               kPassword1)];

  id observer = OCMProtocolMock(@protocol(CWVLeakCheckServiceObserver));

  [service_ addObserver:observer];
  [[observer expect] leakCheckServiceDidChangeState:service_];
  FakeBulkLeakCheck* leak_check;
  EXPECT_CALL(mock_leak_factory(), TryCreateBulkLeakCheck)
      .WillOnce(CreateFakeBulkLeakCheck(&leak_check));
  [service_ checkCredentials:@[ credential ]];
  [observer verify];

  leak_check->FinishNext(IsLeaked(false));

  [service_ removeObserver:observer];
  [[observer reject] leakCheckServiceDidChangeState:service_];
  EXPECT_CALL(mock_leak_factory(), TryCreateBulkLeakCheck)
      .WillOnce(CreateFakeBulkLeakCheck(&leak_check));
  [service_ checkCredentials:@[ credential ]];
  [observer verify];
}

// Tests that observers are notified of completed credentials.
TEST_F(CWVLeakCheckServiceTest, DidCheckCredential) {
  CWVLeakCheckCredential* credential1 = [[CWVLeakCheckCredential alloc]
      initWithCredential:std::make_unique<LeakCheckCredential>(kUsername1,
                                                               kPassword1)];
  CWVLeakCheckCredential* credential2 = [[CWVLeakCheckCredential alloc]
      initWithCredential:std::make_unique<LeakCheckCredential>(kUsername2,
                                                               kPassword2)];

  id observer = OCMProtocolMock(@protocol(CWVLeakCheckServiceObserver));
  [service_ addObserver:observer];
  [[observer expect] leakCheckService:service_
                   didCheckCredential:credential1
                             isLeaked:false];
  [[observer expect] leakCheckService:service_
                   didCheckCredential:credential2
                             isLeaked:true];

  FakeBulkLeakCheck* leak_check;
  EXPECT_CALL(mock_leak_factory(), TryCreateBulkLeakCheck)
      .WillOnce(CreateFakeBulkLeakCheck(&leak_check));
  [service_ checkCredentials:@[ credential1, credential2 ]];

  leak_check->FinishNext(IsLeaked(false));
  leak_check->FinishNext(IsLeaked(true));

  [observer verify];
}

}  // namespace ios_web_view
