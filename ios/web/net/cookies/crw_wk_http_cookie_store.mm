// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/net/cookies/crw_wk_http_cookie_store.h"

#import "base/check.h"
#import "base/feature_list.h"
#import "base/metrics/field_trial_params.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/histogram_macros.h"
#import "base/sequence_checker.h"
#import "base/task/sequenced_task_runner.h"
#import "ios/net/cookies/system_cookie_util.h"
#import "net/cookies/canonical_cookie.h"

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

// There appear to be lots of BackupRefPtr corruption crashes related to
// this cache optimization.  There are also crashes deep within Apple's
// network code that could be related. Since this optimization may no
// longer be necessary, experiment with its removal (crbug.com/40620220)
BASE_FEATURE(kIOSSkipCookieCaching,
             "SkipCookieCaching",
             base::FEATURE_DISABLED_BY_DEFAULT);

enum class SkipCookieCachingMode {
  // Skip the cookie caching optimization.
  kEnabled,

  // Instead of skipping the optimization, re-create the list of NSHTTPCookies
  // by converting to net::CanonicalCookie and back.
  kEnabledWithCopyWorkAround,

};

constexpr base::FeatureParam<SkipCookieCachingMode>::Option
    kIOSSkipCookieCachingOptions[] = {
        {SkipCookieCachingMode::kEnabled, "enabled"},
        {SkipCookieCachingMode::kEnabledWithCopyWorkAround,
         "enabled-with-copy-workaround"},
};

BASE_FEATURE_ENUM_PARAM(SkipCookieCachingMode,
                        kSkipCookieCachingModeParam,
                        &kIOSSkipCookieCaching,
                        "caching-mode",
                        SkipCookieCachingMode::kEnabledWithCopyWorkAround,
                        &kIOSSkipCookieCachingOptions);

base::Time GetNSHTTPCookieCreationTime(NSHTTPCookie* cookie) {
  id created = [[cookie properties] objectForKey:@"Created"];
  if (created && [created isKindOfClass:[NSNumber class]]) {
    CFAbsoluteTime absolute_time = [(NSNumber*)created doubleValue];
    return base::Time::FromCFAbsoluteTime(absolute_time);
  }
  return base::Time::Now();
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
  auto getCookiesBlock = ^(NSArray<NSHTTPCookie*>* cookies) {
    if (!base::FeatureList::IsEnabled(kIOSSkipCookieCaching)) {
      weakSelf.cachedCookies = cookies;
      completionHandler(cookies);
      return;
    }
    if (kSkipCookieCachingModeParam.Get() ==
        SkipCookieCachingMode::kEnabledWithCopyWorkAround) {
      const base::TimeTicks start = base::TimeTicks::Now();
      NSMutableArray<NSHTTPCookie*>* cookiesCopy =
          [NSMutableArray arrayWithCapacity:cookies.count];
      for (NSHTTPCookie* cookie in cookies) {
        // Move this logic to ios/net/cookies/system_cookie_util.h if the
        // experiment proves successful.
        base::Time creation_time = GetNSHTTPCookieCreationTime(cookie);
        std::unique_ptr<const net::CanonicalCookie> canonicalCookie =
            net::CanonicalCookieFromSystemCookie(cookie, creation_time);
        if (canonicalCookie) {
          NSHTTPCookie* systemCookie =
              SystemCookieFromCanonicalCookie(*canonicalCookie);
          if (systemCookie) {
            [cookiesCopy addObject:systemCookie];
          }
        }
      }

      base::UmaHistogramCounts100("IOS.CookieCopyWorkaroundMissingCookies",
                                  cookies.count - cookiesCopy.count);

      weakSelf.cachedCookies = cookiesCopy;
      cookies = weakSelf.cachedCookies;
      const base::TimeDelta elapsed = base::TimeTicks::Now() - start;
      base::UmaHistogramTimes("IOS.CookieCopyWorkaroundTime", elapsed);
    }
    completionHandler(cookies);
  };
  [_websiteDataStore.httpCookieStore getAllCookies:getCookiesBlock];
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
