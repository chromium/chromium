// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/omnibox_test_util.h"

#import "base/containers/contains.h"
#import "base/strings/string_number_conversions.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_configuration.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "net/test/embedded_test_server/http_request.h"
#import "net/test/embedded_test_server/http_response.h"
#import "url/gurl.h"

namespace {

/// URL scheme used for test pages.
const std::string kPageURLScheme = "/pageXX.html";

}  // namespace

namespace omnibox {

#pragma mark - Custom pages

// Web page to try X-Client-Data header.
const char kHeaderPageURL[] = "/page0.html";
const char kHeaderPageSuccess[] = "header found!";
const char kHeaderPageFailure[] = "header failure";

#pragma mark - Pages

/// Returns the content of `page`.
std::string PageContent(Page page) {
  return "This is page " + base::NumberToString(page);
}

/// Returns the title of `page`.
std::string PageTitle(Page page) {
  return "Title " + base::NumberToString(page);
}

/// Returns the URL of `page`.
std::string PageURL(Page page) {
  // Construct an URL conforming to `kPageURLScheme`.
  NSString* nsPageURL = [NSString stringWithFormat:@"/page%02lu.html", page];
  std::string pageURL = base::SysNSStringToUTF8(nsPageURL);
  return pageURL;
}

#pragma mark - Utils

void DisableAutocompleteProviders(AppLaunchConfiguration& configuration,
                                  int provider_types) {
  // Use string literals here as including the headers pulls too many
  // dependencies.
  configuration.additional_args.push_back(
      "--force-fieldtrials=" + std::string("OmniboxBundledExperimentV1") +
      "/Test");
  configuration.additional_args.push_back(
      "--force-fieldtrial-params=" + std::string("OmniboxBundledExperimentV1") +
      ".Test:" + std::string("DisableProviders") + "/" +
      base::NumberToString(provider_types));
}

std::unique_ptr<net::test_server::HttpResponse> OmniboxHTTPResponses(
    const net::test_server::HttpRequest& request) {
  std::unique_ptr<net::test_server::BasicHttpResponse> http_response =
      std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_OK);

  std::string relative_url = request.relative_url;
  if (relative_url == kHeaderPageURL) {
    std::string result = kHeaderPageFailure;
    if (base::Contains(request.headers, "X-Client-Data")) {
      result = kHeaderPageSuccess;
    }
    http_response->set_content("<html><body>" + result + "</body></html>");
    return std::move(http_response);
  }

  if (relative_url.size() != kPageURLScheme.size()) {
    return nil;
  }
  // Retrieve the page number, assuming the relative_url conforms to
  // `kPageURLScheme`.
  std::string page_number_str = relative_url.substr(5, 2);
  // Replace the page number with `XX` and compare to `kPageURLScheme`.
  relative_url.replace(5, 2, "XX");
  if (relative_url != kPageURLScheme) {
    return nil;
  }
  int page_number = stoi(page_number_str);
  http_response->set_content("<html><head><title>" + PageTitle(page_number) +
                             "</title></head><body>" +
                             PageContent(page_number) + "</body></html>");
  return std::move(http_response);
}

}  // namespace omnibox
