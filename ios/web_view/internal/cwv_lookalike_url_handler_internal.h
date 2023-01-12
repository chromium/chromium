// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_CWV_LOOKALIKE_URL_HANDLER_INTERNAL_H_
#define IOS_WEB_VIEW_INTERNAL_CWV_LOOKALIKE_URL_HANDLER_INTERNAL_H_

#import "base/functional/callback.h"
#import "ios/components/security_interstitials/lookalikes/lookalike_url_container.h"
#import "ios/web/public/web_state.h"
#import "ios/web_view/public/cwv_lookalike_url_handler.h"

@interface CWVLookalikeURLHandler ()

// Designated initializer.
// |webState| The associated web state.
// |lookalikeURLInfo| Encapsulates information about the lookalike URL.
// |htmlCallback| A callback that can be used to display an interstitial page.
- (instancetype)
    initWithWebState:(web::WebState*)webState
    lookalikeURLInfo:(std::unique_ptr<LookalikeUrlContainer::LookalikeUrlInfo>)
                         lookalikeURLInfo
        htmlCallback:(base::OnceCallback<void(NSString*)>)htmlCallback
    NS_DESIGNATED_INITIALIZER;

@end

#endif  // IOS_WEB_VIEW_INTERNAL_CWV_LOOKALIKE_URL_HANDLER_INTERNAL_H_
