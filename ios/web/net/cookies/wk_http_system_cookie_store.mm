// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/net/cookies/wk_http_system_cookie_store.h"

#import "base/functional/bind.h"
#import "base/functional/callback_helpers.h"
#import "base/ios/block_types.h"
#import "base/task/bind_post_task.h"
#import "base/task/sequenced_task_runner.h"
#import "ios/net/cookies/cookie_creation_time_manager.h"
#import "ios/net/cookies/system_cookie_util.h"
#import "ios/web/net/cookies/crw_wk_http_cookie_store.h"
#import "ios/web/public/thread/web_task_traits.h"
#import "ios/web/public/thread/web_thread.h"
#import "ios/web/web_state/ui/wk_web_view_configuration_provider.h"
#import "net/base/apple/url_conversions.h"
#import "net/cookies/canonical_cookie.h"
#import "net/cookies/cookie_constants.h"
#import "url/gurl.h"

namespace web {
namespace {

// Returns wether `cookie` should be included for queries about `url`.
// To include `cookie` for `url`, all these conditions need to be met:
//   1- If the cookie is secure the URL needs to be secure.
//   2- `url` domain need to match the cookie domain.
//   3- `cookie` url path need to be on the path of the given `url`.
bool ShouldIncludeForRequestUrl(NSHTTPCookie* cookie, const GURL& url) {
  // CanonicalCookies already implements cookie selection for URLs, so instead
  // of rewriting the checks here, the function converts the NSHTTPCookie to
  // canonical cookie and provide it with dummy CookieOption, so when iOS starts
  // to support cookieOptions this function can be modified to support that.
  std::unique_ptr<net::CanonicalCookie> canonical_cookie =
      net::CanonicalCookieFromSystemCookie(cookie, base::Time());
  if (!canonical_cookie)
    return false;
  // Cookies handled by this method are app specific cookies, so it's safe to
  // use strict same site context.
  net::CookieOptions options = net::CookieOptions::MakeAllInclusive();
  net::CookieAccessSemantics cookie_access_semantics =
      net::CookieAccessSemantics::LEGACY;

  // Using `UNKNOWN` semantics to allow the experiment to switch between non
  // legacy (where cookies that don't have a specific same-site access policy
  // and not secure will not be included), and legacy mode.
  cookie_access_semantics = net::CookieAccessSemantics::UNKNOWN;

  // No extra trustworthy URLs.
  bool delegate_treats_url_as_trustworthy = false;
  net::CookieAccessParams params = {cookie_access_semantics,
                                    delegate_treats_url_as_trustworthy};
  return canonical_cookie->IncludeForRequestURL(url, options, params)
      .status.IsInclude();
}

// Helper method that insert a cookie in `weak_creation_time_manager`
// while ensuring the time is unique.
void SetCreationTimeEnsureUnique(
    base::WeakPtr<net::CookieCreationTimeManager> weak_creation_time_manager,
    NSHTTPCookie* cookie,
    base::Time creation_time) {
  if (net::CookieCreationTimeManager* creation_time_manager =
          weak_creation_time_manager.get()) {
    creation_time_manager->SetCreationTime(
        cookie, creation_time_manager->MakeUniqueCreationTime(creation_time));
  }
}

// Returns a closure that invokes `one` and then `two` unconditionally. If `two`
// is null, then returns `one`.
base::OnceClosure ChainClosure(base::OnceClosure one, base::OnceClosure two) {
  DCHECK(!one.is_null());
  if (two.is_null()) {
    return one;
  }

  return base::BindOnce(
      [](base::OnceClosure one, base::OnceClosure two) {
        std::move(one).Run();
        std::move(two).Run();
      },
      std::move(one), std::move(two));
}

}  // namespace

#pragma mark - SystemCookieStore::Helper

// Class wrapping a WKHTTPCookieStore and providing C++ based API to
// sends requests while dealing with the fact that WKHTTPCookieStore
// is only accessible on the UI thread while WKHTTPSystemCookieStore
// lives on the IO thread.
//
// This object uses scoped_refptr<base::SequencedTaskRunner> to keep
// references to the thread's TaskRunners. This allow to try to post
// tasks between threads even during shutdown (the PostTask will then
// fail but this won't crash).
class WKHTTPSystemCookieStore::Helper {
 public:
  explicit Helper(WKHTTPCookieStore* cookie_store);

