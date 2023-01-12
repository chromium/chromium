// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_SESSION_CLEANUP_COOKIE_STORE_H_
#define SERVICES_NETWORK_SESSION_CLEANUP_COOKIE_STORE_H_

#include <stddef.h>

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/component_export.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "net/cookies/cookie_monster.h"
#include "net/extras/sqlite/sqlite_persistent_cookie_store.h"
#include "net/log/net_log_with_source.h"
#include "services/network/public/cpp/session_cookie_delete_predicate.h"

namespace net {
class CanonicalCookie;
}  // namespace net

namespace network {

// Implements a PersistentCookieStore that keeps an in-memory map of cookie
// origins, and allows deletion of cookies using the
// network::DeleteCookiePredicate. This is used to clear cookies with
// session-only policy at the end of a session.
class COMPONENT_EXPORT(NETWORK_SERVICE) SessionCleanupCookieStore
    : public net::CookieMonster::PersistentCookieStore {
 public:
  using CookiesPerOriginMap =
      std::map<net::SQLitePersistentCookieStore::CookieOrigin, size_t>;

  // Wraps the passed-in |cookie_store|.
  explicit SessionCleanupCookieStore(
      const scoped_refptr<net::SQLitePersistentCookieStore>& cookie_store);

  SessionCleanupCookieStore(const SessionCleanupCookieStore&) = delete;
  SessionCleanupCookieStore& operator=(const SessionCleanupCookieStore&) =
      delete;

  // net::CookieMonster::PersistentCookieStore:
  void Load(LoadedCallback loaded_callback,
            const net::NetLogWithSource& net_log) override;
  void LoadCookiesForKey(const std::string& key,
                         LoadedCallback callback) override;
  void AddCookie(const net::CanonicalCookie& cc) override;
  void UpdateCookieAccessTime(const net::CanonicalCookie& cc) override;
  void DeleteCookie(const net::CanonicalCookie& cc) override;
  void SetForceKeepSessionState() override;
  void SetBeforeCommitCallback(base::RepeatingClosure callback) override;
  void Flush(base::OnceClosure callback) override;

  // Should be called at the end of a session. Deletes all cookies that
  // |delete_cookie_predicate| returns true for.
  void DeleteSessionCookies(DeleteCookiePredicate delete_cookie_predicate);

 protected:
  ~SessionCleanupCookieStore() override;

 private:
  // Called after cookies are loaded from the database.  Calls |loaded_callback|
  // when done.
  void OnLoad(LoadedCallback loaded_callback,
              std::vector<std::unique_ptr<net::CanonicalCookie>> cookies);

  // Map of (domain keys(eTLD+1), is secure cookie) to number of cookies in the
  // database.
  CookiesPerOriginMap cookies_per_origin_;

  scoped_refptr<net::SQLitePersistentCookieStore> persistent_store_;

  // When set to true, DeleteSessionCookies will be a no-op, and all cookies
  // will be kept.
  bool force_keep_session_state_ = false;

  net::NetLogWithSource net_log_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_SESSION_CLEANUP_COOKIE_STORE_H_
