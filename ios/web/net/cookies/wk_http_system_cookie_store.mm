// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/net/cookies/wk_http_system_cookie_store.h"

#include "base/bind.h"
#import "base/ios/block_types.h"
#include "base/task/post_task.h"
#import "ios/net/cookies/cookie_creation_time_manager.h"
#include "ios/net/cookies/system_cookie_util.h"
#include "ios/web/public/web_task_traits.h"
#include "ios/web/public/web_thread.h"
#import "net/base/mac/url_conversions.h"
#include "net/cookies/canonical_cookie.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {
namespace {

// Posts a task to run |block| on IO Thread. This is needed because
// WKHTTPCookieStore executes callbacks on the main thread, while
// SystemCookieStore should operate on IO thread.
void RunBlockOnIOThread(ProceduralBlock block) {
  DCHECK(block != nil);
  base::PostTaskWithTraits(FROM_HERE, {web::WebThread::IO},
                           base::BindOnce(block));
}

// Returns wether |cookie| should be included for queries about |url|.
// To include |cookie| for |url|, all these conditions need to be met:
//   1- If the cookie is secure the URL needs to be secure.
//   2- |url| domain need to match the cookie domain.
//   3- |cookie| url path need to be on the path of the given |url|.
bool ShouldIncludeForRequestUrl(NSHTTPCookie* cookie, const GURL& url) {
  // CanonicalCookies already implements cookie selection for URLs, so instead
  // of rewriting the checks here, the function converts the NSHTTPCookie to
  // canonical cookie and provide it with dummy CookieOption, so when iOS starts
  // to support cookieOptions this function can be modified to support that.
  net::CanonicalCookie canonical_cookie =
      net::CanonicalCookieFromSystemCookie(cookie, base::Time());
  net::CookieOptions options;
  options.set_include_httponly();
  return canonical_cookie.IncludeForRequestURL(url, options);
}

// Prioritizes queued WKHTTPCookieStore completion handlers to run as soon as
// possible. This function is needed because some of WKHTTPCookieStore methods
// completion handlers are not called until there is a WKWebView on the view
// hierarchy.
void PrioritizeWKHTTPCookieStoreCallbacks() {
  // TODO(crbug.com/885218): Currently this hack is needed to fix
  // crbug.com/885218. Remove when the behavior of
  // [WKHTTPCookieStore getAllCookies:] changes.
  NSSet* data_types = [NSSet setWithObject:WKWebsiteDataTypeCookies];
  [[WKWebsiteDataStore defaultDataStore]
      removeDataOfTypes:data_types
          modifiedSince:[NSDate distantFuture]
      completionHandler:^{
      }];
}

}  // namespace

WKHTTPSystemCookieStore::WKHTTPSystemCookieStore(
    WKHTTPCookieStore* cookie_store)
    : cookie_store_(cookie_store) {}

WKHTTPSystemCookieStore::~WKHTTPSystemCookieStore() = default;

#pragma mark -
#pragma mark SystemCookieStore methods

void WKHTTPSystemCookieStore::GetCookiesForURLAsync(
    const GURL& url,
    SystemCookieCallbackForCookies callback) {
  // This function shouldn't be called if cookie_store_ is deleted.
  DCHECK(cookie_store_);
  net::ReportGetCookiesForURLCall(
      net::SystemCookieStoreType::kWKHTTPSystemCookieStore);
  __block SystemCookieCallbackForCookies shared_callback = std::move(callback);
  base::WeakPtr<net::CookieCreationTimeManager> weak_time_manager =
      creation_time_manager_->GetWeakPtr();
  __weak WKHTTPCookieStore* block_cookie_store = cookie_store_;
  GURL block_url = url;
  base::PostTaskWithTraits(
      FROM_HERE, {web::WebThread::UI}, base::BindOnce(^{
        WKHTTPCookieStore* strong_cookie_store = block_cookie_store;
        if (strong_cookie_store) {
          [strong_cookie_store
              getAllCookies:^(NSArray<NSHTTPCookie*>* cookies) {
                NSMutableArray* result = [NSMutableArray array];
                for (NSHTTPCookie* cookie in cookies) {
                  if (ShouldIncludeForRequestUrl(cookie, block_url)) {
                    [result addObject:cookie];
                  }
                }
                net::ReportGetCookiesForURLResult(
                    net::SystemCookieStoreType::kWKHTTPSystemCookieStore,
                    result.count != 0);
                RunSystemCookieCallbackForCookies(std::move(shared_callback),
                                                  weak_time_manager, result);
              }];
          PrioritizeWKHTTPCookieStoreCallbacks();
        } else {
          net::ReportGetCookiesForURLResult(
              net::SystemCookieStoreType::kWKHTTPSystemCookieStore, false);
          RunSystemCookieCallbackForCookies(std::move(shared_callback),
                                            weak_time_manager, @[]);
        }
      }));
}

void WKHTTPSystemCookieStore::GetAllCookiesAsync(
    SystemCookieCallbackForCookies callback) {
  // This function shouldn't be called if cookie_store_ is deleted.
  DCHECK(cookie_store_);
  __block SystemCookieCallbackForCookies shared_callback = std::move(callback);
  __weak WKHTTPCookieStore* block_cookie_store = cookie_store_;
  base::WeakPtr<net::CookieCreationTimeManager> weak_time_manager =
      creation_time_manager_->GetWeakPtr();
  base::PostTaskWithTraits(
      FROM_HERE, {web::WebThread::UI}, base::BindOnce(^{
        WKHTTPCookieStore* strong_cookie_store = block_cookie_store;
        if (strong_cookie_store) {
          [strong_cookie_store
              getAllCookies:^(NSArray<NSHTTPCookie*>* cookies) {
                RunSystemCookieCallbackForCookies(std::move(shared_callback),
                                                  weak_time_manager, cookies);
              }];
          PrioritizeWKHTTPCookieStoreCallbacks();
        } else {
          RunSystemCookieCallbackForCookies(std::move(shared_callback),
                                            weak_time_manager, @[]);
        }
      }));
}

