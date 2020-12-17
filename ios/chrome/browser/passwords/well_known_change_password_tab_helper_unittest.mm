// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#import "ios/chrome/browser/passwords/well_known_change_password_tab_helper.h"

#import <Foundation/Foundation.h>

#include "base/run_loop.h"
#import "base/test/ios/wait_util.h"
#include "components/password_manager/core/browser/well_known_change_password_util.h"
#include "components/ukm/test_ukm_recorder.h"
#include "ios/chrome/browser/passwords/ios_chrome_change_password_url_service_factory.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_client.h"
#import "ios/web/public/test/fakes/fake_web_state_delegate.h"
#import "ios/web/public/test/navigation_test_util.h"
#include "ios/web/public/test/web_task_environment.h"
#include "ios/web/public/test/web_test.h"
#include "ios/web/public/test/web_test_with_web_state.h"
#import "ios/web/public/test/web_view_content_test_util.h"
#include "net/cert/x509_certificate.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

using base::test::ios::WaitUntilConditionOrTimeout;
using net::test_server::BasicHttpResponse;
using net::test_server::EmbeddedTestServer;
using net::test_server::EmbeddedTestServerHandle;
using net::test_server::HttpRequest;
using net::test_server::HttpResponse;
using password_manager::kWellKnownChangePasswordPath;
using password_manager::kWellKnownNotExistingResourcePath;
using password_manager::WellKnownChangePasswordResult;

// ServerResponse describes how a server should respond to a given path.
struct ServerResponse {
  net::HttpStatusCode status_code;
  std::vector<std::pair<std::string, std::string>> headers;
};

constexpr char kMockChangePasswordPath[] = "/change-password-override";

// Re-implementation of web::LoadUrl() that allows specifying a custom page
// transition.
void LoadUrlWithTransition(web::WebState* web_state,
                           const GURL& url,
                           ui::PageTransition transition) {
  web::NavigationManager* navigation_manager =
      web_state->GetNavigationManager();
  web::NavigationManager::WebLoadParams params(url);
  params.transition_type = transition;
  navigation_manager->LoadURLWithParams(params);
}

}  // namespace

class TestChangePasswordUrlService
    : public password_manager::ChangePasswordUrlService {
 public:
  void PrefetchURLs() override {}

  GURL GetChangePasswordUrl(const GURL& url) override {
    if (override_available_) {
      GURL::Replacements replacement;
      replacement.SetPathStr(kMockChangePasswordPath);
      return url.ReplaceComponents(replacement);
    }
    return GURL();
  }

  void SetOverrideAvailable(bool available) { override_available_ = available; }

 private:
  bool override_available_ = false;
};

// This test uses a mockserver to simulate different response. To handle the
// url_loader requests we also mock the response for the url_loader_factory.
class WellKnownChangePasswordTabHelperTest : public web::FakeWebClient,
                                             public web::WebTestWithWebState {
 public:
  using UkmBuilder =
      ukm::builders::PasswordManager_WellKnownChangePasswordResult;
  WellKnownChangePasswordTabHelperTest() {
    test_server_->RegisterRequestHandler(base::BindRepeating(
        &WellKnownChangePasswordTabHelperTest::HandleRequest,
        base::Unretained(this)));
  }

  void SetUp() override {
    web::WebTestWithWebState::SetUp();
    EXPECT_TRUE(test_server_->InitializeAndListen());
    test_server_->StartAcceptingConnections();

    url_service_ = static_cast<TestChangePasswordUrlService*>(
        IOSChromeChangePasswordUrlServiceFactory::GetInstance()
            ->SetTestingFactoryAndUse(
                web_state()->GetBrowserState(),
                base::BindRepeating([](web::BrowserState* browser_state) {
                  return std::unique_ptr<KeyedService>(
                      std::make_unique<TestChangePasswordUrlService>());
                })));
    web_state()->SetDelegate(&delegate_);
    password_manager::WellKnownChangePasswordTabHelper::CreateForWebState(
        web_state());
    SetSharedURLLoaderFactory(
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_));
    test_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
  }

  // Sets a response for the |test_url_loader_factory_| with the |test_server_|
  // as the host.
  void SetUrlLoaderResponse(const std::string& path,
                            net::HttpStatusCode status_code) {
    test_url_loader_factory_.AddResponse(test_server_->GetURL(path).spec(), "",
                                         status_code);
  }

  void ExpectUkmMetric(WellKnownChangePasswordResult expected) {
    auto entries = test_recorder_->GetEntriesByName(UkmBuilder::kEntryName);
    // Expect one recorded metric.
    ASSERT_EQ(1, static_cast<int>(entries.size()));
    test_recorder_->ExpectEntryMetric(
        entries[0], UkmBuilder::kWellKnownChangePasswordResultName,
        static_cast<int64_t>(expected));
  }
  // Waits until the navigation is complete and waits for backgroundtasks to
  // complete. Returns false when timed out.
  bool WaitUntilLoaded();
  // Returns the url after the navigation is complete.
  GURL GetNavigatedUrl() const;

  // Maps a path to a ServerResponse config object.
  base::flat_map<std::string, ServerResponse> path_response_map_;
  std::unique_ptr<EmbeddedTestServer> test_server_ =
      std::make_unique<EmbeddedTestServer>();
  TestChangePasswordUrlService* url_service_ = nullptr;
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> test_recorder_;

 private:
  // Returns a response for the given request. Uses |path_response_map_| to
  // construct the response. Returns nullptr when the path is not defined in
  // |path_response_map_|.
  std::unique_ptr<HttpResponse> HandleRequest(const HttpRequest& request);

  network::TestURLLoaderFactory test_url_loader_factory_;
  web::FakeWebStateDelegate delegate_;
};

