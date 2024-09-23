// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_TEST_UTIL_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_TEST_UTIL_H_

#import <memory>

struct AppLaunchConfiguration;
namespace net {
namespace test_server {
class HttpResponse;
struct HttpRequest;
}  // namespace test_server
}  // namespace net
@protocol GREYMatcher;
class GURL;

namespace omnibox {

#pragma mark Custom pages

// Web page to try X-Client-Data header.
extern const char kHeaderPageURL[];
extern const char kHeaderPageSuccess[];
extern const char kHeaderPageFailure[];

#pragma mark Page

/// Page number are limited to [1, 99].
using Page = unsigned long;  // NSUInteger.

/// Returns the content of `page`.
std::string PageContent(Page page);

/// Returns the title of `page`.
std::string PageTitle(Page page);

/// Returns the URL of `page`.
std::string PageURL(Page page);

#pragma mark Utils

/// Disables autocomplete providers of `provider_types` by forcing a fieldtrial
/// param in `configuration`.
/// `provider_types`:  Bitwise operation of `AutocompleteProvider::Type`.
///
/// Note: Don't include autocomplete_provider header as it pulls to many
/// dependencies.
void DisableAutocompleteProviders(AppLaunchConfiguration& configuration,
                                  int provider_types);

/// HTTP responses for omnibox test pages.
std::unique_ptr<net::test_server::HttpResponse> OmniboxHTTPResponses(
    const net::test_server::HttpRequest& request);

}  // namespace omnibox

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_TEST_UTIL_H_