  // Type of the callbacks used by the different methods.
  using DeleteCookieCallback = base::OnceCallback<void()>;
  using InsertCookieCallback = base::OnceCallback<void()>;
  using ClearCookiesCallback = base::OnceCallback<void()>;
  using FetchCookiesCallback =
      base::OnceCallback<void(NSArray<NSHTTPCookie*>*)>;

  // Deletes `cookie` from the WKHTTPCookieStore and invokes `callback` on
  // the IO thread when the operation completes.
  void DeleteCookie(NSHTTPCookie* cookie, DeleteCookieCallback callback);

  // Inserts `cookie` into the WKHTTPCookieStore and invokes `callback` on
  // the IO thread when the operation completes.
  void InsertCookie(NSHTTPCookie* cookie, InsertCookieCallback callback);

  // Clears all cookies from the WKHTTPCookieStore and invokes `callback`
  // on the IO thread when the operation completes.
  void ClearCookies(ClearCookiesCallback callback);

  // Fetches all cookies from the WKHTTPCookieStore and invokes `callback`
  // with them on the IO thread when the operation completes. If the store
  // is deleted, the callback will still be invoked with an empty array.
  void FetchCookies(FetchCookiesCallback callback);

  void SetCookieStore(WKHTTPCookieStore* cookie_store);

