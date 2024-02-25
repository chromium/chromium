// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/components/security_interstitials/safe_browsing/safe_browsing_query_manager.h"

#import <Foundation/Foundation.h>

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
    EXPECT_NE(safe_browsing::SB_THREAT_TYPE_SAFE, resource.threat_type);
  }
}
}  // namespace

class SafeBrowsingQueryManagerTest : public PlatformTest {
 protected:
  SafeBrowsingQueryManagerTest()
      : task_environment_(web::WebTaskEnvironment::IO_MAINLOOP),
        browser_state_(new web::FakeBrowserState()),
        web_state_(std::make_unique<web::FakeWebState>()),
        http_method_("GET") {
    SafeBrowsingQueryManager::CreateForWebState(web_state_.get(), &client_);
    manager()->AddObserver(&observer_);
    web_state_->SetBrowserState(browser_state_.get());
  }

  SafeBrowsingQueryManager* manager() {
    return SafeBrowsingQueryManager::FromWebState(web_state_.get());
  }

  web::WebTaskEnvironment task_environment_;
  MockQueryManagerObserver observer_;
  std::unique_ptr<web::FakeBrowserState> browser_state_;
  std::unique_ptr<web::FakeWebState> web_state_;
  std::string http_method_;
  FakeSafeBrowsingClient client_;
};

// Tests a query for a safe URL.
TEST_F(SafeBrowsingQueryManagerTest, SafeURLQuery) {
  GURL url("http://chromium.test");
  EXPECT_CALL(observer_, SafeBrowsingQueryFinished(manager(), _, _, _))
      .WillOnce(VerifyQueryFinished(url, http_method_,
                                    /*is_url_safe=*/true));

  // Start a URL check query for the safe URL and run the runloop until the
  // result is received.
  manager()->StartQuery(SafeBrowsingQueryManager::Query(url, http_method_));
  base::RunLoop().RunUntilIdle();
}

// Tests a query for an unsafe URL.
TEST_F(SafeBrowsingQueryManagerTest, UnsafeURLQuery) {
  GURL url("http://" + FakeSafeBrowsingService::kUnsafeHost);
  EXPECT_CALL(observer_, SafeBrowsingQueryFinished(manager(), _, _, _))
      .WillOnce(VerifyQueryFinished(url, http_method_,
                                    /*is_url_safe=*/false));

  // Start a URL check query for the unsafe URL and run the runloop until the
  // result is received.  An UnsafeResource is stored before the query finishes
  // to simulate the production behavior that adds a resource that will be used
  // to populate the error page.
  manager()->StartQuery(SafeBrowsingQueryManager::Query(url, http_method_));
  UnsafeResource resource;
  resource.url = url;
  resource.threat_type = safe_browsing::SB_THREAT_TYPE_URL_PHISHING;
  resource.request_destination = network::mojom::RequestDestination::kDocument;
  manager()->StoreUnsafeResource(resource);
  base::RunLoop().RunUntilIdle();
}

// Tests that back-to-back queries for the same unsafe URL correctly sets an
// UnsafeResource on both queries.
TEST_F(SafeBrowsingQueryManagerTest, MultipleUnsafeURLQueries) {
  GURL url("http://" + FakeSafeBrowsingService::kUnsafeHost);
  EXPECT_CALL(observer_, SafeBrowsingQueryFinished(manager(), _, _, _))
      .Times(2)
      .WillRepeatedly(VerifyQueryFinished(url, http_method_,
                                          /*is_url_safe=*/false));

  // Start a URL check query for the unsafe URL and run the runloop until the
  // result is received.  An UnsafeResource is stored before the query finishes
  // to simulate the production behavior that adds a resource that will be used
  // to populate the error page.
  manager()->StartQuery(SafeBrowsingQueryManager::Query(url, http_method_));
  manager()->StartQuery(SafeBrowsingQueryManager::Query(url, http_method_));
  UnsafeResource resource;
  resource.url = url;
  resource.threat_type = safe_browsing::SB_THREAT_TYPE_URL_PHISHING;
  resource.request_destination = network::mojom::RequestDestination::kDocument;
  manager()->StoreUnsafeResource(resource);
  manager()->StoreUnsafeResource(resource);
  base::RunLoop().RunUntilIdle();
}

