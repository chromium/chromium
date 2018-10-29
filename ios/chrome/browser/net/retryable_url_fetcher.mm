// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/net/retryable_url_fetcher.h"

#include <memory>

#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface RetryableURLFetcher ()
- (void)urlFetchDidComplete:(std::unique_ptr<std::string>)response_body;
@end

@implementation RetryableURLFetcher {
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;
  std::unique_ptr<net::BackoffEntry> backoffEntry_;
  std::unique_ptr<network::SimpleURLLoader> simple_loader_;
  int retryCount_;
  __weak id<RetryableURLFetcherDelegate> delegate_;
}

- (instancetype)
initWithURLLoaderFactory:
    (scoped_refptr<network::SharedURLLoaderFactory>)shared_url_loader_factory
                delegate:(id<RetryableURLFetcherDelegate>)delegate
           backoffPolicy:(const net::BackoffEntry::Policy*)policy {
  self = [super init];
  if (self) {
    DCHECK(shared_url_loader_factory);
    DCHECK(delegate);
    shared_url_loader_factory_ = shared_url_loader_factory;
    delegate_ = delegate;
    if (policy)
      backoffEntry_.reset(new net::BackoffEntry(policy));
  }
  return self;
}

- (void)startFetch {
  DCHECK(shared_url_loader_factory_.get());
  GURL url(base::SysNSStringToUTF8([delegate_ urlToFetch]));
  if (url.is_valid()) {
    auto resource_request = std::make_unique<network::ResourceRequest>();
    resource_request->url = url;
    simple_loader_ = network::SimpleURLLoader::Create(
        std::move(resource_request), NO_TRAFFIC_ANNOTATION_YET);

    simple_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
        shared_url_loader_factory_.get(),
        base::BindOnce(^(std::unique_ptr<std::string> response) {
          [self urlFetchDidComplete:std::forward<std::unique_ptr<std::string>>(
                                        response)];
        }));
  } else {
    // Invalid URLs returned from delegate method are considered a permanent
    // failure. Delegate method is called with nil to indicate failure.
    [delegate_ processSuccessResponse:nil];
  }
}

- (int)failureCount {
  return backoffEntry_ ? backoffEntry_->failure_count() : 0;
}

- (void)urlFetchDidComplete:(std::unique_ptr<std::string>)response_body {
  if (!response_body && backoffEntry_) {
    backoffEntry_->InformOfRequest(false);
    double nextRetry = backoffEntry_->GetTimeUntilRelease().InSecondsF();
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, nextRetry * NSEC_PER_SEC),
                   dispatch_get_main_queue(), ^{
                     [self startFetch];
                   });
    return;
  }
  NSString* response = nil;
  if (response_body)
    response = base::SysUTF8ToNSString(*response_body);
  [delegate_ processSuccessResponse:response];
}

@end