GURL WellKnownChangePasswordTabHelperTest::GetNavigatedUrl() const {
  web::URLVerificationTrustLevel trust_level =
      web::URLVerificationTrustLevel::kAbsolute;
  GURL url = web_state()->GetCurrentURL(&trust_level);
  // When redirecting with WebState::OpenURL() |web_state_| is not
  // updated, we only see the registered request in
  // FakeWebStateDelegate::last_open_url_request().
  if (delegate_.last_open_url_request()) {
    url = delegate_.last_open_url_request()->params.url;
  }
  return url;
}

bool WellKnownChangePasswordTabHelperTest::WaitUntilLoaded() {
  return WaitUntilConditionOrTimeout(base::test::ios::kWaitForPageLoadTimeout,
                                     ^{
                                       WaitForBackgroundTasks();
                                       return !web_state()->IsLoading();
                                     });
}

std::unique_ptr<HttpResponse>
WellKnownChangePasswordTabHelperTest::HandleRequest(
    const HttpRequest& request) {
  GURL absolute_url = test_server_->GetURL(request.relative_url);
  std::string path = absolute_url.path();
  auto it = path_response_map_.find(absolute_url.path_piece());
  if (it == path_response_map_.end())
    return nullptr;

  const ServerResponse& config = it->second;
  auto http_response = std::make_unique<BasicHttpResponse>();
  http_response->set_code(config.status_code);
  http_response->set_content_type("text/plain");
  for (auto header_pair : config.headers) {
    http_response->AddCustomHeader(header_pair.first, header_pair.second);
  }
  return http_response;
}

TEST_F(WellKnownChangePasswordTabHelperTest, SupportForChangePassword) {
  path_response_map_[kWellKnownChangePasswordPath] = {net::HTTP_OK, {}};

  SetUrlLoaderResponse(kWellKnownNotExistingResourcePath, net::HTTP_NOT_FOUND);

  web::test::LoadUrl(web_state(),
                     test_server_->GetURL(kWellKnownChangePasswordPath));
  ASSERT_TRUE(WaitUntilLoaded());
  EXPECT_EQ(GetNavigatedUrl().path(), kWellKnownChangePasswordPath);
  ExpectUkmMetric(WellKnownChangePasswordResult::kUsedWellKnownChangePassword);
}

TEST_F(WellKnownChangePasswordTabHelperTest,
       SupportForChangePassword_WithRedirect) {
  path_response_map_[kWellKnownChangePasswordPath] = {
      net::HTTP_PERMANENT_REDIRECT,
      {std::make_pair("Location", "/change-password")}};
  path_response_map_["/change-password"] = {net::HTTP_OK, {}};

  SetUrlLoaderResponse(kWellKnownNotExistingResourcePath, net::HTTP_NOT_FOUND);

  web::test::LoadUrl(web_state(),
                     test_server_->GetURL(kWellKnownChangePasswordPath));
  ASSERT_TRUE(WaitUntilLoaded());
  EXPECT_EQ(GetNavigatedUrl().path(), "/change-password");
  ExpectUkmMetric(WellKnownChangePasswordResult::kUsedWellKnownChangePassword);
}

