// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_AUTH_CACHE_H_
#define NET_HTTP_HTTP_AUTH_CACHE_H_

#include <stddef.h>

#include <list>
#include <map>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/time/default_clock.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "net/base/net_export.h"
#include "net/base/network_isolation_key.h"
#include "net/http/http_auth.h"
#include "url/gurl.h"

namespace net {

// HttpAuthCache stores HTTP authentication identities and challenge info.
// For each (origin, realm, scheme) triple the cache stores a
// HttpAuthCache::Entry, which holds:
//   - the origin server {protocol scheme, host, port}
//   - the last identity used (username/password)
//   - the last auth handler used (contains realm and authentication scheme)
//   - the list of paths which used this realm
// Entries can be looked up by either (origin, realm, scheme) or (origin, path).
class NET_EXPORT HttpAuthCache {
 public:
  class NET_EXPORT Entry {
   public:
    Entry(const Entry& other);
    ~Entry();

    const GURL& origin() const {
      return origin_;
    }

    // The case-sensitive realm string of the challenge.
    const std::string& realm() const { return realm_; }

    // The authentication scheme of the challenge.
    HttpAuth::Scheme scheme() const {
      return scheme_;
    }

    // The authentication challenge.
    const std::string& auth_challenge() const { return auth_challenge_; }

    // The login credentials.
    const AuthCredentials& credentials() const {
      return credentials_;
    }

    int IncrementNonceCount() {
      return ++nonce_count_;
    }

    void UpdateStaleChallenge(const std::string& auth_challenge);

    bool IsEqualForTesting(const Entry& other) const;

    bool operator==(const Entry& other) const = delete;

   private:
    friend class HttpAuthCache;
    FRIEND_TEST_ALL_PREFIXES(HttpAuthCacheTest, AddPath);
    FRIEND_TEST_ALL_PREFIXES(HttpAuthCacheTest, AddToExistingEntry);

    typedef std::list<std::string> PathList;

    Entry();

    // Adds a path defining the realm's protection space. If the path is
    // already contained in the protection space, is a no-op.
    void AddPath(const std::string& path);

    // Returns true if |dir| is contained within the realm's protection
    // space.  |*path_len| is set to the length of the enclosing path if
    // such a path exists and |path_len| is non-nullptr.  If no enclosing
    // path is found, |*path_len| is left unmodified.
    //
    // If an enclosing path is found, moves it up by one place in the paths list
    // so that more frequently used paths migrate to the front of the list.
    //
    // Note that proxy auth cache entries are associated with empty
    // paths.  Therefore it is possible for HasEnclosingPath() to return
    // true and set |*path_len| to 0.
    bool HasEnclosingPath(const std::string& dir, size_t* path_len);

    // |origin_| contains the {protocol, host, port} of the server.
    GURL origin_;
    std::string realm_;
    HttpAuth::Scheme scheme_;

    // Identity.
    std::string auth_challenge_;
    AuthCredentials credentials_;

    int nonce_count_;

    // List of paths that define the realm's protection space.
    PathList paths_;

    // Times the entry was created and last used (by looking up, adding a path,
    // or updating the challenge.)
    base::TimeTicks creation_time_ticks_;
    base::TimeTicks last_use_time_ticks_;
    base::Time creation_time_;
  };

  // Prevent unbounded memory growth. These are safeguards for abuse; it is
  // not expected that the limits will be reached in ordinary usage.
  // This also defines the worst-case lookup times (which grow linearly
  // with number of elements in the cache).
  enum { kMaxNumPathsPerRealmEntry = 10 };
  enum { kMaxNumRealmEntries = 20 };

  // If |key_server_entries_by_network_isolation_key| is true, all
  // HttpAuth::AUTH_SERVER operations are keyed by NetworkIsolationKey.
  // Otherwise, NetworkIsolationKey arguments are ignored.
  explicit HttpAuthCache(bool key_server_entries_by_network_isolation_key);
  ~HttpAuthCache();

  // Sets whether server entries are keyed by NetworkIsolationKey.
  // If this results in changing the value of the setting, all current server
  // entries are deleted.
  void SetKeyServerEntriesByNetworkIsolationKey(
      bool key_server_entries_by_network_isolation_key);

  // Find the realm entry on server |origin| for realm |realm| and
  // scheme |scheme|. If a matching entry is found, move it up by one place
  // in the entries list, so that more frequently used entries migrate to the
  // front of the list.
  //   |origin| - the {scheme, host, port} of the server.
  //   |target| - whether this is for server or proxy auth.
  //   |realm|  - case sensitive realm string.
  //   |scheme| - the authentication scheme (i.e. basic, negotiate).
  //   returns  - the matched entry or nullptr.
  Entry* Lookup(const GURL& origin,
                HttpAuth::Target target,
                const std::string& realm,
                HttpAuth::Scheme scheme,
                const NetworkIsolationKey& network_isolation_key);

