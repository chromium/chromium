// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/session_cleanup_cookie_store.h"

#include <list>
#include <memory>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/task/sequenced_task_runner.h"
#include "base/values.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_constants.h"
#include "net/cookies/cookie_util.h"
#include "net/extras/sqlite/cookie_crypto_delegate.h"
#include "net/log/net_log.h"
#include "url/gurl.h"

namespace network {

namespace {

base::Value::Dict CookieStoreOriginFiltered(
    const std::string& origin,
    bool is_https,
    net::NetLogCaptureMode capture_mode) {
  if (!net::NetLogCaptureIncludesSensitive(capture_mode)) {
    return base::Value::Dict();
  }
  return base::Value::Dict().Set("origin", origin).Set("is_https", is_https);
}

}  // namespace

SessionCleanupCookieStore::SessionCleanupCookieStore(
    const scoped_refptr<net::SQLitePersistentCookieStore>& cookie_store)
    : persistent_store_(cookie_store) {}

SessionCleanupCookieStore::~SessionCleanupCookieStore() {
  net_log_.AddEventWithStringParams(
      net::NetLogEventType::COOKIE_PERSISTENT_STORE_CLOSED, "type",
      "SessionCleanupCookieStore");
}

void SessionCleanupCookieStore::DeleteSessionCookies(
    DeleteCookiePredicate delete_cookie_predicate) {
  using CookieOrigin = net::SQLitePersistentCookieStore::CookieOrigin;
  if (force_keep_session_state_ || !delete_cookie_predicate)
    return;

  std::list<CookieOrigin> session_only_cookies;
  for (const auto& entry : cookies_per_origin_) {
    if (entry.second == 0) {
      continue;
    }
    const CookieOrigin& cookie = entry.first;
    const GURL url(
        net::cookie_util::CookieOriginToURL(cookie.first, cookie.second));
    if (!url.is_valid() ||
        !delete_cookie_predicate.Run(
            cookie.first, cookie.second
                              ? net::CookieSourceScheme::kSecure
                              : net::CookieSourceScheme::kNonSecure)) {
      continue;
    }
    net_log_.AddEvent(
        net::NetLogEventType::COOKIE_PERSISTENT_STORE_ORIGIN_FILTERED,
        [&](net::NetLogCaptureMode capture_mode) {
          return CookieStoreOriginFiltered(cookie.first, cookie.second,
                                           capture_mode);
        });
    session_only_cookies.push_back(cookie);
  }

  persistent_store_->DeleteAllInList(session_only_cookies);
}

void SessionCleanupCookieStore::Load(LoadedCallback loaded_callback,
                                     const net::NetLogWithSource& net_log) {
  net_log_ = net_log;
  persistent_store_->Load(base::BindOnce(&SessionCleanupCookieStore::OnLoad,
                                         this, std::move(loaded_callback)),
                          net_log);
}

void SessionCleanupCookieStore::LoadCookiesForKey(
    const std::string& key,
    LoadedCallback loaded_callback) {
  persistent_store_->LoadCookiesForKey(
      key, base::BindOnce(&SessionCleanupCookieStore::OnLoad, this,
                          std::move(loaded_callback)));
}

void SessionCleanupCookieStore::AddCookie(const net::CanonicalCookie& cc) {
  net::SQLitePersistentCookieStore::CookieOrigin origin(
      cc.Domain(), cc.SourceScheme() == net::CookieSourceScheme::kSecure);
  ++cookies_per_origin_[origin];
  persistent_store_->AddCookie(cc);
}

void SessionCleanupCookieStore::UpdateCookieAccessTime(
    const net::CanonicalCookie& cc) {
  persistent_store_->UpdateCookieAccessTime(cc);
}

void SessionCleanupCookieStore::DeleteCookie(const net::CanonicalCookie& cc) {
  net::SQLitePersistentCookieStore::CookieOrigin origin(
      cc.Domain(), cc.SourceScheme() == net::CookieSourceScheme::kSecure);
  DCHECK_GE(cookies_per_origin_[origin], 1U);
  --cookies_per_origin_[origin];
  persistent_store_->DeleteCookie(cc);
}

void SessionCleanupCookieStore::SetForceKeepSessionState() {
  force_keep_session_state_ = true;
}

void SessionCleanupCookieStore::SetBeforeCommitCallback(
    base::RepeatingClosure callback) {
  persistent_store_->SetBeforeCommitCallback(std::move(callback));
}

void SessionCleanupCookieStore::Flush(base::OnceClosure callback) {
  persistent_store_->Flush(std::move(callback));
}

void SessionCleanupCookieStore::OnLoad(
    LoadedCallback loaded_callback,
    std::vector<std::unique_ptr<net::CanonicalCookie>> cookies) {
  for (const auto& cookie : cookies) {
    net::SQLitePersistentCookieStore::CookieOrigin origin(
        cookie->Domain(),
        cookie->SourceScheme() == net::CookieSourceScheme::kSecure);
    ++cookies_per_origin_[origin];
  }

  std::move(loaded_callback).Run(std::move(cookies));
}

}  // namespace network
