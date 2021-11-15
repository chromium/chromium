// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_CWV_LEGACY_TLS_WARNING_HANDLER_INTERNAL_H_
#define IOS_WEB_VIEW_INTERNAL_CWV_LEGACY_TLS_WARNING_HANDLER_INTERNAL_H_

#include "ios/web_view/public/cwv_legacy_tls_warning_handler.h"

#import "ios/web/public/web_state.h"

NS_ASSUME_NONNULL_BEGIN

@interface CWVLegacyTLSWarningHandler ()

// Designated initializer.
// |URL| The URL that is using legacy TLS.
// |error| The NSError object containing details of the warning.
// |warningPageHTMLCallback| Callback to be invoked to display an error page.
- (instancetype)initWithWebState:(web::WebState*)webState
                             URL:(NSURL*)URL
                           error:(NSError*)error
         warningPageHTMLCallback:(void (^)(NSString*))warningPageHTMLCallback
    NS_DESIGNATED_INITIALIZER;

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_INTERNAL_CWV_LEGACY_TLS_WARNING_HANDLER_INTERNAL_H_
