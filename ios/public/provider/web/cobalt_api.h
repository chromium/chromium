// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_WEB_COBALT_API_H_
#define IOS_PUBLIC_PROVIDER_WEB_COBALT_API_H_

#import <Foundation/Foundation.h>

namespace web {
class BrowserState;
}
@class WKWebViewConfiguration;

namespace web::provider {

// Initializes Cobalt in `configuration` for the given `browser_state`.
void InitializeCobaltInWKWebViewConfiguration(
    WKWebViewConfiguration* configuration,
    BrowserState* browser_state);

// Returns the list of origins that are allowed to use Cobalt.
NSArray<NSString*>* GetCobaltOriginList();

}  // namespace web::provider

#endif  // IOS_PUBLIC_PROVIDER_WEB_COBALT_API_H_
