// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/components/security_interstitials/safe_browsing/safe_browsing_query_manager.h"

#import <Foundation/Foundation.h>

#import "base/test/scoped_feature_list.h"
#import "components/safe_browsing/core/common/features.h"
#import "components/safe_browsing/ios/browser/safe_browsing_url_allow_list.h"
#import "components/security_interstitials/core/unsafe_resource.h"
#import "ios/components/security_interstitials/safe_browsing/fake_safe_browsing_client.h"
#import "ios/components/security_interstitials/safe_browsing/fake_safe_browsing_service.h"
#import "ios/web/public/test/fakes/fake_browser_state.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "services/network/public/mojom/fetch_api.mojom.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

using security_interstitials::UnsafeResource;
using testing::_;

namespace {
// Mock observer for tests.
class MockQueryManagerObserver : public SafeBrowsingQueryManager::Observer {
 public:
  MockQueryManagerObserver() {}
  ~MockQueryManagerObserver() override {}

  MOCK_METHOD4(SafeBrowsingQueryFinished,
               void(SafeBrowsingQueryManager*,
                    const SafeBrowsingQueryManager::Query&,
                    const SafeBrowsingQueryManager::Result&,
                    safe_browsing::SafeBrowsingUrlCheckerImpl::PerformedCheck
                        performed_check));

  MOCK_METHOD1(SafeBrowsingSyncQueryFinished,
               void(const SafeBrowsingQueryManager::QueryData&));

  MOCK_METHOD1(SafeBrowsingAsyncQueryFinished,
               void(const SafeBrowsingQueryManager::QueryData&));

  // Override rather than mocking so that the observer can remove itself.
  void SafeBrowsingQueryManagerDestroyed(
      SafeBrowsingQueryManager* manager) override {
    manager_destroyed_ = true;
    manager->RemoveObserver(this);
  }
  bool manager_destroyed() const { return manager_destroyed_; }

 private:
  bool manager_destroyed_ = false;
};

// Verifies the expected values passed to the SafeBrowsingQueryFinished()
// callback.
ACTION_P4(VerifyQueryFinished,
          expected_url,
          expected_http_method,
          is_url_safe) {
  const SafeBrowsingQueryManager::Query& query = arg1;
  EXPECT_EQ(expected_url, query.url);
  EXPECT_EQ(expected_http_method, query.http_method);

  const SafeBrowsingQueryManager::Result& result = arg2;
  EXPECT_EQ(is_url_safe, result.proceed);
  EXPECT_EQ(is_url_safe, !result.show_error_page);
  if (is_url_safe) {
    EXPECT_FALSE(result.resource);
  } else {
    ASSERT_TRUE(result.resource);
    UnsafeResource resource = result.resource.value();
    EXPECT_EQ(expected_url, resource.url);
    EXPECT_NE(safe_browsing::SBThreatType::SB_THREAT_TYPE_SAFE,
              resource.threat_type);
  }
}

// Verifies the expected values passed to the SafeBrowsingSyncQueryFinished
// callback.
ACTION_P4(VerifySyncQueryFinished,
          expected_url,
          expected_http_method,
          is_url_sync_safe,
          is_url_async_safe) {
  const SafeBrowsingQueryManager::QueryData& query_data = arg0;
  const SafeBrowsingQueryManager::Query& query = *query_data.query;
  EXPECT_EQ(expected_url, query.url);
  EXPECT_EQ(expected_http_method, query.http_method);

  const SafeBrowsingQueryManager::Result& result = *query_data.result;
  if (is_url_sync_safe && is_url_async_safe) {
    EXPECT_FALSE(result.resource);
    EXPECT_TRUE(result.proceed);
    EXPECT_FALSE(result.show_error_page);
  } else if (!is_url_sync_safe) {
    EXPECT_FALSE(result.proceed);
    EXPECT_TRUE(result.show_error_page);
    ASSERT_TRUE(result.resource);
    UnsafeResource resource = result.resource.value();
    EXPECT_EQ(expected_url, resource.url);
    EXPECT_NE(safe_browsing::SBThreatType::SB_THREAT_TYPE_SAFE,
              resource.threat_type);
  }
}

// Verifies the expected values passed to the SafeBrowsingAsyncQueryFinished
// callback.
ACTION_P4(VerifyAsyncQueryFinished,
          expected_url,
          expected_http_method,
          is_url_sync_safe,
          is_url_async_safe) {
  const SafeBrowsingQueryManager::QueryData& query_data = arg0;
  const SafeBrowsingQueryManager::Query& query = *query_data.query;
  EXPECT_EQ(expected_url, query.url);
  EXPECT_EQ(expected_http_method, query.http_method);

  const SafeBrowsingQueryManager::Result& result = *query_data.result;
  if (is_url_sync_safe && is_url_async_safe) {
    EXPECT_FALSE(result.resource);
    EXPECT_TRUE(result.proceed);
    EXPECT_FALSE(result.show_error_page);
  } else if (!is_url_async_safe) {
    EXPECT_FALSE(result.proceed);
    EXPECT_TRUE(result.show_error_page);
    ASSERT_TRUE(result.resource);
    UnsafeResource resource = result.resource.value();
    EXPECT_EQ(expected_url, resource.url);
    EXPECT_NE(safe_browsing::SBThreatType::SB_THREAT_TYPE_SAFE,
              resource.threat_type);
  }
}
}  // namespace

