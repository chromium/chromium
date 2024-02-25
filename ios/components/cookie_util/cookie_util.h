// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_COMPONENTS_COOKIE_UTIL_COOKIE_UTIL_H_
#define IOS_COMPONENTS_COOKIE_UTIL_COOKIE_UTIL_H_

#include <memory>

#include "base/files/file_path.h"
#include "net/cookies/canonical_cookie.h"

class PrefService;

namespace net {
class CookieStore;
class SystemCookieStore;
class NetLog;
}  // namespace net

namespace web {
class BrowserState;
}

namespace cookie_util {

// Configuration for creation of new cookie stores.
struct CookieStoreConfig {
  // Specifies how session cookies are persisted in the backing data store.
  //
  // EPHEMERAL_SESSION_COOKIES specifies session cookies will not be written
  // out in a manner that allows for restoration.
  //
  // RESTORED_SESSION_COOKIES specifies that session cookies are written in a
  // manner that allows for them to be restored if the cookie store is opened
  // again using RESTORED_SESSION_COOKIES.
  enum SessionCookieMode {
    EPHEMERAL_SESSION_COOKIES,
    RESTORED_SESSION_COOKIES
  };

  // Persistent cookie stores can either be CookieMonster or CookieStoreIOS.
  // The CookieMonster is the cross-platform implementation and should be
  // preferred in general.
  // The CookieStoreIOS must be used when the cookie store is accessed by the
  // system through UIWebView. However, note that only one CookieStoreIOS
  // instance can be kept synchronized with the global system cookie store at a
  // time.
  enum CookieStoreType {
    COOKIE_MONSTER,   // CookieMonster backend.
    COOKIE_STORE_IOS  // CookieStoreIOS backend.
  };

  // If `path` is empty, then this specifies an in-memory cookie store.
  // With in-memory cookie stores, `session_cookie_mode` must be
  // EPHEMERAL_SESSION_COOKIES.
  CookieStoreConfig(const base::FilePath& path,
                    SessionCookieMode session_cookie_mode,
                    CookieStoreType cookie_store_type);
  ~CookieStoreConfig();

  const base::FilePath path;
  const SessionCookieMode session_cookie_mode;
  const CookieStoreType cookie_store_type;
};

// Creates a cookie store which is internally either a CookieMonster or a
// CookieStoreIOS.
std::unique_ptr<net::CookieStore> CreateCookieStore(
    const CookieStoreConfig& config,
    std::unique_ptr<net::SystemCookieStore> system_cookie_store,
    net::NetLog* net_log);

// Returns true if the cookies should be cleared.
// Current implementation returns true if the device has rebooted since the
// last time cookies have been cleared.
bool ShouldClearSessionCookies(PrefService* pref_service);

// Clears the session cookies for `browser_state`.
void ClearSessionCookies(web::BrowserState* browser_state);

}  // namespace cookie_util

#endif  // IOS_COMPONENTS_COOKIE_UTIL_COOKIE_UTIL_H_
