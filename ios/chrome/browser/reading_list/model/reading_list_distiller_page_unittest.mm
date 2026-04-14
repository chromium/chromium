// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reading_list/model/reading_list_distiller_page.h"

#import "base/test/task_environment.h"
#import "components/favicon/core/test/mock_favicon_service.h"
#import "components/favicon/ios/web_favicon_driver.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/security/ssl_status.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "net/cert/x509_certificate.h"
#import "net/cert/x509_util.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

namespace reading_list {

namespace {

class DummyDelegate : public ReadingListDistillerPageDelegate {
 public:
  void DistilledPageRedirectedToURL(const GURL& original_url,
                                    const GURL& final_url) override {}
  void DistilledPageHasMimeType(const GURL& original_url,
                                const std::string& mime_type) override {}
};

scoped_refptr<net::X509Certificate> CreateDummyCert() {
  std::vector<uint8_t> cert_der =
      net::x509_util::CreateUnusableCert("CN=Error");
  return net::X509Certificate::CreateFromBytes(cert_der);
}

}  // namespace

class ReadingListDistillerPageTest : public PlatformTest {
 protected:
  ReadingListDistillerPageTest()
      : delegate_(), profile_(TestProfileIOS::Builder().Build()) {}

  base::test::TaskEnvironment task_environment_;
  DummyDelegate delegate_;
  std::unique_ptr<TestProfileIOS> profile_;
};

TEST_F(ReadingListDistillerPageTest, IsGoogleCachedAMPPage) {
  ReadingListDistillerPage distiller_page(GURL(), profile_.get(), nullptr,
                                          &delegate_);

  scoped_refptr<net::X509Certificate> cert = CreateDummyCert();

  // Case 1: Valid Google AMP URL
  {
    auto fake_web_state = std::make_unique<web::FakeWebState>();
    auto fake_navigation_manager =
        std::make_unique<web::FakeNavigationManager>();

    GURL url("https://www.google.com/amp/s/example.com");
    fake_navigation_manager->AddItem(url, ui::PAGE_TRANSITION_TYPED);
    web::NavigationItem* item = fake_navigation_manager->GetItemAtIndex(0);
    fake_navigation_manager->SetLastCommittedItem(item);

    web::SSLStatus& ssl_status = item->GetSSL();
    ssl_status.certificate = cert;
    ssl_status.cert_status = 0;

    fake_web_state->SetNavigationManager(std::move(fake_navigation_manager));
    fake_web_state->SetCurrentURL(url);

    distiller_page.AttachWebState(std::move(fake_web_state));
    EXPECT_TRUE(distiller_page.IsGoogleCachedAMPPage());
    distiller_page.DetachWebState();
  }

  // Case 2: Valid Google Non-AMP URL
  {
    auto fake_web_state = std::make_unique<web::FakeWebState>();
    auto fake_navigation_manager =
        std::make_unique<web::FakeNavigationManager>();

    GURL url("https://www.google.com/search");
    fake_navigation_manager->AddItem(url, ui::PAGE_TRANSITION_TYPED);
    web::NavigationItem* item = fake_navigation_manager->GetItemAtIndex(0);
    fake_navigation_manager->SetLastCommittedItem(item);

    web::SSLStatus& ssl_status = item->GetSSL();
    ssl_status.certificate = cert;
    ssl_status.cert_status = 0;

    fake_web_state->SetNavigationManager(std::move(fake_navigation_manager));
    fake_web_state->SetCurrentURL(url);

    distiller_page.AttachWebState(std::move(fake_web_state));
    EXPECT_FALSE(distiller_page.IsGoogleCachedAMPPage());
    distiller_page.DetachWebState();
  }

  // Case 3: Non-Google URL with /amp/
  {
    auto fake_web_state = std::make_unique<web::FakeWebState>();
    auto fake_navigation_manager =
        std::make_unique<web::FakeNavigationManager>();

    GURL url("https://example.com/amp/s/example.com");
    fake_navigation_manager->AddItem(url, ui::PAGE_TRANSITION_TYPED);
    web::NavigationItem* item = fake_navigation_manager->GetItemAtIndex(0);
    fake_navigation_manager->SetLastCommittedItem(item);

    web::SSLStatus& ssl_status = item->GetSSL();
    ssl_status.certificate = cert;
    ssl_status.cert_status = 0;

    fake_web_state->SetNavigationManager(std::move(fake_navigation_manager));
    fake_web_state->SetCurrentURL(url);

    distiller_page.AttachWebState(std::move(fake_web_state));
    EXPECT_FALSE(distiller_page.IsGoogleCachedAMPPage());
    distiller_page.DetachWebState();
  }

  // Case 4: HTTP Google AMP URL
  {
    auto fake_web_state = std::make_unique<web::FakeWebState>();
    auto fake_navigation_manager =
        std::make_unique<web::FakeNavigationManager>();

    GURL url("http://www.google.com/amp/s/example.com");
    fake_navigation_manager->AddItem(url, ui::PAGE_TRANSITION_TYPED);
    web::NavigationItem* item = fake_navigation_manager->GetItemAtIndex(0);
    fake_navigation_manager->SetLastCommittedItem(item);

    web::SSLStatus& ssl_status = item->GetSSL();
    ssl_status.certificate = cert;
    ssl_status.cert_status = 0;

    fake_web_state->SetNavigationManager(std::move(fake_navigation_manager));
    fake_web_state->SetCurrentURL(url);

    distiller_page.AttachWebState(std::move(fake_web_state));
    EXPECT_FALSE(distiller_page.IsGoogleCachedAMPPage());
    distiller_page.DetachWebState();
  }

  // Case 5: Google AMP URL with invalid cert (null)
  {
    auto fake_web_state = std::make_unique<web::FakeWebState>();
    auto fake_navigation_manager =
        std::make_unique<web::FakeNavigationManager>();

    GURL url("https://www.google.com/amp/s/example.com");
    fake_navigation_manager->AddItem(url, ui::PAGE_TRANSITION_TYPED);
    web::NavigationItem* item = fake_navigation_manager->GetItemAtIndex(0);
    fake_navigation_manager->SetLastCommittedItem(item);

    web::SSLStatus& ssl_status = item->GetSSL();
    ssl_status.certificate = nullptr;
    ssl_status.cert_status = 0;

    fake_web_state->SetNavigationManager(std::move(fake_navigation_manager));
    fake_web_state->SetCurrentURL(url);

    distiller_page.AttachWebState(std::move(fake_web_state));
    EXPECT_FALSE(distiller_page.IsGoogleCachedAMPPage());
    distiller_page.DetachWebState();
  }

  // Case 6: Google AMP URL with cert error
  {
    auto fake_web_state = std::make_unique<web::FakeWebState>();
    auto fake_navigation_manager =
        std::make_unique<web::FakeNavigationManager>();

    GURL url("https://www.google.com/amp/s/example.com");
    fake_navigation_manager->AddItem(url, ui::PAGE_TRANSITION_TYPED);
    web::NavigationItem* item = fake_navigation_manager->GetItemAtIndex(0);
    fake_navigation_manager->SetLastCommittedItem(item);

    web::SSLStatus& ssl_status = item->GetSSL();
    ssl_status.certificate = cert;
    ssl_status.cert_status = net::CERT_STATUS_DATE_INVALID;

    fake_web_state->SetNavigationManager(std::move(fake_navigation_manager));
    fake_web_state->SetCurrentURL(url);

    distiller_page.AttachWebState(std::move(fake_web_state));
    EXPECT_FALSE(distiller_page.IsGoogleCachedAMPPage());
    distiller_page.DetachWebState();
  }
}

TEST_F(ReadingListDistillerPageTest, OnHandleGoogleCachedAMPPageResult) {
  ReadingListDistillerPage distiller_page(GURL(), profile_.get(), nullptr,
                                          &delegate_);

  favicon::MockFaviconService mock_favicon_service;

  // Case 1: Valid HTTPS URL
  {
    auto fake_web_state = std::make_unique<web::FakeWebState>();
    fake_web_state->SetBrowserState(profile_.get());
    auto fake_navigation_manager =
        std::make_unique<web::FakeNavigationManager>();
    auto* raw_nav_manager = fake_navigation_manager.get();
    fake_web_state->SetNavigationManager(std::move(fake_navigation_manager));

    favicon::WebFaviconDriver::CreateForWebState(fake_web_state.get(),
                                                 &mock_favicon_service);

    distiller_page.AttachWebState(std::move(fake_web_state));

    base::Value value("https://example.com");
    distiller_page.OnHandleGoogleCachedAMPPageResult(&value, nil);

    EXPECT_TRUE(raw_nav_manager->LoadURLWithParamsWasCalled());
    auto params = raw_nav_manager->GetLastLoadURLWithParams();
    EXPECT_TRUE(params.has_value());
    EXPECT_EQ(params->url, GURL("https://example.com"));

    distiller_page.DetachWebState();
  }

  // Case 2: Valid HTTP URL
  {
    auto fake_web_state = std::make_unique<web::FakeWebState>();
    fake_web_state->SetBrowserState(profile_.get());
    auto fake_navigation_manager =
        std::make_unique<web::FakeNavigationManager>();
    auto* raw_nav_manager = fake_navigation_manager.get();
    fake_web_state->SetNavigationManager(std::move(fake_navigation_manager));

    favicon::WebFaviconDriver::CreateForWebState(fake_web_state.get(),
                                                 &mock_favicon_service);

    distiller_page.AttachWebState(std::move(fake_web_state));

    base::Value value("http://example.com");
    distiller_page.OnHandleGoogleCachedAMPPageResult(&value, nil);

    EXPECT_TRUE(raw_nav_manager->LoadURLWithParamsWasCalled());
    auto params = raw_nav_manager->GetLastLoadURLWithParams();
    EXPECT_TRUE(params.has_value());
    EXPECT_EQ(params->url, GURL("http://example.com"));

    distiller_page.DetachWebState();
  }

  // Case 3: Invalid URL (file://)
  {
    auto fake_web_state = std::make_unique<web::FakeWebState>();
    fake_web_state->SetBrowserState(profile_.get());
    auto fake_navigation_manager =
        std::make_unique<web::FakeNavigationManager>();
    auto* raw_nav_manager = fake_navigation_manager.get();
    fake_web_state->SetNavigationManager(std::move(fake_navigation_manager));

    favicon::WebFaviconDriver::CreateForWebState(fake_web_state.get(),
                                                 &mock_favicon_service);

    distiller_page.AttachWebState(std::move(fake_web_state));

    base::Value value("file:///etc/passwd");
    distiller_page.OnHandleGoogleCachedAMPPageResult(&value, nil);

    EXPECT_FALSE(raw_nav_manager->LoadURLWithParamsWasCalled());

    distiller_page.DetachWebState();
  }

  // Case 4: Invalid string
  {
    auto fake_web_state = std::make_unique<web::FakeWebState>();
    fake_web_state->SetBrowserState(profile_.get());
    auto fake_navigation_manager =
        std::make_unique<web::FakeNavigationManager>();
    auto* raw_nav_manager = fake_navigation_manager.get();
    fake_web_state->SetNavigationManager(std::move(fake_navigation_manager));

    favicon::WebFaviconDriver::CreateForWebState(fake_web_state.get(),
                                                 &mock_favicon_service);

    distiller_page.AttachWebState(std::move(fake_web_state));

    base::Value value("not a url");
    distiller_page.OnHandleGoogleCachedAMPPageResult(&value, nil);

    EXPECT_FALSE(raw_nav_manager->LoadURLWithParamsWasCalled());

    distiller_page.DetachWebState();
  }

  // Case 5: Error passed
  {
    auto fake_web_state = std::make_unique<web::FakeWebState>();
    fake_web_state->SetBrowserState(profile_.get());
    auto fake_navigation_manager =
        std::make_unique<web::FakeNavigationManager>();
    auto* raw_nav_manager = fake_navigation_manager.get();
    fake_web_state->SetNavigationManager(std::move(fake_navigation_manager));

    favicon::WebFaviconDriver::CreateForWebState(fake_web_state.get(),
                                                 &mock_favicon_service);

    distiller_page.AttachWebState(std::move(fake_web_state));

    base::Value value("https://example.com");
    NSError* error = [NSError errorWithDomain:@"test" code:1 userInfo:nil];
    distiller_page.OnHandleGoogleCachedAMPPageResult(&value, error);

    EXPECT_FALSE(raw_nav_manager->LoadURLWithParamsWasCalled());

    distiller_page.DetachWebState();
  }
}

}  // namespace reading_list
