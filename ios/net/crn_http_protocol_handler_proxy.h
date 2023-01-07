// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_NET_CRN_HTTP_PROTOCOL_HANDLER_PROXY_H_
#define IOS_NET_CRN_HTTP_PROTOCOL_HANDLER_PROXY_H_

#import <Foundation/Foundation.h>

#import "ios/net/clients/crn_network_client_protocol.h"

// HttpProtocolHandlerProxy is responsible for managing access to the
// NSURLProtocol and its client.
@protocol CRNHTTPProtocolHandlerProxy<CRNNetworkClientProtocol>

// All of the methods defined below must be called on the client thread. Methods
// defined by |CRNNetworkClientProtocol| can be called on any thread.

// Invalidates any reference to the protocol handler: the handler will never
// be called after this.
- (void)invalidate;

// Pauses notifications from this protocol handler.
- (void)pause;

// Resumes notifications from this protocol handler.
- (void)resume;

@end

#endif  // IOS_NET_CRN_HTTP_PROTOCOL_HANDLER_PROXY_H_
