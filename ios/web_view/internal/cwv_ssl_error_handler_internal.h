// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_CWV_SSL_ERROR_HANDLER_INTERNAL_H_
#define IOS_WEB_VIEW_INTERNAL_CWV_SSL_ERROR_HANDLER_INTERNAL_H_

#include "ios/web_view/public/cwv_ssl_error_handler.h"

#import "ios/web/public/web_state.h"
#include "net/ssl/ssl_info.h"

NS_ASSUME_NONNULL_BEGIN

@interface CWVSSLErrorHandler ()

// Designated initializer.
// |URL| The URL associated with the SSL error.
// |error| The NSError object describing the error.
// |SSLInfo| Contains details of the SSL error.
// |errorPageHTMLCallback| Callback to be invoked to display an error page.
- (instancetype)initWithWebState:(web::WebState*)webState
                             URL:(NSURL*)URL
                           error:(NSError*)error
                         SSLInfo:(net::SSLInfo)SSLInfo
           errorPageHTMLCallback:(void (^)(NSString*))errorPageHTMLCallback
    NS_DESIGNATED_INITIALIZER;

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_INTERNAL_CWV_SSL_ERROR_HANDLER_INTERNAL_H_
