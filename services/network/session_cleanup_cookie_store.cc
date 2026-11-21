// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/session_cleanup_cookie_store.h"

#include <list>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/task/sequenced_task_runner.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_constants.h"
#include "net/cookies/cookie_partition_key.h"
#include "net/cookies/cookie_util.h"
#include "net/extras/sqlite/cookie_crypto_delegate.h"
#include "net/log/net_log.h"
#include "services/network/public/cpp/features.h"
#include "url/gurl.h"

namespace network {

namespace {

base::DictValue CookieStoreOriginFiltered(const std::string& origin,
                                          bool is_https,
                                          net::NetLogCaptureMode capture_mode) {
  if (!net::NetLogCaptureIncludesSensitive(capture_mode)) {
    return base::DictValue();
  }
  return base::DictValue().Set("origin", origin).Set("is_https", is_https);
}

base::DictValue CookieStorePartitionFiltered(
    const std::string& top_level_site,
    net::NetLogCaptureMode capture_mode) {
  if (!net::NetLogCaptureIncludesSensitive(capture_mode)) {
    return base::DictValue();
  }
  return base::DictValue().Set("top_level_site", top_level_site);
}

// Returns the registrable domain (or host) and scheme of `site` in the form
// expected by a `DeleteCookiePredicate`, or std::nullopt if `site` has no host.
// The site is treated as a host (not a domain) so that it is matched against
// content settings the same way as a cookie set by the site's representative
// origin would be.
std::optional<std::pair<std::string, net::CookieSourceScheme>>
GetDeleteCookiePredicateArgsForSite(const net::SchemefulSite& site) {
  const GURL url = site.GetURL();
  if (!url.is_valid() || !url.has_host()) {
    return std::nullopt;
  }
  return std::make_pair(std::string(url.host()),
                        url.SchemeIsCryptographic()
                            ? net::CookieSourceScheme::kSecure
                            : net::CookieSourceScheme::kNonSecure);
}

}  // namespace

// static
bool SessionCleanupCookieStore::ShouldDeletePartitionedCookiesForTopLevelSite(
    const DeleteCookiePredicate& delete_cookie_predicate,
    const net::SchemefulSite& top_level_site) {
  std::optional<std::pair<std::string, net::CookieSourceScheme>> args =
      GetDeleteCookiePredicateArgsForSite(top_level_site);
  return args && delete_cookie_predicate.Run(args->first, args->second);
}

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

  std::list<std::string> session_only_partition_sites;
  if (base::FeatureList::IsEnabled(
          features::kClearPartitionedCookiesOnExitByTopLevelSite)) {
    for (const auto& [serialized_site, entry] : cookies_per_partition_site_) {
      if (!delete_cookie_predicate.Run(entry.registrable_domain_or_host,
                                       entry.scheme)) {
        continue;
      }
      net_log_.AddEvent(
          net::NetLogEventType::COOKIE_PERSISTENT_STORE_PARTITION_FILTERED,
          [&](net::NetLogCaptureMode capture_mode) {
            return CookieStorePartitionFiltered(serialized_site, capture_mode);
          });
      session_only_partition_sites.push_back(serialized_site);
    }
  }

  persistent_store_->DeleteAllInList(session_only_cookies,
                                     session_only_partition_sites);
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
  IncrementCookieCount(cc);
  persistent_store_->AddCookie(cc);
}

void SessionCleanupCookieStore::UpdateCookieAccessTime(
    const net::CanonicalCookie& cc) {
  persistent_store_->UpdateCookieAccessTime(cc);
}

void SessionCleanupCookieStore::DeleteCookie(const net::CanonicalCookie& cc) {
  DecrementCookieCount(cc);
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
    IncrementCookieCount(*cookie);
  }

  std::move(loaded_callback).Run(std::move(cookies));
}

void SessionCleanupCookieStore::IncrementCookieCount(
    const net::CanonicalCookie& cc) {
  net::SQLitePersistentCookieStore::CookieOrigin origin(
      cc.Domain(), cc.SourceScheme() == net::CookieSourceScheme::kSecure);
  ++cookies_per_origin_[origin];

  if (!cc.IsPartitioned()) {
    return;
  }
  // Cookies whose partition key cannot be serialized (e.g. nonced partition
  // keys) are not persisted, so there is nothing to track for them.
  base::expected<net::CookiePartitionKey::SerializedCookiePartitionKey,
                 std::string>
      serialized_key = net::CookiePartitionKey::Serialize(cc.PartitionKey());
  if (!serialized_key.has_value()) {
    return;
  }
  auto [it, inserted] =
      cookies_per_partition_site_.try_emplace(serialized_key->TopLevelSite());
  if (inserted) {
    std::optional<std::pair<std::string, net::CookieSourceScheme>> args =
        GetDeleteCookiePredicateArgsForSite(cc.PartitionKey()->site());
    if (!args) {
      // A site without a host can never match a `DeleteCookiePredicate`, so
      // there is no need to track it.
      cookies_per_partition_site_.erase(it);
      return;
    }
    it->second.registrable_domain_or_host = std::move(args->first);
    it->second.scheme = args->second;
  }
  ++it->second.count;
}

void SessionCleanupCookieStore::DecrementCookieCount(
    const net::CanonicalCookie& cc) {
  net::SQLitePersistentCookieStore::CookieOrigin origin(
      cc.Domain(), cc.SourceScheme() == net::CookieSourceScheme::kSecure);
  DCHECK_GE(cookies_per_origin_[origin], 1U);
  --cookies_per_origin_[origin];

  if (!cc.IsPartitioned()) {
    return;
  }
  base::expected<net::CookiePartitionKey::SerializedCookiePartitionKey,
                 std::string>
      serialized_key = net::CookiePartitionKey::Serialize(cc.PartitionKey());
  if (!serialized_key.has_value()) {
    return;
  }
  auto it = cookies_per_partition_site_.find(serialized_key->TopLevelSite());
  // Partition sites without a host are not tracked; see
  // IncrementCookieCount().
  if (it == cookies_per_partition_site_.end()) {
    return;
  }
  DCHECK_GE(it->second.count, 1U);
  if (--it->second.count == 0) {
    cookies_per_partition_site_.erase(it);
  }
}

}  // namespace network
