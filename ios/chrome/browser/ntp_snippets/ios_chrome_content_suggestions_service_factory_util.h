// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NTP_SNIPPETS_IOS_CHROME_CONTENT_SUGGESTIONS_SERVICE_FACTORY_UTIL_H_
#define IOS_CHROME_BROWSER_NTP_SNIPPETS_IOS_CHROME_CONTENT_SUGGESTIONS_SERVICE_FACTORY_UTIL_H_

#include <memory>

#include "components/keyed_service/core/keyed_service.h"
#include "ios/web/public/browser_state.h"

namespace ntp_snippets {
class ContentSuggestionsService;
}  // namespace ntp_snippets

namespace ntp_snippets {

// Returns a new ContentSuggestionsService with all providers registered.
std::unique_ptr<KeyedService>
CreateChromeContentSuggestionsServiceWithProviders(
    web::BrowserState* browser_state);

// Returns a new ContentSuggestionsService with no provider registered.
std::unique_ptr<KeyedService> CreateChromeContentSuggestionsService(
    web::BrowserState* browser_state);

// Registers the RemoteSuggestionsProvider (articles provider).
void RegisterRemoteSuggestionsProvider(ContentSuggestionsService* service,
                                       web::BrowserState* browser_state);

}  // namespace ntp_snippets

#endif  // IOS_CHROME_BROWSER_NTP_SNIPPETS_IOS_CHROME_CONTENT_SUGGESTIONS_SERVICE_FACTORY_UTIL_H_
