// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_FIND_IN_PAGE_FIND_IN_PAGE_API_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_FIND_IN_PAGE_FIND_IN_PAGE_API_H_

#import <os/availability.h>

namespace web {
class WebState;
}  // namespace web

@protocol UITextSearching;

namespace ios {
namespace provider {

// Convenience method for determining when Native Find in Page with System Find
// Panel is enabled.
bool IsNativeFindInPageWithSystemFindPanel();

// Convenience method for determining when Native Find in Page with Chrome Find
// Bar is enabled.
bool IsNativeFindInPageWithChromeFindBar();

// Convenience method for determining when Native Find in Page experiment is
// enabled.
bool IsNativeFindInPageEnabled();

// Provides a searchable object for a given `web_state`. Should only be called
// when `IsNativeFindInPageWithChromeFindBar()` returns `true`.
id<UITextSearching> GetSearchableObjectForWebState(web::WebState* web_state)
    API_AVAILABLE(ios(16));

// Initiates a Find in Page session on the given `web_state`. Should only be
// called when `IsNativeFindInPageWithChromeFindBar()` returns `true`.
void StartTextSearchInWebState(web::WebState* web_state);

// Terminates a Find in Page session on the given `web_state`. Should only be
// called when `IsNativeFindInPageWithChromeFindBar()` returns `true`.
void StopTextSearchInWebState(web::WebState* web_state);

}  // namespace provider
}  // namespace ios

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_FIND_IN_PAGE_FIND_IN_PAGE_API_H_
