// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_NET_CRN_HTTP_PROTOCOL_HANDLER_PROXY_WITH_CLIENT_THREAD_H_
#define IOS_NET_CRN_HTTP_PROTOCOL_HANDLER_PROXY_WITH_CLIENT_THREAD_H_

#import <Foundation/Foundation.h>

#import "ios/net/crn_http_protocol_handler_proxy.h"

// This CRNHTTPProtocolHandlerProxy only calls the client from the client
// thread. This is what is recommended by the documentation.
@interface CRNHTTPProtocolHandlerProxyWithClientThread
    : NSObject<CRNHTTPProtocolHandlerProxy>

- (instancetype)initWithProtocol:(NSURLProtocol*)protocol
                    clientThread:(NSThread*)clientThread
                     runLoopMode:(NSString*)mode;
@end

#endif  // IOS_NET_CRN_HTTP_PROTOCOL_HANDLER_PROXY_WITH_CLIENT_THREAD_H_
