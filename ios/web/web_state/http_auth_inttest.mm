// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "ios/web/public/test/fakes/fake_web_state_delegate.h"
#import "ios/web/public/test/js_test_util.h"
#import "ios/web/public/test/navigation_test_util.h"
#import "ios/web/public/test/web_test_with_web_state.h"
#import "net/test/embedded_test_server/default_handlers.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "testing/gtest_mac.h"
#import "url/gurl.h"

using base::test::ios::kWaitForPageLoadTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

namespace web {

// Test fixture for WebStateDelegate::OnAuthRequired integration tests.
class HttpAuthTest : public WebTestWithWebState {
 protected:
  void SetUp() override {
    WebTestWithWebState::SetUp();
    web_state()->SetDelegate(&delegate_);
    RegisterDefaultHandlers(&server_);
    ASSERT_TRUE(server_.Start());
  }
  // Waits until WebStateDelegate::OnAuthRequired callback is called.
  [[nodiscard]] bool WaitForOnAuthRequiredCallback() {
    delegate_.ClearLastAuthenticationRequest();
    return WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^bool {
      return delegate_.last_authentication_request();
    });
  }
  net::EmbeddedTestServer server_;
  FakeWebStateDelegate delegate_;
};

// Tests successful basic authentication.
TEST_F(HttpAuthTest, SuccessfullBasicAuth) {
  // Load the page which requests basic HTTP authentication.
  GURL url = server_.GetURL("/auth-basic?password=goodpass&realm=Realm1");
  test::LoadUrl(web_state(), url);
  ASSERT_TRUE(WaitForOnAuthRequiredCallback());

  // Verify that callback receives correct WebState.
  auto* auth_request = delegate_.last_authentication_request();
  EXPECT_EQ(web_state(), auth_request->web_state);

  // Verify that callback receives correctly configured protection space.
  NSURLProtectionSpace* protection_space = auth_request->protection_space;
  EXPECT_NSEQ(@"Realm1", protection_space.realm);
  EXPECT_FALSE(protection_space.receivesCredentialSecurely);
  EXPECT_FALSE([protection_space isProxy]);
  EXPECT_EQ(url.host(), base::SysNSStringToUTF8(protection_space.host));
  EXPECT_EQ(server_.port(),
            base::checked_cast<uint16_t>(protection_space.port));
  EXPECT_FALSE(protection_space.proxyType);
  EXPECT_NSEQ(NSURLProtectionSpaceHTTP, protection_space.protocol);
  EXPECT_NSEQ(NSURLAuthenticationMethodHTTPBasic,
              protection_space.authenticationMethod);

  // Make sure that authenticated page renders expected text.
  ASSERT_TRUE(web_state()->IsLoading());
  auth_request = delegate_.last_authentication_request();
  ASSERT_TRUE(auth_request);
  std::move(auth_request->auth_callback).Run(@"me", @"goodpass");
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    return web_state()->GetTitle() == u"me/goodpass";
  }));
}

// Tests unsuccessful basic authentication.
TEST_F(HttpAuthTest, UnsucessfulBasicAuth) {
  // Load the page which requests basic HTTP authentication.
  GURL url = server_.GetURL("/auth-basic?password=goodpass&realm=Realm2");
  test::LoadUrl(web_state(), url);
  ASSERT_TRUE(WaitForOnAuthRequiredCallback());

  // Make sure that incorrect credentials request authentication again.
  auto* auth_request = delegate_.last_authentication_request();
  std::move(auth_request->auth_callback).Run(@"me", @"badpass");
  ASSERT_TRUE(WaitForOnAuthRequiredCallback());

  // Verify that callback receives correct WebState.
  auth_request = delegate_.last_authentication_request();
  EXPECT_EQ(web_state(), auth_request->web_state);

  // Verify that callback receives correctly configured protection space.
  NSURLProtectionSpace* protection_space = auth_request->protection_space;
  EXPECT_NSEQ(@"Realm2", protection_space.realm);
  EXPECT_FALSE(protection_space.receivesCredentialSecurely);
  EXPECT_FALSE([protection_space isProxy]);
  EXPECT_EQ(url.host(), base::SysNSStringToUTF8(protection_space.host));
  EXPECT_EQ(server_.port(),
            base::checked_cast<uint16_t>(protection_space.port));
  EXPECT_FALSE(protection_space.proxyType);
  EXPECT_NSEQ(NSURLProtectionSpaceHTTP, protection_space.protocol);
  EXPECT_NSEQ(NSURLAuthenticationMethodHTTPBasic,
              protection_space.authenticationMethod);

  // Cancel authentication and make sure that authentication is denied.
  std::move(auth_request->auth_callback)
      .Run(/*username=*/nil, /*password=*/nil);
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    return web_state()->GetTitle() == u"Denied: Missing Authorization Header";
  }));
}

}  // web
