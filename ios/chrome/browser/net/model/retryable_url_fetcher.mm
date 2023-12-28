// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/net/model/retryable_url_fetcher.h"

#import <memory>

#import "base/check.h"
#import "base/functional/bind.h"
#import "base/strings/sys_string_conversions.h"
#import "services/network/public/cpp/resource_request.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"
#import "services/network/public/cpp/simple_url_loader.h"
#import "url/gurl.h"

@interface RetryableURLFetcher ()
- (void)urlFetchDidComplete:(std::unique_ptr<std::string>)response_body;
@end

@implementation RetryableURLFetcher {
  scoped_refptr<network::SharedURLLoaderFactory> _shared_url_loader_factory;
  std::unique_ptr<net::BackoffEntry> _backoffEntry;
  std::unique_ptr<network::SimpleURLLoader> _simple_loader;
  int _retryCount;
  __weak id<RetryableURLFetcherDelegate> _delegate;
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
    _shared_url_loader_factory = shared_url_loader_factory;
    _delegate = delegate;
    if (policy)
      _backoffEntry.reset(new net::BackoffEntry(policy));
  }
  return self;
}

- (void)startFetch {
  DCHECK(_shared_url_loader_factory.get());
  GURL url(base::SysNSStringToUTF8([_delegate urlToFetch]));
  if (url.is_valid()) {
    auto resource_request = std::make_unique<network::ResourceRequest>();
    resource_request->url = url;
    _simple_loader = network::SimpleURLLoader::Create(
        std::move(resource_request), NO_TRAFFIC_ANNOTATION_YET);

    _simple_loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
        _shared_url_loader_factory.get(),
        base::BindOnce(^(std::unique_ptr<std::string> response) {
          [self urlFetchDidComplete:std::forward<std::unique_ptr<std::string>>(
                                        response)];
        }));
  } else {
    // Invalid URLs returned from delegate method are considered a permanent
    // failure. Delegate method is called with nil to indicate failure.
    [_delegate processSuccessResponse:nil];
  }
}

- (int)failureCount {
  return _backoffEntry ? _backoffEntry->failure_count() : 0;
}

- (void)urlFetchDidComplete:(std::unique_ptr<std::string>)response_body {
  if (!response_body && _backoffEntry) {
    _backoffEntry->InformOfRequest(false);
    double nextRetry = _backoffEntry->GetTimeUntilRelease().InSecondsF();
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, nextRetry * NSEC_PER_SEC),
                   dispatch_get_main_queue(), ^{
                     [self startFetch];
                   });
    return;
  }
  NSString* response = nil;
  if (response_body)
    response = base::SysUTF8ToNSString(*response_body);
  [_delegate processSuccessResponse:response];
}

@end