class SafeBrowsingQueryManagerTest : public testing::TestWithParam<bool> {
 protected:
  SafeBrowsingQueryManagerTest()
      : browser_state_(new web::FakeBrowserState()),
        web_state_(std::make_unique<web::FakeWebState>()),
        http_method_("GET"),
        use_async_safe_browsing_(GetParam()) {
    scoped_feature_list_.InitWithFeatureState(
        safe_browsing::kSafeBrowsingAsyncRealTimeCheck,
        use_async_safe_browsing_);
    SafeBrowsingQueryManager::CreateForWebState(web_state_.get(), &client_);
    SafeBrowsingUrlAllowList::CreateForWebState(web_state_.get());
    manager()->AddObserver(&observer_);
    web_state_->SetBrowserState(browser_state_.get());
  }

  SafeBrowsingQueryManager* manager() {
    return SafeBrowsingQueryManager::FromWebState(web_state_.get());
  }

  // Helper function to run all sync callbacks first then async callbacks.
  void RunSyncCallbacksThenAsyncCallbacks() {
    if (base::FeatureList::IsEnabled(
            safe_browsing::kSafeBrowsingAsyncRealTimeCheck)) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(^() {
            client_.run_sync_callbacks();
          }));
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(^() {
            client_.run_async_callbacks();
          }));
    }

    // TODO(crbug.com/359420122): Remove when clean up is complete.
    base::RunLoop().RunUntilIdle();
  }

  web::WebTaskEnvironment task_environment_{
      web::WebTaskEnvironment::MainThreadType::IO};
  MockQueryManagerObserver observer_;
  std::unique_ptr<web::FakeBrowserState> browser_state_;
  std::unique_ptr<web::FakeWebState> web_state_;
  std::string http_method_;
  FakeSafeBrowsingClient client_;
  bool use_async_safe_browsing_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(/* No Instantiation Name */,
                         SafeBrowsingQueryManagerTest,
                         testing::Bool());

// Tests a query for a safe URL.
TEST_P(SafeBrowsingQueryManagerTest, SafeURLQuery) {
  GURL url("http://chromium.test");
  if (base::FeatureList::IsEnabled(
          safe_browsing::kSafeBrowsingAsyncRealTimeCheck)) {
    EXPECT_CALL(observer_, SafeBrowsingSyncQueryFinished(_))
        .WillOnce(VerifySyncQueryFinished(url, http_method_,
                                          /*is_url_sync_safe=*/true,
                                          /*is_url_async_safe=*/true));
    EXPECT_CALL(observer_, SafeBrowsingAsyncQueryFinished(_))
        .WillOnce(VerifyAsyncQueryFinished(url, http_method_,
                                           /*is_url_sync_safe=*/true,
                                           /*is_url_async_safe=*/true));
  } else {
    EXPECT_CALL(observer_, SafeBrowsingQueryFinished(manager(), _, _, _))
        .WillOnce(VerifyQueryFinished(url, http_method_,
                                      /*is_url_safe=*/true));
  }

  // Start a URL check query for the safe URL and run the runloop until the
  // result is received.
  manager()->StartQuery(SafeBrowsingQueryManager::Query(url, http_method_));
  RunSyncCallbacksThenAsyncCallbacks();
}