  // Find the entry on server |origin| whose protection space includes
  // |path|. This uses the assumption in RFC 2617 section 2 that deeper
  // paths lie in the same protection space. If a matching entry is found, move
  // it up by one place in the entries list, so that more frequently used
  // entries migrate to the front of the list.
  //   |origin| - the {scheme, host, port} of the server.
  //   |path|   - absolute path of the resource, or empty string in case of
  //              proxy auth (which does not use the concept of paths).
  //   returns  - the matched entry or nullptr.
  Entry* LookupByPath(const GURL& origin,
                      HttpAuth::Target target,
                      const NetworkIsolationKey& network_isolation_key,
                      const std::string& path);

  // Add an entry on server |origin| for realm |handler->realm()| and
  // scheme |handler->scheme()|.  If an entry for this (realm,scheme)
  // already exists, update it rather than replace it -- this  preserves the
  // paths list.
  //   |origin|   - the {scheme, host, port} of the server.
  //   |realm|    - the auth realm for the challenge.
  //   |scheme|   - the authentication scheme (i.e. basic, negotiate).
  //   |credentials| - login information for the realm.
  //   |path|     - absolute path for a resource contained in the protection
  //                space; this will be added to the list of known paths.
  //   returns    - the entry that was just added/updated.
  Entry* Add(const GURL& origin,
             HttpAuth::Target target,
             const std::string& realm,
             HttpAuth::Scheme scheme,
             const NetworkIsolationKey& network_isolation_key,
             const std::string& auth_challenge,
             const AuthCredentials& credentials,
             const std::string& path);

  // Remove entry on server |origin| for realm |realm| and scheme |scheme|
  // if one exists AND if the cached credentials matches |credentials|.
  //   |origin|   - the {scheme, host, port} of the server.
  //   |realm|    - case sensitive realm string.
  //   |scheme|   - the authentication scheme (i.e. basic, negotiate).
  //   |credentials| - the credentials to match.
  //   returns    - true if an entry was removed.
  bool Remove(const GURL& origin,
              HttpAuth::Target target,
              const std::string& realm,
              HttpAuth::Scheme scheme,
              const NetworkIsolationKey& network_isolation_key,
              const AuthCredentials& credentials);

  // Clears cache entries added since |begin_time| or all entries if
  // |begin_time| is null.
  void ClearEntriesAddedSince(base::Time begin_time);

  // Clears all added entries.
  void ClearAllEntries();

  // Updates a stale digest entry on server |origin| for realm |realm| and
  // scheme |scheme|. The cached auth challenge is replaced with
  // |auth_challenge| and the nonce count is reset.
  // |UpdateStaleChallenge()| returns true if a matching entry exists in the
  // cache, false otherwise.
  bool UpdateStaleChallenge(const GURL& origin,
                            HttpAuth::Target target,
                            const std::string& realm,
                            HttpAuth::Scheme scheme,
                            const NetworkIsolationKey& network_isolation_key,
                            const std::string& auth_challenge);

  // Copies all entries from |other| cache with a target of
  // HttpAuth::AUTH_PROXY. |this| and |other| need not have the same
  // |key_server_entries_by_network_isolation_key_| value, since proxy
  // credentials are not keyed on NetworkIsolationKey.
  void CopyProxyEntriesFrom(const HttpAuthCache& other);

  size_t GetEntriesSizeForTesting();
  void set_tick_clock_for_testing(const base::TickClock* tick_clock) {
    tick_clock_ = tick_clock;
  }
  void set_clock_for_testing(const base::Clock* clock) { clock_ = clock; }

  bool key_server_entries_by_network_isolation_key() const {
    return key_server_entries_by_network_isolation_key_;
  }

 private:
  struct EntryMapKey {
    EntryMapKey(const GURL& url,
                HttpAuth::Target target,
                const NetworkIsolationKey& network_isolation_key,
                bool key_server_entries_by_network_isolation_key);
    ~EntryMapKey();

    bool operator<(const EntryMapKey& other) const;

    GURL url;
    HttpAuth::Target target;
    // Empty if |key_server_entries_by_network_isolation_key| is false, |target|
    // is HttpAuth::AUTH_PROXY, or an empty NetworkIsolationKey is passed in to
    // the EntryMap constructor.
    NetworkIsolationKey network_isolation_key;
  };

  using EntryMap = std::multimap<EntryMapKey, Entry>;

  const base::TickClock* tick_clock_ = base::DefaultTickClock::GetInstance();
  const base::Clock* clock_ = base::DefaultClock::GetInstance();

  EntryMap::iterator LookupEntryIt(
      const GURL& origin,
      HttpAuth::Target target,
      const std::string& realm,
      HttpAuth::Scheme scheme,
      const NetworkIsolationKey& network_isolation_key);

  void EvictLeastRecentlyUsedEntry();

  bool key_server_entries_by_network_isolation_key_;

  EntryMap entries_;

  DISALLOW_COPY_AND_ASSIGN(HttpAuthCache);
};

// An authentication realm entry.
}  // namespace net

#endif  // NET_HTTP_HTTP_AUTH_CACHE_H_
