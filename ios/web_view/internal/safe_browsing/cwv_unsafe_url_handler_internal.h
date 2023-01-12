// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_SAFE_BROWSING_CWV_UNSAFE_URL_HANDLER_INTERNAL_H_
#define IOS_WEB_VIEW_INTERNAL_SAFE_BROWSING_CWV_UNSAFE_URL_HANDLER_INTERNAL_H_

#import "base/functional/callback.h"
#import "components/security_interstitials/core/unsafe_resource.h"
#import "ios/web/public/web_state.h"
#import "ios/web_view/public/cwv_unsafe_url_handler.h"

@interface CWVUnsafeURLHandler ()

- (instancetype)initWithWebState:(web::WebState*)webState
                  unsafeResource:(const security_interstitials::UnsafeResource&)
                                     unsafeResource
                    htmlCallback:
                        (base::OnceCallback<void(NSString*)>)htmlCallback
    NS_DESIGNATED_INITIALIZER;

@end

#endif  // IOS_WEB_VIEW_INTERNAL_SAFE_BROWSING_CWV_UNSAFE_URL_HANDLER_INTERNAL_H_
