// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_auth_cache.h"

#include <list>
#include <map>

#include "base/logging.h"
#include "base/memory/raw_ref.h"
#include "base/metrics/histogram_macros.h"
#include "base/not_fatal_until.h"
#include "base/strings/string_util.h"
#include "url/gurl.h"
#include "url/scheme_host_port.h"
#include "url/url_constants.h"

namespace {

// Helper to find the containing directory of path. In RFC 2617 this is what
// they call the "last symbolic element in the absolute path".
// Examples:
//   "/foo/bar.txt" --> "/foo/"
//   "/foo/" --> "/foo/"
std::string GetParentDirectory(const std::string& path) {
  std::string::size_type last_slash = path.rfind("/");
  if (last_slash == std::string::npos) {
    // No slash (absolute paths always start with slash, so this must be
    // the proxy case which uses empty string).
    DCHECK(path.empty());
    return path;
  }
  return path.substr(0, last_slash + 1);
}

// Return true if |path| is a subpath of |container|. In other words, is
// |container| an ancestor of |path|?
bool IsEnclosingPath(const std::string& container, const std::string& path) {
  DCHECK(container.empty() || *(container.end() - 1) == '/');
  return ((container.empty() && path.empty()) ||
          (!container.empty() && path.starts_with(container)));
}

#if DCHECK_IS_ON()
// Debug helper to check that |scheme_host_port| arguments are properly formed.
void CheckSchemeHostPortIsValid(const url::SchemeHostPort& scheme_host_port) {
  DCHECK(scheme_host_port.IsValid());
  DCHECK(scheme_host_port.scheme() == url::kHttpScheme ||
         scheme_host_port.scheme() == url::kHttpsScheme ||
         scheme_host_port.scheme() == url::kWsScheme ||
         scheme_host_port.scheme() == url::kWssScheme);
}

// Debug helper to check that |path| arguments are properly formed.
// (should be absolute path, or empty string).
void CheckPathIsValid(const std::string& path) {
  DCHECK(path.empty() || path[0] == '/');
}
#endif

// Functor used by std::erase_if.
struct IsEnclosedBy {
  explicit IsEnclosedBy(const std::string& path) : path(path) { }
  bool operator() (const std::string& x) const {
    return IsEnclosingPath(*path, x);
  }
  const raw_ref<const std::string> path;
};

}  // namespace