// Tests a query for an unsafe URL.
TEST_P(SafeBrowsingQueryManagerTest, UnsafeURLQuery) {
  GURL url("http://" + FakeSafeBrowsingService::kUnsafeHost);
  if (base::FeatureList::IsEnabled(
          safe_browsing::kSafeBrowsingAsyncRealTimeCheck)) {
    EXPECT_CALL(observer_, SafeBrowsingSyncQueryFinished(_))
        .WillOnce(VerifySyncQueryFinished(url, http_method_,
                                          /*is_url_sync_safe=*/false,
                                          /*is_url_async_safe=*/false));
    EXPECT_CALL(observer_, SafeBrowsingAsyncQueryFinished(_))
        .WillOnce(VerifyAsyncQueryFinished(url, http_method_,
                                           /*is_url_sync_safe=*/false,
                                           /*is_url_async_safe=*/false));
  } else {
    EXPECT_CALL(observer_, SafeBrowsingQueryFinished(manager(), _, _, _))
        .WillOnce(VerifyQueryFinished(url, http_method_,
                                      /*is_url_safe=*/false));
  }

  // Start a URL check query for the unsafe URL and run the runloop until the
  // result is received.
  manager()->StartQuery(SafeBrowsingQueryManager::Query(url, http_method_));
  RunSyncCallbacksThenAsyncCallbacks();
}

// Tests a query for an unsafe URL with async checks enabled, where the URL
// is unsafe with both sync and async checks.
TEST_P(SafeBrowsingQueryManagerTest, SyncAndAsyncUnsafeURLQuery) {
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitAndEnableFeature(
      safe_browsing::kSafeBrowsingAsyncRealTimeCheck);
  GURL url("http://" + FakeSafeBrowsingService::kUnsafeHost);
  EXPECT_CALL(observer_, SafeBrowsingSyncQueryFinished(_))
      .WillOnce(VerifySyncQueryFinished(url, http_method_,
                                        /*is_url_sync_safe=*/false,
                                        /*is_url_async_safe=*/false));
  EXPECT_CALL(observer_, SafeBrowsingAsyncQueryFinished(_))
      .WillOnce(VerifyAsyncQueryFinished(url, http_method_,
                                         /*is_url_sync_safe=*/false,
                                         /*is_url_async_safe=*/false));

  // Start a URL check query for the unsafe URL and run the runloop until the
  // results are received.
  manager()->StartQuery(SafeBrowsingQueryManager::Query(url, http_method_));
  RunSyncCallbacksThenAsyncCallbacks();
}

// Tests a query for an unsafe URL with async checks enabled, where the URL
// is unsafe with async checks only.
TEST_P(SafeBrowsingQueryManagerTest, AsyncUnsafeURLQuery) {
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitAndEnableFeature(
      safe_browsing::kSafeBrowsingAsyncRealTimeCheck);
  GURL url("http://" + FakeSafeBrowsingService::kAsyncUnsafeHost);
  EXPECT_CALL(observer_, SafeBrowsingSyncQueryFinished(_))
      .WillOnce(VerifySyncQueryFinished(url, http_method_,
                                        /*is_url_sync_safe=*/true,
                                        /*is_url_async_safe=*/false));
  EXPECT_CALL(observer_, SafeBrowsingAsyncQueryFinished(_))
      .WillOnce(VerifyAsyncQueryFinished(url, http_method_,
                                         /*is_url_sync_safe=*/true,
                                         /*is_url_async_safe=*/false));

  // Start a URL check query for the unsafe URL and run the runloop until the
  // results are received.
  manager()->StartQuery(SafeBrowsingQueryManager::Query(url, http_method_));
  RunSyncCallbacksThenAsyncCallbacks();
}

