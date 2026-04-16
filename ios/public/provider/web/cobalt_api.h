// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_WEB_COBALT_API_H_
#define IOS_PUBLIC_PROVIDER_WEB_COBALT_API_H_

#import <Foundation/Foundation.h>

@class WKWebViewConfiguration;

namespace web {
class CobaltController;
}

namespace web::provider {

// Initializes Cobalt in `configuration` using the given `cobalt_controller`.
void InitializeCobaltInWKWebViewConfiguration(
    WKWebViewConfiguration* configuration,
    bool is_off_the_record,
    web::CobaltController* cobalt_controller);

// Returns the list of origins that are allowed to use Cobalt.
NSArray<NSString*>* GetCobaltOriginList();

}  // namespace web::provider

#endif  // IOS_PUBLIC_PROVIDER_WEB_COBALT_API_H_