namespace net {

HttpAuthCache::HttpAuthCache(
    bool key_server_entries_by_network_anonymization_key)
    : key_server_entries_by_network_anonymization_key_(
          key_server_entries_by_network_anonymization_key) {}

HttpAuthCache::~HttpAuthCache() = default;

void HttpAuthCache::SetKeyServerEntriesByNetworkAnonymizationKey(
    bool key_server_entries_by_network_anonymization_key) {
  if (key_server_entries_by_network_anonymization_key_ ==
      key_server_entries_by_network_anonymization_key) {
    return;
  }

  key_server_entries_by_network_anonymization_key_ =
      key_server_entries_by_network_anonymization_key;
  std::erase_if(entries_, [](const EntryMap::value_type& entry_map_pair) {
    return entry_map_pair.first.target == HttpAuth::AUTH_SERVER;
  });
}

// Performance: O(logN+n), where N is the total number of entries, n is the
// number of realm entries for the given SchemeHostPort, target, and with a
// matching NetworkAnonymizationKey.
HttpAuthCache::Entry* HttpAuthCache::Lookup(
    const url::SchemeHostPort& scheme_host_port,
    HttpAuth::Target target,
    const std::string& realm,
    HttpAuth::Scheme scheme,
    const NetworkAnonymizationKey& network_anonymization_key) {
  EntryMap::iterator entry_it = LookupEntryIt(
      scheme_host_port, target, realm, scheme, network_anonymization_key);
  if (entry_it == entries_.end())
    return nullptr;
  return &(entry_it->second);
}

// Performance: O(logN+n*m), where N is the total number of entries, n is the
// number of realm entries for the given SchemeHostPort, target, and
// NetworkAnonymizationKey, m is the number of path entries per realm. Both n
// and m are expected to be small; m is kept small because AddPath() only keeps
// the shallowest entry.
HttpAuthCache::Entry* HttpAuthCache::LookupByPath(
    const url::SchemeHostPort& scheme_host_port,
    HttpAuth::Target target,
    const NetworkAnonymizationKey& network_anonymization_key,
    const std::string& path) {
#if DCHECK_IS_ON()
  CheckSchemeHostPortIsValid(scheme_host_port);
  CheckPathIsValid(path);
#endif

  // RFC 2617 section 2:
  // A client SHOULD assume that all paths at or deeper than the depth of
  // the last symbolic element in the path field of the Request-URI also are
  // within the protection space ...
  std::string parent_dir = GetParentDirectory(path);

  // Linear scan through the <scheme, realm> entries for the given
  // SchemeHostPort.
  auto entry_range = entries_.equal_range(
      EntryMapKey(scheme_host_port, target, network_anonymization_key,
                  key_server_entries_by_network_anonymization_key_));
  auto best_match_it = entries_.end();
  size_t best_match_length = 0;
  for (auto it = entry_range.first; it != entry_range.second; ++it) {
    size_t len = 0;
    auto& entry = it->second;
    DCHECK(entry.scheme_host_port() == scheme_host_port);
    if (entry.HasEnclosingPath(parent_dir, &len) &&
        (best_match_it == entries_.end() || len > best_match_length)) {
      best_match_it = it;
      best_match_length = len;
    }
  }
  if (best_match_it != entries_.end()) {
    Entry& best_match_entry = best_match_it->second;
    best_match_entry.last_use_time_ticks_ = tick_clock_->NowTicks();
    return &best_match_entry;
  }
  return nullptr;
}

HttpAuthCache::Entry* HttpAuthCache::Add(
    const url::SchemeHostPort& scheme_host_port,
    HttpAuth::Target target,
    const std::string& realm,
    HttpAuth::Scheme scheme,
    const NetworkAnonymizationKey& network_anonymization_key,
    const std::string& auth_challenge,
    const AuthCredentials& credentials,
    const std::string& path) {
#if DCHECK_IS_ON()
  CheckSchemeHostPortIsValid(scheme_host_port);
  CheckPathIsValid(path);
#endif

  base::TimeTicks now_ticks = tick_clock_->NowTicks();

  // Check for existing entry (we will re-use it if present).
  HttpAuthCache::Entry* entry = Lookup(scheme_host_port, target, realm, scheme,
                                       network_anonymization_key);
  if (!entry) {
    // Failsafe to prevent unbounded memory growth of the cache.
    //
    // Data was collected in June of 2019, before entries were keyed on either
    // HttpAuth::Target or NetworkAnonymizationKey. That data indicated that the
    // eviction rate was at around 0.05%. I.e. 0.05% of the time the number of
    // entries in the cache exceed kMaxNumRealmEntries. The evicted entry is
    // roughly half an hour old (median), and it's been around 25 minutes since
    // its last use (median).
    if (entries_.size() >= kMaxNumRealmEntries) {
      DLOG(WARNING) << "Num auth cache entries reached limit -- evicting";
      EvictLeastRecentlyUsedEntry();
    }
    entry =
        &(entries_
              .insert({EntryMapKey(
                           scheme_host_port, target, network_anonymization_key,
                           key_server_entries_by_network_anonymization_key_),
                       Entry()})
              ->second);
    entry->scheme_host_port_ = scheme_host_port;
    entry->realm_ = realm;
    entry->scheme_ = scheme;
    entry->creation_time_ticks_ = now_ticks;
    entry->creation_time_ = clock_->Now();
  }
  DCHECK_EQ(scheme_host_port, entry->scheme_host_port_);
  DCHECK_EQ(realm, entry->realm_);
  DCHECK_EQ(scheme, entry->scheme_);

  entry->auth_challenge_ = auth_challenge;
  entry->credentials_ = credentials;
  entry->nonce_count_ = 1;
  entry->AddPath(path);
  entry->last_use_time_ticks_ = now_ticks;

  return entry;
}

HttpAuthCache::Entry::Entry(const Entry& other) = default;

HttpAuthCache::Entry::~Entry() = default;

void HttpAuthCache::Entry::UpdateStaleChallenge(
    const std::string& auth_challenge) {
  auth_challenge_ = auth_challenge;
  nonce_count_ = 1;
}

bool HttpAuthCache::Entry::IsEqualForTesting(const Entry& other) const {
  if (scheme_host_port() != other.scheme_host_port())
    return false;
  if (realm() != other.realm())
    return false;
  if (scheme() != other.scheme())
    return false;
  if (auth_challenge() != other.auth_challenge())
    return false;
  if (!credentials().Equals(other.credentials()))
    return false;
  std::set<std::string> lhs_paths(paths_.begin(), paths_.end());
  std::set<std::string> rhs_paths(other.paths_.begin(), other.paths_.end());
  if (lhs_paths != rhs_paths)
    return false;
  return true;
}

HttpAuthCache::Entry::Entry() = default;

void HttpAuthCache::Entry::AddPath(const std::string& path) {
  std::string parent_dir = GetParentDirectory(path);
  if (!HasEnclosingPath(parent_dir, nullptr)) {
    // Remove any entries that have been subsumed by the new entry.
    std::erase_if(paths_, IsEnclosedBy(parent_dir));

    // Failsafe to prevent unbounded memory growth of the cache.
    //
    // Data collected on June of 2019 indicate that when we get here, the list
    // of paths has reached the 10 entry maximum around 1% of the time.
    if (paths_.size() >= kMaxNumPathsPerRealmEntry) {
      DLOG(WARNING) << "Num path entries for " << scheme_host_port()
                    << " has grown too large -- evicting";
      paths_.pop_back();
    }

    // Add new path.
    paths_.push_front(parent_dir);
  }
}

bool HttpAuthCache::Entry::HasEnclosingPath(const std::string& dir,
                                            size_t* path_len) {
  DCHECK(GetParentDirectory(dir) == dir);
  for (PathList::iterator it = paths_.begin(); it != paths_.end(); ++it) {
    if (IsEnclosingPath(*it, dir)) {
      // No element of paths_ may enclose any other element.
      // Therefore this path is the tightest bound.  Important because
      // the length returned is used to determine the cache entry that
      // has the closest enclosing path in LookupByPath().
      if (path_len)
        *path_len = it->length();
      // Move the found path up by one place so that more frequently used paths
      // migrate towards the beginning of the list of paths.
      if (it != paths_.begin())
        std::iter_swap(it, std::prev(it));
      return true;
    }
  }
  return false;
}

bool HttpAuthCache::Remove(
    const url::SchemeHostPort& scheme_host_port,
    HttpAuth::Target target,
    const std::string& realm,
    HttpAuth::Scheme scheme,
    const NetworkAnonymizationKey& network_anonymization_key,
    const AuthCredentials& credentials) {
  EntryMap::iterator entry_it = LookupEntryIt(
      scheme_host_port, target, realm, scheme, network_anonymization_key);
  if (entry_it == entries_.end())
    return false;
  Entry& entry = entry_it->second;
  if (credentials.Equals(entry.credentials())) {
    entries_.erase(entry_it);
    return true;
  }
  return false;
}

void HttpAuthCache::ClearEntriesAddedBetween(
    base::Time begin_time,
    base::Time end_time,
    base::RepeatingCallback<bool(const GURL&)> url_matcher) {
  if (begin_time.is_min() && end_time.is_max() && !url_matcher) {
    ClearAllEntries();
    return;
  }
  std::erase_if(entries_, [begin_time, end_time, url_matcher](
                              const EntryMap::value_type& entry_map_pair) {
    const Entry& entry = entry_map_pair.second;
    return entry.creation_time_ >= begin_time &&
           entry.creation_time_ < end_time &&
           (url_matcher ? url_matcher.Run(entry.scheme_host_port().GetURL())
                        : true);
  });
}

void HttpAuthCache::ClearAllEntries() {
  entries_.clear();
}

bool HttpAuthCache::UpdateStaleChallenge(
    const url::SchemeHostPort& scheme_host_port,
    HttpAuth::Target target,
    const std::string& realm,
    HttpAuth::Scheme scheme,
    const NetworkAnonymizationKey& network_anonymization_key,
    const std::string& auth_challenge) {
  HttpAuthCache::Entry* entry = Lookup(scheme_host_port, target, realm, scheme,
                                       network_anonymization_key);
  if (!entry)
    return false;
  entry->UpdateStaleChallenge(auth_challenge);
  entry->last_use_time_ticks_ = tick_clock_->NowTicks();
  return true;
}

void HttpAuthCache::CopyProxyEntriesFrom(const HttpAuthCache& other) {
  for (auto it = other.entries_.begin(); it != other.entries_.end(); ++it) {
    const Entry& e = it->second;

    // Skip non-proxy entries.
    if (it->first.target != HttpAuth::AUTH_PROXY)
      continue;

    // Sanity check - proxy entries should have an empty
    // NetworkAnonymizationKey.
    DCHECK(NetworkAnonymizationKey() == it->first.network_anonymization_key);

    // Add an Entry with one of the original entry's paths.
    DCHECK(e.paths_.size() > 0);
    Entry* entry = Add(e.scheme_host_port(), it->first.target, e.realm(),
                       e.scheme(), it->first.network_anonymization_key,
                       e.auth_challenge(), e.credentials(), e.paths_.back());
    // Copy all other paths.
    for (auto it2 = std::next(e.paths_.rbegin()); it2 != e.paths_.rend(); ++it2)
      entry->AddPath(*it2);
    // Copy nonce count (for digest authentication).
    entry->nonce_count_ = e.nonce_count_;
  }
}

HttpAuthCache::EntryMapKey::EntryMapKey(
    const url::SchemeHostPort& scheme_host_port,
    HttpAuth::Target target,
    const NetworkAnonymizationKey& network_anonymization_key,
    bool key_server_entries_by_network_anonymization_key)
    : scheme_host_port(scheme_host_port),
      target(target),
      network_anonymization_key(
          target == HttpAuth::AUTH_SERVER &&
                  key_server_entries_by_network_anonymization_key
              ? network_anonymization_key
              : NetworkAnonymizationKey()) {}

HttpAuthCache::EntryMapKey::~EntryMapKey() = default;

bool HttpAuthCache::EntryMapKey::operator<(const EntryMapKey& other) const {
  return std::tie(scheme_host_port, target, network_anonymization_key) <
         std::tie(other.scheme_host_port, other.target,
                  other.network_anonymization_key);
}

size_t HttpAuthCache::GetEntriesSizeForTesting() {
  return entries_.size();
}

HttpAuthCache::EntryMap::iterator HttpAuthCache::LookupEntryIt(
    const url::SchemeHostPort& scheme_host_port,
    HttpAuth::Target target,
    const std::string& realm,
    HttpAuth::Scheme scheme,
    const NetworkAnonymizationKey& network_anonymization_key) {
#if DCHECK_IS_ON()
  CheckSchemeHostPortIsValid(scheme_host_port);
#endif

  // Linear scan through the <scheme, realm> entries for the given
  // SchemeHostPort and NetworkAnonymizationKey.
  auto entry_range = entries_.equal_range(
      EntryMapKey(scheme_host_port, target, network_anonymization_key,
                  key_server_entries_by_network_anonymization_key_));
  for (auto it = entry_range.first; it != entry_range.second; ++it) {
    Entry& entry = it->second;
    DCHECK(entry.scheme_host_port() == scheme_host_port);
    if (entry.scheme() == scheme && entry.realm() == realm) {
      entry.last_use_time_ticks_ = tick_clock_->NowTicks();
      return it;
    }
  }
  return entries_.end();
}

// Linear scan through all entries to find least recently used entry (by oldest
// |last_use_time_ticks_| and evict it from |entries_|.
void HttpAuthCache::EvictLeastRecentlyUsedEntry() {
  DCHECK(entries_.size() == kMaxNumRealmEntries);
  base::TimeTicks now_ticks = tick_clock_->NowTicks();

  EntryMap::iterator oldest_entry_it = entries_.end();
  base::TimeTicks oldest_last_use_time_ticks = now_ticks;

  for (auto it = entries_.begin(); it != entries_.end(); ++it) {
    Entry& entry = it->second;
    if (entry.last_use_time_ticks_ < oldest_last_use_time_ticks ||
        oldest_entry_it == entries_.end()) {
      oldest_entry_it = it;
      oldest_last_use_time_ticks = entry.last_use_time_ticks_;
    }
  }
  CHECK(oldest_entry_it != entries_.end(), base::NotFatalUntil::M130);
  entries_.erase(oldest_entry_it);
}

}  // namespace net