 private:
  __strong CRWWKHTTPCookieStore* crw_cookie_store_ = nil;
  scoped_refptr<base::SequencedTaskRunner> ui_task_runner_;
  scoped_refptr<base::SequencedTaskRunner> io_task_runner_;
};

WKHTTPSystemCookieStore::Helper::Helper(WKHTTPCookieStore* cookie_store)
    : ui_task_runner_(web::GetUIThreadTaskRunner({})),
      io_task_runner_(web::GetIOThreadTaskRunner({})) {
  crw_cookie_store_ = [[CRWWKHTTPCookieStore alloc] init];
  crw_cookie_store_.HTTPCookieStore = cookie_store;
}

void WKHTTPSystemCookieStore::Helper::DeleteCookie(
    NSHTTPCookie* cookie,
    DeleteCookieCallback callback) {
  // Convert the callback to a block and ensure it is invoked on the IO thread.
  void (^completion)() = base::CallbackToBlock(
      base::BindPostTask(io_task_runner_, std::move(callback)));

  __weak CRWWKHTTPCookieStore* weak_cookie_store = crw_cookie_store_;
  ui_task_runner_->PostTask(FROM_HERE, base::BindOnce(^{
                              [weak_cookie_store deleteCookie:cookie
                                            completionHandler:completion];
                            }));
}

void WKHTTPSystemCookieStore::Helper::InsertCookie(
    NSHTTPCookie* cookie,
    InsertCookieCallback callback) {
  // Convert the callback to a block and ensure it is invoked on the IO thread.
  void (^completion)() = base::CallbackToBlock(
      base::BindPostTask(io_task_runner_, std::move(callback)));

  __weak CRWWKHTTPCookieStore* weak_cookie_store = crw_cookie_store_;
  ui_task_runner_->PostTask(FROM_HERE, base::BindOnce(^{
                              [weak_cookie_store setCookie:cookie
                                         completionHandler:completion];
                            }));
}

void WKHTTPSystemCookieStore::Helper::ClearCookies(
    ClearCookiesCallback callback) {
  // Convert the callback to a block and ensure it is invoked on the IO thread.
  void (^completion)() = base::CallbackToBlock(
      base::BindPostTask(io_task_runner_, std::move(callback)));

  __weak CRWWKHTTPCookieStore* weak_cookie_store = crw_cookie_store_;
  ui_task_runner_->PostTask(FROM_HERE, base::BindOnce(^{
                              [weak_cookie_store clearCookies:completion];
                            }));
}

void WKHTTPSystemCookieStore::Helper::FetchCookies(
    FetchCookiesCallback callback) {
  // Convert the callback to a block and ensure it is invoked on the IO thread.
  void (^completion)(NSArray<NSHTTPCookie*>*) = base::CallbackToBlock(
      base::BindPostTask(io_task_runner_, std::move(callback)));

  __weak CRWWKHTTPCookieStore* weak_cookie_store = crw_cookie_store_;
  ui_task_runner_->PostTask(FROM_HERE, base::BindOnce(^{
                              if (weak_cookie_store) {
                                [weak_cookie_store getAllCookies:completion];
                              } else {
                                // If the store is nil, return an empty list.
                                completion(@[]);
                              }
                            }));
}

void WKHTTPSystemCookieStore::Helper::SetCookieStore(
    WKHTTPCookieStore* cookie_store) {
  crw_cookie_store_.HTTPCookieStore = cookie_store;
}

#pragma mark - SystemCookieStore

WKHTTPSystemCookieStore::WKHTTPSystemCookieStore(
    WKWebViewConfigurationProvider* config_provider) {
  helper_ = std::make_unique<Helper>(config_provider->GetWebViewConfiguration()
                                         .websiteDataStore.httpCookieStore);

  config_provider->AddObserver(this);
}

WKHTTPSystemCookieStore::~WKHTTPSystemCookieStore() = default;

void WKHTTPSystemCookieStore::GetCookiesForURLAsync(
    const GURL& url,
    SystemCookieCallbackForCookies callback) {
  helper_->FetchCookies(
      base::BindOnce(&WKHTTPSystemCookieStore::FilterAndSortCookies,
                     creation_time_manager_->GetWeakPtr(), url)
          .Then(std::move(callback)));
}

void WKHTTPSystemCookieStore::GetAllCookiesAsync(
    SystemCookieCallbackForCookies callback) {
  GetCookiesForURLAsync(GURL(), std::move(callback));
}

void WKHTTPSystemCookieStore::DeleteCookieAsync(NSHTTPCookie* cookie,
                                                SystemCookieCallback callback) {
  base::OnceClosure closure =
      base::BindOnce(&net::CookieCreationTimeManager::DeleteCreationTime,
                     creation_time_manager_->GetWeakPtr(), cookie);

  helper_->DeleteCookie(cookie,
                        ChainClosure(std::move(closure), std::move(callback)));
}

void WKHTTPSystemCookieStore::SetCookieAsync(
    NSHTTPCookie* cookie,
    const base::Time* optional_creation_time,
    SystemCookieCallback callback) {
  const base::Time creation_time =
      optional_creation_time ? *optional_creation_time : base::Time::Now();

  base::OnceClosure closure = base::BindOnce(
      &SetCreationTimeEnsureUnique, creation_time_manager_->GetWeakPtr(),
      cookie, creation_time);

  helper_->InsertCookie(cookie,
                        ChainClosure(std::move(closure), std::move(callback)));
}

void WKHTTPSystemCookieStore::ClearStoreAsync(SystemCookieCallback callback) {
  base::OnceClosure closure =
      base::BindOnce(&net::CookieCreationTimeManager::Clear,
                     creation_time_manager_->GetWeakPtr());

  helper_->ClearCookies(ChainClosure(std::move(closure), std::move(callback)));
}

NSHTTPCookieAcceptPolicy WKHTTPSystemCookieStore::GetCookieAcceptPolicy() {
  // TODO(crbug.com/41341295): Make sure there is no other way to return
  // WKHTTPCookieStore Specific cookieAcceptPolicy.
  return [[NSHTTPCookieStorage sharedHTTPCookieStorage] cookieAcceptPolicy];
}

#pragma mark WKWebViewConfigurationProviderObserver implementation

void WKHTTPSystemCookieStore::DidCreateNewConfiguration(
    WKWebViewConfigurationProvider* provider,
    WKWebViewConfiguration* new_config) {
  helper_->SetCookieStore(new_config.websiteDataStore.httpCookieStore);
}

#pragma mark private methods

// static
NSArray<NSHTTPCookie*>* WKHTTPSystemCookieStore::FilterAndSortCookies(
    base::WeakPtr<net::CookieCreationTimeManager> weak_time_manager,
    const GURL& include_url,
    NSArray<NSHTTPCookie*>* cookies) {
  if (include_url.is_valid()) {
    NSMutableArray<NSHTTPCookie*>* filtered_cookies =
        [[NSMutableArray alloc] initWithCapacity:cookies.count];

    for (NSHTTPCookie* cookie in cookies) {
      if (ShouldIncludeForRequestUrl(cookie, include_url)) {
        [filtered_cookies addObject:cookie];
      }
    }

    cookies = [filtered_cookies copy];
  }

  if (!weak_time_manager) {
    return cookies;
  }

  return
      [cookies sortedArrayUsingFunction:net::SystemCookieStore::CompareCookies
                                context:weak_time_manager.get()];
}

}  // namespace web
