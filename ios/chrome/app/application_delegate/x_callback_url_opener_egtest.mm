// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/authentication/test/signin_earl_grey.h"
#import "ios/chrome/browser/authentication/test/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/search_engines/model/search_engines_app_interface.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/earl_grey/test_switches.h"
#import "ios/testing/earl_grey/app_launch_configuration.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/base/url_util.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "net/test/embedded_test_server/http_request.h"
#import "net/test/embedded_test_server/http_response.h"

namespace {

const char kManagedIdentityGaiaID[] = "foo_google.com_GAIAID";
const char kPersonalIdentityGaiaID[] = "foo_gmail.com_GAIAID";

const char kSearchResultHtmlTemplate[] =
    "<html>"
    "  <head>"
    "    <meta name='viewport' content='width=device-width, "
    "      initial-scale=1.0, maximum-scale=1.0, user-scalable=no' "
    "    />"
    "  </head>"
    "  <body>"
    "    Search Result <br/><br/>"
    "    SEARCH_QUERY"
    "  </body>"
    "</html>";
const char kSearchResultUrl[] = "/search";

std::unique_ptr<net::test_server::HttpResponse> StandardResponse(
    const net::test_server::HttpRequest& request) {
  std::unique_ptr<net::test_server::BasicHttpResponse> http_response =
      std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_OK);
  GURL request_url = request.GetURL();

  if (request_url.path() == kSearchResultUrl) {
    std::string html = kSearchResultHtmlTemplate;
    std::string query;
    bool has_query = net::GetValueForKeyInQuery(request_url, "q", &query);
    if (has_query) {
      base::ReplaceFirstSubstringAfterOffset(&html, 0, "SEARCH_QUERY", query);
    }
    http_response->set_content(html);
  } else {
    return nullptr;
  }

  return std::move(http_response);
}

}  // namespace

// Tests the Chrome opening when triggered for a `x-callback-url`, mimiching the
// behavior of the share extension.
@interface XCallbackURLEGTest : ChromeTestCase

@end

@implementation XCallbackURLEGTest

- (void)addIdentityAndSetDSE:(FakeSystemIdentity*)identity
                     managed:(BOOL)managed {
  [SigninEarlGrey addFakeIdentity:identity];
  if (managed) {
    [SigninEarlGrey signinWithFakeManagedIdentityInPersonalProfile:identity];
  } else {
    [SigninEarlGrey signinWithFakeIdentity:identity
                waitForSyncTransportActive:NO];
  }

  GURL searchUrl = self.testServer->GetURL(kSearchResultUrl);
  [SearchEnginesAppInterface
      addSearchEngineWithName:@"test"
                          URL:base::SysUTF8ToNSString(searchUrl.spec() +
                                                      "?q={searchTerms}")
                   setDefault:YES];
}

- (void)setUp {
  [super setUp];
  self.testServer->RegisterRequestHandler(
      base::BindRepeating(&StandardResponse));
  GREYAssertTrue(self.testServer->Start(), @"Server did not start.");
}

- (void)testTextSearchWithoutProfileSwitch {
  FakeSystemIdentity* const personalIdentity =
      [FakeSystemIdentity fakeIdentity1];

  // Signin to the personal identity and add a test DSE to its profile.
  [self addIdentityAndSetDSE:personalIdentity managed:NO];

  // Add a text search command and open chrome with a callback URL with the
  // personal gaia id.
  [ChromeEarlGrey setAppGroupCommandToSearchText:@"some text"];
  std::string url_string =
      "chromium-dev://x-callback-url/app-group-command?gaia_id=";
  url_string += kPersonalIdentityGaiaID;
  GURL gurl(url_string);
  [ChromeEarlGrey sceneOpenURL:gurl];

  [ChromeEarlGrey waitForWebStateContainingText:"Search Result"];
  [ChromeEarlGrey waitForWebStateContainingText:"some text"];

  [SigninEarlGrey verifySignedInWithFakeIdentity:personalIdentity];
}

