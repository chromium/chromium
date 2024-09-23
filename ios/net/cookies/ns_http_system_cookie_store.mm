// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/net/cookies/ns_http_system_cookie_store.h"

#include "base/functional/bind.h"
#include "base/time/time.h"
#import "ios/net/cookies/cookie_creation_time_manager.h"
#import "ios/net/cookies/cookie_store_ios_client.h"
#import "ios/net/cookies/system_cookie_util.h"
#import "net/base/apple/url_conversions.h"
#include "url/gurl.h"

namespace net {

// private
void RunCookieCallback(base::OnceClosure callback) {
  if (callback.is_null())
    return;
  CookieStoreIOSClient* client = net::GetCookieStoreIOSClient();
  auto sequenced_task_runner = client->GetTaskRunner();
  sequenced_task_runner->PostTask(FROM_HERE, std::move(callback));
}

NSHTTPSystemCookieStore::NSHTTPSystemCookieStore()
    : cookie_store_([NSHTTPCookieStorage sharedHTTPCookieStorage]) {}

NSHTTPSystemCookieStore::NSHTTPSystemCookieStore(
    NSHTTPCookieStorage* cookie_store)
    : cookie_store_(cookie_store) {}

NSHTTPSystemCookieStore::~NSHTTPSystemCookieStore() = default;

#pragma mark -
#pragma mark SystemCookieStore methods

void NSHTTPSystemCookieStore::GetCookiesForURLAsync(
    const GURL& url,
    SystemCookieCallbackForCookies callback) {
  NSArray* cookies = GetCookiesForURL(url);
  RunCookieCallback(base::BindOnce(std::move(callback), cookies));
}

void NSHTTPSystemCookieStore::GetAllCookiesAsync(
    SystemCookieCallbackForCookies callback) {
  NSArray* cookies = GetAllCookies();
  RunCookieCallback(base::BindOnce(std::move(callback), cookies));
}

void NSHTTPSystemCookieStore::DeleteCookieAsync(NSHTTPCookie* cookie,
                                                SystemCookieCallback callback) {
  DeleteCookie(cookie);
  RunCookieCallback(std::move(callback));
}

void NSHTTPSystemCookieStore::SetCookieAsync(
    NSHTTPCookie* cookie,
    const base::Time* optional_creation_time,
    SystemCookieCallback callback) {
  SetCookie(cookie, optional_creation_time);
  RunCookieCallback(std::move(callback));
}

void NSHTTPSystemCookieStore::ClearStoreAsync(SystemCookieCallback callback) {
  ClearStore();
  RunCookieCallback(std::move(callback));
}

NSHTTPCookieAcceptPolicy NSHTTPSystemCookieStore::GetCookieAcceptPolicy() {
  return [cookie_store_ cookieAcceptPolicy];
}

#pragma mark private methods

NSArray* NSHTTPSystemCookieStore::GetCookiesForURL(const GURL& url) {
  NSArray<NSHTTPCookie*>* cookies =
      [cookie_store_ cookiesForURL:NSURLWithGURL(url)];
  // Sort cookies by decreasing path length, then creation time, as per
  // RFC6265.
  return [cookies sortedArrayUsingFunction:CompareCookies
                                   context:creation_time_manager_.get()];
}

NSArray* NSHTTPSystemCookieStore::GetAllCookies() {
  NSArray<NSHTTPCookie*>* cookies = cookie_store_.cookies;
  return [cookies sortedArrayUsingFunction:CompareCookies
                                   context:creation_time_manager_.get()];
}

void NSHTTPSystemCookieStore::DeleteCookie(NSHTTPCookie* cookie) {
  [cookie_store_ deleteCookie:cookie];
  creation_time_manager_->DeleteCreationTime(cookie);
}

void NSHTTPSystemCookieStore::SetCookie(
    NSHTTPCookie* cookie,
    const base::Time* optional_creation_time) {
  [cookie_store_ setCookie:cookie];
  base::Time cookie_time = base::Time::Now();
  if (optional_creation_time && !optional_creation_time->is_null())
    cookie_time = *optional_creation_time;

  creation_time_manager_->SetCreationTime(
      cookie, creation_time_manager_->MakeUniqueCreationTime(cookie_time));
}

void NSHTTPSystemCookieStore::ClearStore() {
  NSArray* copy = [cookie_store_.cookies copy];
  for (NSHTTPCookie* cookie in copy)
    [cookie_store_ deleteCookie:cookie];
  DCHECK_EQ(0u, cookie_store_.cookies.count);
  creation_time_manager_->Clear();
}
}  // namespace net
