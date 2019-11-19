// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/net/cookies/wk_http_system_cookie_store.h"

#include "base/bind.h"
#import "base/ios/block_types.h"
#include "base/task/post_task.h"
#import "ios/net/cookies/cookie_creation_time_manager.h"
#include "ios/net/cookies/system_cookie_util.h"
#include "ios/web/public/thread/web_task_traits.h"
#include "ios/web/public/thread/web_thread.h"
#import "ios/web/web_state/ui/wk_web_view_configuration_provider.h"
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
  base::PostTask(FROM_HERE, {web::WebThread::IO}, base::BindOnce(block));
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
  // Cookies handled by this method are app specific cookies, so it's safe to
  // use strict same site context.
  net::CookieOptions options = net::CookieOptions::MakeAllInclusive();
  net::CookieAccessSemantics cookie_access_semantics =
      net::CookieAccessSemantics::LEGACY;
  if (@available(iOS 13, *)) {
    // Using |UNKNOWN| semantics to allow the experiment to switch between non
    // legacy (where cookies that don't have a specific same-site access policy
    // and not secure will not be included), and legacy mode.
    cookie_access_semantics = net::CookieAccessSemantics::UNKNOWN;
  }
  return canonical_cookie
      .IncludeForRequestURL(url, options, cookie_access_semantics)
      .IsInclude();
}

}  // namespace

WKHTTPSystemCookieStore::WKHTTPSystemCookieStore(
    WKWebViewConfigurationProvider* config_provider)
    : crw_cookie_store_([[CRWWKHTTPCookieStore alloc] init]) {
  crw_cookie_store_.HTTPCookieStore = config_provider->GetWebViewConfiguration()
                                          .websiteDataStore.httpCookieStore;
  config_provider->AddObserver(this);
}

WKHTTPSystemCookieStore::~WKHTTPSystemCookieStore() = default;

#pragma mark -
#pragma mark SystemCookieStore methods

void WKHTTPSystemCookieStore::GetCookiesForURLAsync(
    const GURL& url,
    SystemCookieCallbackForCookies callback) {
  net::ReportGetCookiesForURLCall(
      net::SystemCookieStoreType::kWKHTTPSystemCookieStore);
  GetCookiesAsyncInternal(url, std::move(callback));
}

void WKHTTPSystemCookieStore::GetAllCookiesAsync(
    SystemCookieCallbackForCookies callback) {
  GetCookiesAsyncInternal(GURL::EmptyGURL(), std::move(callback));
}

void WKHTTPSystemCookieStore::DeleteCookieAsync(NSHTTPCookie* cookie,
                                                SystemCookieCallback callback) {
  __block SystemCookieCallback shared_callback = std::move(callback);
  base::WeakPtr<net::CookieCreationTimeManager> weak_time_manager =
      creation_time_manager_->GetWeakPtr();
  NSHTTPCookie* block_cookie = cookie;
  __weak __typeof(crw_cookie_store_) block_cookie_store = crw_cookie_store_;
  base::PostTask(
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
  // cookies can't be set if crw_cookie_store_ is deleted.
  DCHECK(crw_cookie_store_);
  __block SystemCookieCallback shared_callback = std::move(callback);
  base::WeakPtr<net::CookieCreationTimeManager> weak_time_manager =
      creation_time_manager_->GetWeakPtr();
  NSHTTPCookie* block_cookie = cookie;
  base::Time cookie_time = base::Time::Now();
  if (optional_creation_time && !optional_creation_time->is_null())
    cookie_time = *optional_creation_time;
  __weak __typeof(crw_cookie_store_) block_cookie_store = crw_cookie_store_;
  base::PostTask(
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
  __weak __typeof(crw_cookie_store_) block_cookie_store = crw_cookie_store_;
  base::PostTask(
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

#pragma mark WKWebViewConfigurationProviderObserver implementation

void WKHTTPSystemCookieStore::DidCreateNewConfiguration(
    WKWebViewConfigurationProvider* provider,
    WKWebViewConfiguration* new_config) {
  crw_cookie_store_.HTTPCookieStore =
      new_config.websiteDataStore.httpCookieStore;
}

#pragma mark private methods

void WKHTTPSystemCookieStore::GetCookiesAsyncInternal(
    const GURL& include_url,
    SystemCookieCallbackForCookies callback) {
  __block SystemCookieCallbackForCookies shared_callback = std::move(callback);
  base::WeakPtr<net::CookieCreationTimeManager> weak_time_manager =
      creation_time_manager_->GetWeakPtr();
  __weak __typeof(crw_cookie_store_) weak_cookie_store = crw_cookie_store_;
  GURL block_url = include_url;
  base::PostTask(
      FROM_HERE, {web::WebThread::UI}, base::BindOnce(^{
        __typeof(weak_cookie_store) strong_cookie_store = weak_cookie_store;
        if (strong_cookie_store) {
          [strong_cookie_store
              getAllCookies:^(NSArray<NSHTTPCookie*>* cookies) {
                ProcessGetCookiesResultInIOThread(std::move(shared_callback),
                                                  weak_time_manager, block_url,
                                                  cookies);
              }];
        } else {
          ProcessGetCookiesResultInIOThread(std::move(shared_callback),
                                            weak_time_manager, block_url, @[]);
        }
      }));
}

// static
void WKHTTPSystemCookieStore::ProcessGetCookiesResultInIOThread(
    net::SystemCookieStore::SystemCookieCallbackForCookies callback,
    base::WeakPtr<net::CookieCreationTimeManager> weak_time_manager,
    const GURL& include_url,
    NSArray<NSHTTPCookie*>* _Nonnull cookies) {
  if (callback.is_null())
    return;
  __block NSArray* block_cookies = cookies;
  GURL block_url = include_url;

  __block net::SystemCookieStore::SystemCookieCallbackForCookies
      shared_callback = std::move(callback);
  RunBlockOnIOThread(^{
    if (!block_url.is_empty()) {
      NSMutableArray* filtered_cookies = [NSMutableArray array];
      for (NSHTTPCookie* cookie in block_cookies) {
        if (ShouldIncludeForRequestUrl(cookie, block_url)) {
          [filtered_cookies addObject:cookie];
        }
      }
      net::ReportGetCookiesForURLResult(
          net::SystemCookieStoreType::kWKHTTPSystemCookieStore,
          filtered_cookies.count != 0);
      block_cookies = filtered_cookies;
    }

    if (weak_time_manager) {
      NSArray* sorted_results = [block_cookies
          sortedArrayUsingFunction:net::SystemCookieStore::CompareCookies
                           context:weak_time_manager.get()];
      std::move(shared_callback).Run(sorted_results);
    } else {
      std::move(shared_callback).Run([block_cookies copy]);
    }
  });
}

}  // namespace web