TEST_F(WellKnownChangePasswordTabHelperTest,
       NoSupportForChangePassword_NotFound) {
  path_response_map_[kWellKnownChangePasswordPath] = {net::HTTP_NOT_FOUND, {}};
  path_response_map_["/"] = {net::HTTP_OK, {}};
  SetUrlLoaderResponse(kWellKnownNotExistingResourcePath, net::HTTP_NOT_FOUND);

  web::test::LoadUrl(web_state(),
                     test_server_->GetURL(kWellKnownChangePasswordPath));
  ASSERT_TRUE(WaitUntilLoaded());
  EXPECT_EQ(GetNavigatedUrl().path(), "/");
  ExpectUkmMetric(WellKnownChangePasswordResult::kFallbackToOriginUrl);
}

TEST_F(WellKnownChangePasswordTabHelperTest, NoSupportForChangePassword_Ok) {
  path_response_map_[kWellKnownChangePasswordPath] = {net::HTTP_OK, {}};
  path_response_map_["/"] = {net::HTTP_OK, {}};
  SetUrlLoaderResponse(kWellKnownNotExistingResourcePath, net::HTTP_OK);

  web::test::LoadUrl(web_state(),
                     test_server_->GetURL(kWellKnownChangePasswordPath));
  ASSERT_TRUE(WaitUntilLoaded());
  EXPECT_EQ(GetNavigatedUrl().path(), "/");
  ExpectUkmMetric(WellKnownChangePasswordResult::kFallbackToOriginUrl);
}

TEST_F(WellKnownChangePasswordTabHelperTest,
       NoSupportForChangePassword_WithRedirect) {
  path_response_map_[kWellKnownChangePasswordPath] = {
      net::HTTP_PERMANENT_REDIRECT, {std::make_pair("Location", "/not-found")}};
  path_response_map_["/not-found"] = {net::HTTP_NOT_FOUND, {}};
  SetUrlLoaderResponse(kWellKnownNotExistingResourcePath, net::HTTP_OK);
  web::test::LoadUrl(web_state(),
                     test_server_->GetURL(kWellKnownChangePasswordPath));
  ASSERT_TRUE(WaitUntilLoaded());
  EXPECT_EQ(GetNavigatedUrl().path(), "/");
  ExpectUkmMetric(WellKnownChangePasswordResult::kFallbackToOriginUrl);
}

TEST_F(WellKnownChangePasswordTabHelperTest,
       NoSupportForChangePassword_WithOverride) {
  url_service_->SetOverrideAvailable(true);
  path_response_map_[kWellKnownChangePasswordPath] = {
      net::HTTP_PERMANENT_REDIRECT, {std::make_pair("Location", "/not-found")}};
  path_response_map_["/not-found"] = {net::HTTP_NOT_FOUND, {}};
  SetUrlLoaderResponse(kWellKnownNotExistingResourcePath, net::HTTP_OK);
  web::test::LoadUrl(web_state(),
                     test_server_->GetURL(kWellKnownChangePasswordPath));
  ASSERT_TRUE(WaitUntilLoaded());
  EXPECT_EQ(GetNavigatedUrl().path(), kMockChangePasswordPath);
  ExpectUkmMetric(WellKnownChangePasswordResult::kFallbackToOverrideUrl);
}

TEST_F(WellKnownChangePasswordTabHelperTest,
       NoSupportForChangePasswordForLinks) {
  path_response_map_[kWellKnownChangePasswordPath] = {net::HTTP_OK, {}};
  LoadUrlWithTransition(web_state(),
                        test_server_->GetURL(kWellKnownChangePasswordPath),
                        ui::PAGE_TRANSITION_LINK);
  ASSERT_TRUE(WaitUntilLoaded());
  EXPECT_EQ(GetNavigatedUrl().path(), kWellKnownChangePasswordPath);

  // In the case of PAGE_TRANSITION_LINK the tab helper should not be active and
  // no metrics should be recorded.
  EXPECT_TRUE(test_recorder_->GetEntriesByName(UkmBuilder::kEntryName).empty());
}