// Tests that back-to-back queries for the same unsafe URL correctly sets an
// UnsafeResource on both queries.
TEST_P(SafeBrowsingQueryManagerTest, MultipleUnsafeURLQueries) {
  GURL url("http://" + FakeSafeBrowsingService::kUnsafeHost);

  if (base::FeatureList::IsEnabled(
          safe_browsing::kSafeBrowsingAsyncRealTimeCheck)) {
    EXPECT_CALL(observer_, SafeBrowsingSyncQueryFinished(_))
        .Times(2)
        .WillRepeatedly(VerifySyncQueryFinished(url, http_method_,
                                                /*is_url_sync_safe=*/false,
                                                /*is_url_async_safe=*/false));
    EXPECT_CALL(observer_, SafeBrowsingAsyncQueryFinished(_))
        .Times(2)
        .WillRepeatedly(VerifyAsyncQueryFinished(url, http_method_,
                                                 /*is_url_sync_safe=*/false,
                                                 /*is_url_async_safe=*/false));
  } else {
    EXPECT_CALL(observer_, SafeBrowsingQueryFinished(manager(), _, _, _))
        .Times(2)
        .WillRepeatedly(VerifyQueryFinished(url, http_method_,
                                            /*is_url_safe=*/false));
  }

  // Start a URL check query for the unsafe URL and run the runloop until the
  // result is received.
  manager()->StartQuery(SafeBrowsingQueryManager::Query(url, http_method_));
  manager()->StartQuery(SafeBrowsingQueryManager::Query(url, http_method_));
  RunSyncCallbacksThenAsyncCallbacks();
}

// Tests that StoreUnsafeResource associates the UnsafeResource with all
// queries that match the UnsafeResource's URL.
TEST_P(SafeBrowsingQueryManagerTest, StoreUnsafeResourceMultipleQueries) {
  GURL url("http://" + FakeSafeBrowsingService::kUnsafeHost);
  if (base::FeatureList::IsEnabled(
          safe_browsing::kSafeBrowsingAsyncRealTimeCheck)) {
    EXPECT_CALL(observer_, SafeBrowsingSyncQueryFinished(_))
        .Times(2)
        .WillRepeatedly(VerifySyncQueryFinished(url, http_method_,
                                                /*is_url_sync_safe=*/false,
                                                /*is_url_async_safe=*/false));
    EXPECT_CALL(observer_, SafeBrowsingAsyncQueryFinished(_))
        .Times(2)
        .WillRepeatedly(VerifyAsyncQueryFinished(url, http_method_,
                                                 /*is_url_sync_safe=*/false,
                                                 /*is_url_async_safe=*/false));
  } else {
    EXPECT_CALL(observer_, SafeBrowsingQueryFinished(manager(), _, _, _))
        .Times(2)
        .WillRepeatedly(VerifyQueryFinished(url, http_method_,
                                            /*is_url_safe=*/false));
  }

  // Start two URL check queries for the unsafe URL and run the runloop until
  // the results are received. Only call StoreUnsafeResource once, rather than
  // once for each query.
  manager()->StartQuery(SafeBrowsingQueryManager::Query(url, http_method_));
  manager()->StartQuery(SafeBrowsingQueryManager::Query(url, http_method_));
  RunSyncCallbacksThenAsyncCallbacks();
}

// Tests observer callbacks for manager destruction.
TEST_P(SafeBrowsingQueryManagerTest, ManagerDestruction) {
  web_state_ = nullptr;
  EXPECT_TRUE(observer_.manager_destroyed());
}

