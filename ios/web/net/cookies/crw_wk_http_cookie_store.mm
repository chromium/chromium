// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/net/cookies/crw_wk_http_cookie_store.h"

#import "base/check.h"
#import "base/sequence_checker.h"
#import "base/task/sequenced_task_runner.h"

namespace {
// Prioritizes queued WKHTTPCookieStore completion handlers to run as soon as
// possible. This function is needed because some of WKHTTPCookieStore methods
// completion handlers are not called until there is a WKWebView on the view
// hierarchy.
void PrioritizeWKHTTPCookieStoreCallbacks(WKWebsiteDataStore* data_store) {
  CHECK(data_store);
  // TODO(crbug.com/41414488): Currently this hack is needed to fix
  // crbug.com/885218. Remove when the behavior of
  // [WKHTTPCookieStore getAllCookies:] changes.
  NSSet* data_types = [NSSet setWithObject:WKWebsiteDataTypeCookies];
  [data_store fetchDataRecordsOfTypes:data_types
                    completionHandler:^(NSArray<WKWebsiteDataRecord*>* records){
                    }];
}
}  // namespace

@interface CRWWKHTTPCookieStore () <WKHTTPCookieStoreObserver>

// The last getAllCookies output. Will always be set from the UI
// thread.
@property(nonatomic) NSArray<NSHTTPCookie*>* cachedCookies;

@end

@implementation CRWWKHTTPCookieStore {
  SEQUENCE_CHECKER(_sequenceChecker);
}

- (void)dealloc {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
}

- (void)getAllCookies:(void (^)(NSArray<NSHTTPCookie*>*))completionHandler {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (_cachedCookies) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(completionHandler, _cachedCookies));
    return;
  }

  if (!_websiteDataStore.httpCookieStore) {
    // CRWWKHTTPCookieStore doesn't retain `_websiteDataStore` instance so it's
    // possible that it becomes nil while tearing down an application. Call the
    // callback if it's nil.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(completionHandler, @[]));
    return;
  }

  __weak __typeof(self) weakSelf = self;
  [_websiteDataStore.httpCookieStore
      getAllCookies:^(NSArray<NSHTTPCookie*>* cookies) {
        weakSelf.cachedCookies = cookies;
        completionHandler(cookies);
      }];
  PrioritizeWKHTTPCookieStoreCallbacks(_websiteDataStore);
}

- (void)setCookie:(NSHTTPCookie*)cookie
    completionHandler:(void (^)(void))completionHandler {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  _cachedCookies = nil;
  if (!_websiteDataStore.httpCookieStore) {
    // CRWWKHTTPCookieStore doesn't retain `_websiteDataStore` instance so it's
    // possible that it becomes nil while tearing down an application. Call the
    // callback if it's nil.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(completionHandler));
    return;
  }
  [_websiteDataStore.httpCookieStore setCookie:cookie
                             completionHandler:completionHandler];
}

- (void)deleteCookie:(NSHTTPCookie*)cookie
    completionHandler:(void (^)(void))completionHandler {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  _cachedCookies = nil;
  if (!_websiteDataStore.httpCookieStore) {
    // CRWWKHTTPCookieStore doesn't retain `_websiteDataStore` instance so it's
    // possible that it becomes nil while tearing down an application. Call the
    // callback if it's nil.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(completionHandler));
    return;
  }
  [_websiteDataStore.httpCookieStore deleteCookie:cookie
                                completionHandler:completionHandler];
}

- (void)clearCookies:(void (^)(void))completionHandler {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  __weak CRWWKHTTPCookieStore* weakSelf = self;
  [self getAllCookies:^(NSArray<NSHTTPCookie*>* cookies) {
    [weakSelf deleteCookies:cookies completionHandler:completionHandler];
  }];
}

- (void)setWebsiteDataStore:(WKWebsiteDataStore*)newWebsiteDataStore {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  _cachedCookies = nil;
  if (newWebsiteDataStore == _websiteDataStore) {
    return;
  }
  [_websiteDataStore.httpCookieStore removeObserver:self];
  _websiteDataStore = newWebsiteDataStore;
  [_websiteDataStore.httpCookieStore addObserver:self];
}

#pragma mark WKHTTPCookieStoreObserver method

- (void)cookiesDidChangeInCookieStore:(WKHTTPCookieStore*)cookieStore {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  CHECK_EQ(cookieStore, _websiteDataStore.httpCookieStore);
  _cachedCookies = nil;
}

#pragma mark - Private methods

- (void)deleteCookies:(NSArray<NSHTTPCookie*>*)cookies
    completionHandler:(void (^)(void))completionHandler {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  _cachedCookies = nil;

  // If there are no cookies to clear, then invoke the completion handler and
  // return, otherwise ask `_websiteDataStore.httpCookieStore` to delete all
  // cookies, invoking the completion handler after the last delete operation
  // completes.
  if (cookies.count == 0) {
    completionHandler();
    return;
  }

  __block NSUInteger counter = cookies.count;
  for (NSHTTPCookie* cookie in cookies) {
    [_websiteDataStore.httpCookieStore deleteCookie:cookie
                                  completionHandler:^{
                                    if (--counter == 0) {
                                      completionHandler();
                                    }
                                  }];
  }
}

@end