- (void)testIncognitoTextSearchWithoutProfileSwitch {
  FakeSystemIdentity* const personalIdentity =
      [FakeSystemIdentity fakeIdentity1];

  // Signin to the personal identity and add a test DSE to its profile.
  [self addIdentityAndSetDSE:personalIdentity managed:NO];

  // Add a text search command and open chrome with a callback URL with the
  // personal gaia id.
  [ChromeEarlGrey setAppGroupCommandToIncognitoSearchText:@"some text"];
  std::string url_string =
      "chromium-dev://x-callback-url/app-group-command?gaia_id=";
  url_string += kPersonalIdentityGaiaID;
  GURL gurl(url_string);
  [ChromeEarlGrey sceneOpenURL:gurl];

  [ChromeEarlGrey waitForWebStateContainingText:"Search Result"];
  [ChromeEarlGrey waitForWebStateContainingText:"some text"];
  GREYAssertTrue([ChromeEarlGrey isIncognitoMode],
                 @"Failed to switch to incognito mode");

  [SigninEarlGrey verifySignedInWithFakeIdentity:personalIdentity];
}

- (void)testTextSearchWithProfileSwitch {
  FakeSystemIdentity* const personalIdentity =
      [FakeSystemIdentity fakeIdentity1];
  FakeSystemIdentity* const managedIdentity =
      [FakeSystemIdentity fakeManagedIdentity];

  // Signin to the managed identity and add a test DSE to its profile.
  [self addIdentityAndSetDSE:managedIdentity managed:YES];

  // Signin to the personal identity and add a test DSE to its profile.
  [self addIdentityAndSetDSE:personalIdentity managed:NO];
  [ChromeEarlGreyUI waitForAppToIdle];

  // Check that the personal identity is signed in.
  [SigninEarlGrey verifySignedInWithFakeIdentity:personalIdentity];

  // Add a text search command and open chrome with a callback URL with the
  // managed gaia id.
  [ChromeEarlGrey setAppGroupCommandToSearchText:@"some text"];
  std::string url_string =
      "chromium-dev://x-callback-url/app-group-command?gaia_id=";
  url_string += kManagedIdentityGaiaID;
  GURL gurl(url_string);
  [ChromeEarlGrey sceneOpenURL:gurl];

  [ChromeEarlGrey waitForWebStateContainingText:"Search Result"];
  [ChromeEarlGrey waitForWebStateContainingText:"some text"];

  [SigninEarlGrey verifySignedInWithFakeIdentity:managedIdentity];
}

- (void)testIncognitoTextSearchWithProfileSwitch {
  FakeSystemIdentity* const personalIdentity =
      [FakeSystemIdentity fakeIdentity1];
  FakeSystemIdentity* const managedIdentity =
      [FakeSystemIdentity fakeManagedIdentity];

  // Signin to the managed identity and add a test DSE to its profile.
  [self addIdentityAndSetDSE:managedIdentity managed:YES];

  // Signin to the personal identity and add a test DSE to its profile.
  [self addIdentityAndSetDSE:personalIdentity managed:NO];
  [ChromeEarlGreyUI waitForAppToIdle];

  // Check that the personal identity is signed in.
  [SigninEarlGrey verifySignedInWithFakeIdentity:personalIdentity];

  // Add a text search command and open chrome with a callback URL with the
  // managed gaia id.
  [ChromeEarlGrey setAppGroupCommandToIncognitoSearchText:@"some text"];
  std::string url_string =
      "chromium-dev://x-callback-url/app-group-command?gaia_id=";
  url_string += kManagedIdentityGaiaID;
  GURL gurl(url_string);
  [ChromeEarlGrey sceneOpenURL:gurl];

  [ChromeEarlGrey waitForWebStateContainingText:"Search Result"];
  [ChromeEarlGrey waitForWebStateContainingText:"some text"];
  GREYAssertTrue([ChromeEarlGrey isIncognitoMode],
                 @"Failed to switch to incognito mode");

  [SigninEarlGrey verifySignedInWithFakeIdentity:managedIdentity];
}

@end
