// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/net/cookie_util.h"

#import <Foundation/Foundation.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/sysctl.h>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/task/post_task.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/net/cookies/cookie_store_ios.h"
#import "ios/net/cookies/system_cookie_store.h"
#include "ios/web/common/features.h"
#include "ios/web/public/thread/web_task_traits.h"
#include "ios/web/public/thread/web_thread.h"
#include "net/cookies/cookie_monster.h"
#include "net/cookies/cookie_store.h"
#include "net/extras/sqlite/sqlite_persistent_cookie_store.h"
#include "net/log/net_log.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace cookie_util {

namespace {

// Date of the last cookie deletion.
NSString* const kLastCookieDeletionDate = @"LastCookieDeletionDate";

// Creates a SQLitePersistentCookieStore running on a background thread.
scoped_refptr<net::SQLitePersistentCookieStore> CreatePersistentCookieStore(
    const base::FilePath& path,
    bool restore_old_session_cookies,
    net::CookieCryptoDelegate* crypto_delegate) {
  return scoped_refptr<net::SQLitePersistentCookieStore>(
      new net::SQLitePersistentCookieStore(
          path, base::CreateSingleThreadTaskRunner({web::WebThread::IO}),
          base::CreateSequencedTaskRunner({base::ThreadPool(), base::MayBlock(),
                                           base::TaskPriority::BEST_EFFORT}),
          restore_old_session_cookies, crypto_delegate));
}

// Creates a CookieMonster configured by |config|.
std::unique_ptr<net::CookieMonster> CreateCookieMonster(
    const CookieStoreConfig& config,
    net::NetLog* net_log) {
  if (config.path.empty()) {
    // Empty path means in-memory store.
    return std::make_unique<net::CookieMonster>(nullptr /* store */, net_log);
  }

  const bool restore_old_session_cookies =
      config.session_cookie_mode == CookieStoreConfig::RESTORED_SESSION_COOKIES;
  scoped_refptr<net::SQLitePersistentCookieStore> persistent_store =
      CreatePersistentCookieStore(config.path, restore_old_session_cookies,
                                  config.crypto_delegate);
  std::unique_ptr<net::CookieMonster> cookie_monster(
      new net::CookieMonster(persistent_store.get(), net_log));
  if (restore_old_session_cookies)
    cookie_monster->SetPersistSessionCookies(true);
  return cookie_monster;
}

}  // namespace

CookieStoreConfig::CookieStoreConfig(const base::FilePath& path,
                                     SessionCookieMode session_cookie_mode,
                                     CookieStoreType cookie_store_type,
                                     net::CookieCryptoDelegate* crypto_delegate)
    : path(path),
      session_cookie_mode(session_cookie_mode),
      cookie_store_type(cookie_store_type),
      crypto_delegate(crypto_delegate) {
  CHECK(!path.empty() || session_cookie_mode == EPHEMERAL_SESSION_COOKIES);
}

CookieStoreConfig::~CookieStoreConfig() {}

std::unique_ptr<net::CookieStore> CreateCookieStore(
    const CookieStoreConfig& config,
    std::unique_ptr<net::SystemCookieStore> system_cookie_store,
    net::NetLog* net_log) {
  if (config.cookie_store_type == CookieStoreConfig::COOKIE_MONSTER)
    return CreateCookieMonster(config, net_log);

  // Using the SystemCookieStore will allow URLFetcher and any other users of
  // net:CookieStore to in iOS to use cookies directly from WKHTTPCookieStore.
  return std::make_unique<net::CookieStoreIOS>(std::move(system_cookie_store),
                                               net_log);
}

bool ShouldClearSessionCookies() {
  NSUserDefaults* standardDefaults = [NSUserDefaults standardUserDefaults];
  struct timeval boottime;
  int mib[2] = {CTL_KERN, KERN_BOOTTIME};
  size_t size = sizeof(boottime);
  time_t lastCookieDeletionDate =
      [standardDefaults integerForKey:kLastCookieDeletionDate];
  time_t now;
  time(&now);
  bool clear_cookies = true;
  if (lastCookieDeletionDate != 0 &&
      sysctl(mib, 2, &boottime, &size, NULL, 0) != -1 && boottime.tv_sec != 0) {
    clear_cookies = boottime.tv_sec > lastCookieDeletionDate;
  }
  if (clear_cookies)
    [standardDefaults setInteger:now forKey:kLastCookieDeletionDate];
  return clear_cookies;
}

// Clears the session cookies for |profile|.
void ClearSessionCookies(ios::ChromeBrowserState* browser_state) {
  scoped_refptr<net::URLRequestContextGetter> getter =
      browser_state->GetRequestContext();
  base::PostTask(FROM_HERE, {web::WebThread::IO}, base::BindOnce(^{
                   getter->GetURLRequestContext()
                       ->cookie_store()
                       ->DeleteSessionCookiesAsync(base::DoNothing());
                 }));
}

}  // namespace cookie_util
