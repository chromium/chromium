// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_NET_CLIENTS_CRN_NETWORK_CLIENT_PROTOCOL_H_
#define IOS_NET_CLIENTS_CRN_NETWORK_CLIENT_PROTOCOL_H_

#import <Foundation/Foundation.h>

#include "base/strings/string_util.h"

namespace net {
class AuthChallengeInfo;
class URLRequest;
}  // namespace net

// CRNNetworkClientProtocol provides an interface for delegate classes that
// receive calls about data loading from the Chromium network stack.
// Many methods in this protocol correspond to the NSURLProtocol methods, and
// are called by the HttpProtocolHandlerCore when events occur in the network
// stack. A class that implements this protocol can respond by proxying the call
// over to an NSURLProtocolClient that it manages, or by managing the data as it
// sees fit.
@protocol CRNNetworkClientProtocol<NSObject>
// Methods corresponding to NSURLProtocolClient methods.
// Called from the IO thread.
// Corresponds to |-URLProtocol:didFailWithError|.
// The base client will create an NSError instance using |nsErrorCode| (an error
// code from NSURLError.h) and |netErrorCode| (a Chrome  errror code) and url
// and creation time information supplied by the HttpProtocolHandlerCore
// instance.
- (void)didFailWithNSErrorCode:(NSInteger)nsErrorCode
                  netErrorCode:(int)netErrorCode;
// Corresponds to |-URLProtocol:didLoadData|.
- (void)didLoadData:(NSData*)data;
// Corresponds to |-URLProtocol:didReceiveResponse:cacheStoragePolicy|.
- (void)didReceiveResponse:(NSURLResponse*)response;
// Corresponds to |-URLProtocol:wasRedirectedToRequest:redirectResponse|.
- (void)wasRedirectedToRequest:(NSURLRequest*)request
                 nativeRequest:(net::URLRequest*)nativeRequest
              redirectResponse:(NSURLResponse*)redirectResponse;
// Corresponds to |-URLProtocol:didFinishLoading|.
- (void)didFinishLoading;

// Methods that don't correspond to NSURLProtocolClient methdods but which are
// used to implement injectible features using network clients.

// Called after |nativeRequest| is created, but before it's started; native
// clients have the opportunity to modify the request at this time.
- (void)didCreateNativeRequest:(net::URLRequest*)nativeRequest;
@end

#endif  // IOS_NET_CLIENTS_CRN_NETWORK_CLIENT_PROTOCOL_H_
