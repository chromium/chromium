// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NET_RETRYABLE_URL_FETCHER_H_
#define IOS_CHROME_BROWSER_NET_RETRYABLE_URL_FETCHER_H_

#import <Foundation/Foundation.h>

#include "base/memory/scoped_refptr.h"
#include "net/base/backoff_entry.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

// Delegate protocol for RetryableURLFetcher object.
@protocol RetryableURLFetcherDelegate<NSObject>

// Returns the HTTP URL for RetryableURLFetcher to fetch.
- (NSString*)urlToFetch;

// Callback function after URL has been fetched. |response| is the content of
// the HTTP response. This method may be called with a nil |response| if the
// HTTP request failed.
- (void)processSuccessResponse:(NSString*)response;

@end

@interface RetryableURLFetcher : NSObject

// Designated initializer. |shared_url_loader_factory| and |delegate| must not
// be nil. If |policy| is not null, it specifies how often to retry the URL
// fetch on a call to -startFetch. If |policy| is null, there is no retry.
- (instancetype)
initWithURLLoaderFactory:
    (scoped_refptr<network::SharedURLLoaderFactory>)shared_url_loader_factory
                delegate:(id<RetryableURLFetcherDelegate>)delegate
           backoffPolicy:(const net::BackoffEntry::Policy*)policy;

// Starts fetching URL. Uses the backoff policy specified when the object was
// initialized.
- (void)startFetch;

// Returns the number of times that this URL Fetcher failed to receive a
// success response. Returns 0 if this URL Fetcher was not set up to do retries.
- (int)failureCount;

@end

#endif  // IOS_CHROME_BROWSER_NET_RETRYABLE_URL_FETCHER_H_