// Tests that StoreUnsafeResource associates the UnsafeResource with all
// queries that match the UnsafeResource's URL.
TEST_F(SafeBrowsingQueryManagerTest, StoreUnsafeResourceMultipleQueries) {
  GURL url("http://" + FakeSafeBrowsingService::kUnsafeHost);
  EXPECT_CALL(observer_, SafeBrowsingQueryFinished(manager(), _, _, _))
      .Times(2)
      .WillRepeatedly(VerifyQueryFinished(url, http_method_,
                                          /*is_url_safe=*/false));

  // Start two URL check queries for the unsafe URL and run the runloop until
  // the results are received. Only call StoreUnsafeResource once, rather than
  // once for each query.
  manager()->StartQuery(SafeBrowsingQueryManager::Query(url, http_method_));
  manager()->StartQuery(SafeBrowsingQueryManager::Query(url, http_method_));
  UnsafeResource resource;
  resource.url = url;
  resource.threat_type = safe_browsing::SB_THREAT_TYPE_URL_PHISHING;
  resource.request_destination = network::mojom::RequestDestination::kDocument;
  manager()->StoreUnsafeResource(resource);
  base::RunLoop().RunUntilIdle();
}

// Tests observer callbacks for manager destruction.
TEST_F(SafeBrowsingQueryManagerTest, ManagerDestruction) {
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
    web_state_.reset();
  }

  void SafeBrowsingQueryManagerDestroyed(
      SafeBrowsingQueryManager* manager) override {
    manager->RemoveObserver(this);
  }

  web::WebState* web_state() { return web_state_.get(); }

 private:
  std::unique_ptr<web::FakeBrowserState> browser_state_;
  std::unique_ptr<web::FakeWebState> web_state_;
};
}  // namespace

// Test fixture for testing WebState destruction during a
// SafeBrowsingQueryManager::Observer callback.
class SafeBrowsingQueryManagerWebStateDestructionTest : public PlatformTest {
 protected:
  SafeBrowsingQueryManagerWebStateDestructionTest()
      : task_environment_(web::WebTaskEnvironment::IO_MAINLOOP),
        http_method_("GET") {
    SafeBrowsingQueryManager::CreateForWebState(observer_.web_state(),
                                                &client_);
    manager()->AddObserver(&observer_);
  }

  SafeBrowsingQueryManager* manager() {
    return SafeBrowsingQueryManager::FromWebState(observer_.web_state());
  }

  web::WebTaskEnvironment task_environment_;
  WebStateDestroyingQueryManagerObserver observer_;
  std::string http_method_;
  FakeSafeBrowsingClient client_;
};

// Tests that a query for a safe URL doesn't cause a crash.
TEST_F(SafeBrowsingQueryManagerWebStateDestructionTest, SafeURLQuery) {
  GURL url("http://chromium.test");
  // Start a URL check query for the safe URL and run the runloop until the
  // result is received.
  manager()->StartQuery(SafeBrowsingQueryManager::Query(url, http_method_));
  base::RunLoop().RunUntilIdle();
}

// Tests that a query for an unsafe URL doesn't cause a crash.
TEST_F(SafeBrowsingQueryManagerWebStateDestructionTest, UnsafeURLQuery) {
  GURL url("http://" + FakeSafeBrowsingService::kUnsafeHost);

  // Start a URL check query for the unsafe URL and run the runloop until the
  // result is received. An UnsafeResource is stored before the query finishes
  // to simulate the production behavior that adds a resource that will be used
  // to populate the error page.
  manager()->StartQuery(SafeBrowsingQueryManager::Query(url, http_method_));
  UnsafeResource resource;
  resource.url = url;
  resource.threat_type = safe_browsing::SB_THREAT_TYPE_URL_PHISHING;
  resource.request_destination = network::mojom::RequestDestination::kDocument;
  manager()->StoreUnsafeResource(resource);
  base::RunLoop().RunUntilIdle();
}