namespace {
// An observer that owns a WebState and destroys it when it gets a
// `SafeBrowsingQueryFinished` callback.
class WebStateDestroyingQueryManagerObserver
    : public SafeBrowsingQueryManager::Observer {
 public:
  WebStateDestroyingQueryManagerObserver()
      : browser_state_(new web::FakeBrowserState()),
        web_state_(std::make_unique<web::FakeWebState>()) {
    web_state_->SetBrowserState(browser_state_.get());
  }
  ~WebStateDestroyingQueryManagerObserver() override {}

  void SafeBrowsingQueryFinished(
      SafeBrowsingQueryManager* query_manager,
      const SafeBrowsingQueryManager::Query& query,
      const SafeBrowsingQueryManager::Result& result,
      safe_browsing::SafeBrowsingUrlCheckerImpl::PerformedCheck performed_check)
      override {
    ResetWebState();
  }

  void SafeBrowsingSyncQueryFinished(
      const SafeBrowsingQueryManager::QueryData& query_data) override {
    if (!reset_with_async_check_) {
      ResetWebState();
    }
  }

  void SafeBrowsingAsyncQueryFinished(
      const SafeBrowsingQueryManager::QueryData& query_data) override {
    ResetWebState();
  }

  void ResetWebState() {
    web_state_.reset();
    was_observer_removed_ = true;
  }

  void SafeBrowsingQueryManagerDestroyed(
      SafeBrowsingQueryManager* manager) override {
    manager->RemoveObserver(this);
  }

  void EnsureWebStateResetsWithAsyncCheck() { reset_with_async_check_ = true; }

  web::WebState* web_state() { return web_state_.get(); }

  bool was_observer_removed() { return was_observer_removed_; }

 private:
  std::unique_ptr<web::FakeBrowserState> browser_state_;
  std::unique_ptr<web::FakeWebState> web_state_;
  bool was_observer_removed_ = false;
  bool reset_with_async_check_ = false;
};
}  // namespace

// Test fixture for testing WebState destruction during a
// SafeBrowsingQueryManager::Observer callback.
class SafeBrowsingQueryManagerWebStateDestructionTest
    : public testing::TestWithParam<bool> {
 protected:
  SafeBrowsingQueryManagerWebStateDestructionTest()
      : http_method_("GET"), use_async_safe_browsing_(GetParam()) {
    scoped_feature_list_.InitWithFeatureState(
        safe_browsing::kSafeBrowsingAsyncRealTimeCheck,
        use_async_safe_browsing_);
    SafeBrowsingQueryManager::CreateForWebState(observer_.web_state(),
                                                &client_);
    SafeBrowsingUrlAllowList::CreateForWebState(observer_.web_state());
    manager()->AddObserver(&observer_);
  }

  SafeBrowsingQueryManager* manager() {
    return SafeBrowsingQueryManager::FromWebState(observer_.web_state());
  }

  // Helper function to run all sync callbacks first then async callbacks.
  void RunSyncCallbacksThenAsyncCallbacks() {
    if (base::FeatureList::IsEnabled(
            safe_browsing::kSafeBrowsingAsyncRealTimeCheck)) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(^() {
            client_.run_sync_callbacks();
          }));
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(^() {
            client_.run_async_callbacks();
          }));
    }

    // TODO(crbug.com/359420122): Remove when clean up is complete.
    base::RunLoop().RunUntilIdle();
  }

  web::WebTaskEnvironment task_environment_{
      web::WebTaskEnvironment::MainThreadType::IO};
  WebStateDestroyingQueryManagerObserver observer_;
  std::string http_method_;
  FakeSafeBrowsingClient client_;
  bool use_async_safe_browsing_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(/* No Instantiation Name */,
                         SafeBrowsingQueryManagerWebStateDestructionTest,
                         testing::Bool());

// Tests that a query for a safe URL doesn't cause a crash.
TEST_P(SafeBrowsingQueryManagerWebStateDestructionTest, SafeURLQuery) {
  GURL url("http://chromium.test");
  // Start a URL check query for the safe URL and run the runloop until the
  // result is received.
  manager()->StartQuery(SafeBrowsingQueryManager::Query(url, http_method_));
  RunSyncCallbacksThenAsyncCallbacks();
  EXPECT_TRUE(observer_.was_observer_removed());
}

// Tests that a query for an unsafe URL doesn't cause a crash.
TEST_P(SafeBrowsingQueryManagerWebStateDestructionTest, UnsafeURLQuery) {
  GURL url("http://" + FakeSafeBrowsingService::kUnsafeHost);

  // Start a URL check query for the unsafe URL and run the runloop until the
  // result is received.
  manager()->StartQuery(SafeBrowsingQueryManager::Query(url, http_method_));
  base::RunLoop().RunUntilIdle();
}
