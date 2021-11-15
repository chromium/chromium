// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/cwv_legacy_tls_warning_handler_internal.h"

#include <memory>

#import "ios/components/security_interstitials/legacy_tls/legacy_tls_tab_allow_list.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "net/base/mac/url_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios_web_view {

using CWVLegacyTLSWarningHandlerTest = PlatformTest;

TEST_F(CWVLegacyTLSWarningHandlerTest, Initialization) {
  web::FakeWebState web_state;
  NSURL* URL = [NSURL URLWithString:@"https://www.chromium.org"];
  NSDictionary* user_info =
      @{NSLocalizedDescriptionKey : @"This is an error description."};
  NSError* error = [NSError errorWithDomain:@"TestDomain"
                                       code:-1
                                   userInfo:user_info];
  CWVLegacyTLSWarningHandler* legacy_tls_warning_handler =
      [[CWVLegacyTLSWarningHandler alloc] initWithWebState:&web_state
                                                       URL:URL
                                                     error:error
                                   warningPageHTMLCallback:^(NSString* HTML){
                                       // No op.
                                   }];
  EXPECT_NSEQ(URL, legacy_tls_warning_handler.URL);
  EXPECT_NSEQ(error, legacy_tls_warning_handler.error);
}

TEST_F(CWVLegacyTLSWarningHandlerTest, DisplayHTML) {
  web::FakeWebState web_state;
  NSURL* URL = [NSURL URLWithString:@"https://www.chromium.org"];
  NSError* error = [NSError errorWithDomain:@"TestDomain" code:-1 userInfo:nil];
  __block NSString* displayed_html = nil;
  CWVLegacyTLSWarningHandler* legacy_tls_warning_handler =
      [[CWVLegacyTLSWarningHandler alloc] initWithWebState:&web_state
                                                       URL:URL
                                                     error:error
                                   warningPageHTMLCallback:^(NSString* HTML) {
                                     displayed_html = HTML;
                                   }];

  [legacy_tls_warning_handler
      displayWarningPageWithHTML:@"This is a test warning page."];
  EXPECT_NSEQ(@"This is a test warning page.", displayed_html);
}

TEST_F(CWVLegacyTLSWarningHandlerTest, OverrideAndReload) {
  web::FakeWebState web_state;
  auto navigation_manager = std::make_unique<web::FakeNavigationManager>();
  web::FakeNavigationManager* navigation_manager_ptr = navigation_manager.get();
  web_state.SetNavigationManager(std::move(navigation_manager));
  NSURL* URL = [NSURL URLWithString:@"https://www.chromium.org"];
  NSError* error = [NSError errorWithDomain:@"TestDomain" code:-1 userInfo:nil];
  CWVLegacyTLSWarningHandler* legacy_tls_warning_handler =
      [[CWVLegacyTLSWarningHandler alloc] initWithWebState:&web_state
                                                       URL:URL
                                                     error:error
                                   warningPageHTMLCallback:^(NSString* HTML){
                                       // No op.
                                   }];

  LegacyTLSTabAllowList* allow_list =
      LegacyTLSTabAllowList::FromWebState(&web_state);
  std::string host = net::GURLWithNSURL(URL).host();
  EXPECT_FALSE(allow_list->IsDomainAllowed(host));
  [legacy_tls_warning_handler overrideWarningAndReloadPage];
  EXPECT_TRUE(allow_list->IsDomainAllowed(host));
  EXPECT_TRUE(navigation_manager_ptr->ReloadWasCalled());
}

}  // namespace ios_web_view
