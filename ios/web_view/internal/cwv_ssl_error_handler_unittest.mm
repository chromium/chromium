// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/cwv_ssl_error_handler_internal.h"

#include <memory>

#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#include "net/cert/cert_status_flags.h"
#include "net/ssl/ssl_info.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"

namespace ios_web_view {

class CWVSSLErrorHandlerTest : public PlatformTest {
 public:
  CWVSSLErrorHandlerTest(const CWVSSLErrorHandlerTest&) = delete;
  CWVSSLErrorHandlerTest& operator=(const CWVSSLErrorHandlerTest&) = delete;

 protected:
  CWVSSLErrorHandlerTest() {}
};

TEST_F(CWVSSLErrorHandlerTest, Initialization) {
  web::FakeWebState web_state;
  NSURL* URL = [NSURL URLWithString:@"https://www.chromium.org"];
  NSDictionary* user_info =
      @{NSLocalizedDescriptionKey : @"This is an error description."};
  NSError* error = [NSError errorWithDomain:@"TestDomain"
                                       code:-1
                                   userInfo:user_info];
  net::SSLInfo ssl_info;
  ssl_info.is_fatal_cert_error = true;
  ssl_info.cert_status = net::CERT_STATUS_REVOKED;
  CWVSSLErrorHandler* ssl_error_handler =
      [[CWVSSLErrorHandler alloc] initWithWebState:&web_state
                                               URL:URL
                                             error:error
                                           SSLInfo:ssl_info
                             errorPageHTMLCallback:^(NSString* HTML){
                                 // No op.
                             }];
  EXPECT_NSEQ(URL, ssl_error_handler.URL);
  EXPECT_NSEQ(error, ssl_error_handler.error);
  EXPECT_TRUE(ssl_error_handler.overridable);
  EXPECT_EQ(CWVCertStatusRevoked, ssl_error_handler.certStatus);
}

TEST_F(CWVSSLErrorHandlerTest, DisplayHTML) {
  web::FakeWebState web_state;
  NSURL* URL = [NSURL URLWithString:@"https://www.chromium.org"];
  NSError* error = [NSError errorWithDomain:@"TestDomain" code:-1 userInfo:nil];
  net::SSLInfo ssl_info;
  __block NSString* displayed_html = nil;
  CWVSSLErrorHandler* ssl_error_handler =
      [[CWVSSLErrorHandler alloc] initWithWebState:&web_state
                                               URL:URL
                                             error:error
                                           SSLInfo:ssl_info
                             errorPageHTMLCallback:^(NSString* HTML) {
                               displayed_html = HTML;
                             }];

  [ssl_error_handler displayErrorPageWithHTML:@"This is a test error page."];
  EXPECT_NSEQ(@"This is a test error page.", displayed_html);
}

TEST_F(CWVSSLErrorHandlerTest, CanOverrideAndReload) {
  web::FakeWebState web_state;
  auto navigation_manager = std::make_unique<web::FakeNavigationManager>();
  web::FakeNavigationManager* navigation_manager_ptr = navigation_manager.get();
  web_state.SetNavigationManager(std::move(navigation_manager));
  NSURL* URL = [NSURL URLWithString:@"https://www.chromium.org"];
  NSError* error = [NSError errorWithDomain:@"TestDomain" code:-1 userInfo:nil];
  net::SSLInfo ssl_info;
  ssl_info.is_fatal_cert_error = true;
  ssl_info.cert_status = net::CERT_STATUS_REVOKED;
  CWVSSLErrorHandler* ssl_error_handler =
      [[CWVSSLErrorHandler alloc] initWithWebState:&web_state
                                               URL:URL
                                             error:error
                                           SSLInfo:ssl_info
                             errorPageHTMLCallback:^(NSString* HTML){
                                 // No op.
                             }];

  EXPECT_TRUE(ssl_error_handler.overridable);
  [ssl_error_handler overrideErrorAndReloadPage];
  EXPECT_TRUE(navigation_manager_ptr->ReloadWasCalled());
}

TEST_F(CWVSSLErrorHandlerTest, CannotOverrideAndReload) {
  web::FakeWebState web_state;
  auto navigation_manager = std::make_unique<web::FakeNavigationManager>();
  web::FakeNavigationManager* navigation_manager_ptr = navigation_manager.get();
  web_state.SetNavigationManager(std::move(navigation_manager));
  NSURL* URL = [NSURL URLWithString:@"https://www.chromium.org"];
  NSError* error = [NSError errorWithDomain:@"TestDomain" code:-1 userInfo:nil];
  net::SSLInfo ssl_info;
  ssl_info.is_fatal_cert_error = false;
  ssl_info.cert_status = net::CERT_STATUS_REVOKED;
  CWVSSLErrorHandler* ssl_error_handler =
      [[CWVSSLErrorHandler alloc] initWithWebState:&web_state
                                               URL:URL
                                             error:error
                                           SSLInfo:ssl_info
                             errorPageHTMLCallback:^(NSString* HTML){
                                 // No op.
                             }];

  EXPECT_FALSE(ssl_error_handler.overridable);
  [ssl_error_handler overrideErrorAndReloadPage];
  EXPECT_FALSE(navigation_manager_ptr->ReloadWasCalled());
}

}  // namespace ios_web_view