void WKHTTPSystemCookieStore::DeleteCookieAsync(NSHTTPCookie* cookie,
                                                SystemCookieCallback callback) {
  // This function shouldn't be called if cookie_store_ is deleted.
  DCHECK(cookie_store_);
  __block SystemCookieCallback shared_callback = std::move(callback);
  base::WeakPtr<net::CookieCreationTimeManager> weak_time_manager =
      creation_time_manager_->GetWeakPtr();
  NSHTTPCookie* block_cookie = cookie;
  __weak WKHTTPCookieStore* block_cookie_store = cookie_store_;
  base::PostTaskWithTraits(
      FROM_HERE, {web::WebThread::UI}, base::BindOnce(^{
        [block_cookie_store
                 deleteCookie:block_cookie
            completionHandler:^{
              RunBlockOnIOThread(^{
                if (weak_time_manager)
                  weak_time_manager->DeleteCreationTime(block_cookie);
                if (!shared_callback.is_null())
                  std::move(shared_callback).Run();
              });
            }];
      }));
}

void WKHTTPSystemCookieStore::SetCookieAsync(
    NSHTTPCookie* cookie,
    const base::Time* optional_creation_time,
    SystemCookieCallback callback) {
  // cookies can't be set if cookie_store_ is deleted.
  DCHECK(cookie_store_);
  __block SystemCookieCallback shared_callback = std::move(callback);
  base::WeakPtr<net::CookieCreationTimeManager> weak_time_manager =
      creation_time_manager_->GetWeakPtr();
  NSHTTPCookie* block_cookie = cookie;
  base::Time cookie_time = base::Time::Now();
  if (optional_creation_time && !optional_creation_time->is_null())
    cookie_time = *optional_creation_time;
  __weak WKHTTPCookieStore* block_cookie_store = cookie_store_;
  base::PostTaskWithTraits(
      FROM_HERE, {web::WebThread::UI}, base::BindOnce(^{
        [block_cookie_store
                    setCookie:block_cookie
            completionHandler:^{
              RunBlockOnIOThread(^{
                if (weak_time_manager) {
                  weak_time_manager->SetCreationTime(
                      block_cookie,
                      weak_time_manager->MakeUniqueCreationTime(cookie_time));
                }
                if (!shared_callback.is_null())
                  std::move(shared_callback).Run();
              });
            }];
      }));
}

void WKHTTPSystemCookieStore::ClearStoreAsync(SystemCookieCallback callback) {
  __block SystemCookieCallback shared_callback = std::move(callback);
  base::WeakPtr<net::CookieCreationTimeManager> weak_time_manager =
      creation_time_manager_->GetWeakPtr();
  __weak WKHTTPCookieStore* block_cookie_store = cookie_store_;
  base::PostTaskWithTraits(
      FROM_HERE, {web::WebThread::UI}, base::BindOnce(^{
        [block_cookie_store getAllCookies:^(NSArray<NSHTTPCookie*>* cookies) {
          ProceduralBlock completionHandler = ^{
            RunBlockOnIOThread(^{
              if (weak_time_manager)
                weak_time_manager->Clear();
              std::move(shared_callback).Run();
            });
          };

          // If there are no cookies to clear, immediately invoke the
          // completion handler on IO thread, otherwise count the number
          // of cookies that still need to be cleared and invoke it when
          // all of them have been cleared.
          __block NSUInteger remainingCookiesToClearCount = cookies.count;
          if (remainingCookiesToClearCount == 0) {
            completionHandler();
            return;
          }

          for (NSHTTPCookie* cookie in cookies) {
            [block_cookie_store deleteCookie:cookie
                           completionHandler:^{
                             DCHECK(remainingCookiesToClearCount);
                             if (--remainingCookiesToClearCount == 0) {
                               completionHandler();
                             }
                           }];
          }
        }];
      }));
}

NSHTTPCookieAcceptPolicy WKHTTPSystemCookieStore::GetCookieAcceptPolicy() {
  // TODO(crbug.com/759226): Make sure there is no other way to return
  // WKHTTPCookieStore Specific cookieAcceptPolicy.
  return [[NSHTTPCookieStorage sharedHTTPCookieStorage] cookieAcceptPolicy];
}

#pragma mark private methods

// static
// Runs |callback| on |cookies| after sorting them as per RFC6265 using
// |weak_time_manager|.
void WKHTTPSystemCookieStore::RunSystemCookieCallbackForCookies(
    net::SystemCookieStore::SystemCookieCallbackForCookies callback,
    base::WeakPtr<net::CookieCreationTimeManager> weak_time_manager,
    NSArray<NSHTTPCookie*>* _Nonnull cookies) {
  if (callback.is_null())
    return;
  NSArray* block_cookies = cookies;
  __block net::SystemCookieStore::SystemCookieCallbackForCookies
      shared_callback = std::move(callback);
  RunBlockOnIOThread(^{
    if (weak_time_manager) {
      NSArray* result = [block_cookies
          sortedArrayUsingFunction:net::SystemCookieStore::CompareCookies
                           context:weak_time_manager.get()];
      std::move(shared_callback).Run(result);
    } else {
      std::move(shared_callback).Run(block_cookies);
    }
  });
}

}  // namespace web
