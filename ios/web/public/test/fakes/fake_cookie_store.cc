// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/public/test/fakes/fake_cookie_store.h"

#include "base/task/post_task.h"
#include "ios/web/public/thread/web_task_traits.h"
#include "ios/web/public/thread/web_thread.h"

namespace web {

FakeCookieStore::FakeCookieStore() {}

FakeCookieStore::~FakeCookieStore() {}

void FakeCookieStore::SetAllCookies(const net::CookieList& all_cookies) {
  all_cookies_ = all_cookies;
}

void FakeCookieStore::GetAllCookiesAsync(GetAllCookiesCallback callback) {
  DCHECK_CURRENTLY_ON(WebThread::IO);
  base::PostTask(FROM_HERE, {WebThread::IO},
                 base::BindOnce(std::move(callback), all_cookies_));
}

void FakeCookieStore::SetCanonicalCookieAsync(
    std::unique_ptr<net::CanonicalCookie> cookie,
    std::string source_scheme,
    const net::CookieOptions& options,
    SetCookiesCallback callback) {
  NOTIMPLEMENTED() << "Implement this if necessary.";
}

void FakeCookieStore::GetCookieListWithOptionsAsync(
    const GURL& url,
    const net::CookieOptions& options,
    GetCookieListCallback callback) {
  NOTIMPLEMENTED() << "Implement this if necessary.";
}

void FakeCookieStore::DeleteCanonicalCookieAsync(
    const net::CanonicalCookie& cookie,
    DeleteCallback callback) {
  NOTIMPLEMENTED() << "Implement this if necessary.";
}

void FakeCookieStore::DeleteAllCreatedInTimeRangeAsync(
    const net::CookieDeletionInfo::TimeRange& creation_range,
    DeleteCallback callback) {
  NOTIMPLEMENTED() << "Implement this if necessary.";
}

void FakeCookieStore::DeleteAllMatchingInfoAsync(
    net::CookieDeletionInfo delete_info,
    DeleteCallback callback) {
  NOTIMPLEMENTED() << "Implement this if necessary.";
}

void FakeCookieStore::DeleteSessionCookiesAsync(DeleteCallback) {
  NOTIMPLEMENTED() << "Implement this if necessary.";
}

void FakeCookieStore::FlushStore(base::OnceClosure callback) {
  NOTIMPLEMENTED() << "Implement this if necessary.";
}

void FakeCookieStore::SetCookieableSchemes(
    const std::vector<std::string>& schemes,
    SetCookieableSchemesCallback callback) {
  NOTIMPLEMENTED() << "Implement this if necessary.";
}

net::CookieChangeDispatcher& FakeCookieStore::GetChangeDispatcher() {
  // This is NOTREACHED not NOTIMPLEMENTED because it would likely cause a weird
  // SEGV in the next line anyways. Crashing here with a more friendly error
  // message is preferred.
  NOTREACHED() << "Not implemented. Implement this if necessary.";
  // Perform a crazy thing here just to make the compiler happy. It doesn't
  // matter because it should never reach here.
  return *reinterpret_cast<net::CookieChangeDispatcher*>(this);
}

}  // namespace web
