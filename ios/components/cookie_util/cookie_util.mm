// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/components/cookie_util/cookie_util.h"

#import <Foundation/Foundation.h>
#import <stddef.h>
#import <stdint.h>
#import <sys/sysctl.h>

#import "base/check.h"
#import "base/functional/bind.h"
#import "base/functional/callback_helpers.h"
#import "base/memory/ref_counted.h"
#import "base/task/thread_pool.h"
#import "ios/net/cookies/cookie_store_ios.h"
#import "ios/net/cookies/system_cookie_store.h"
#import "ios/web/common/features.h"
#import "ios/web/public/browser_state.h"
#import "ios/web/public/thread/web_task_traits.h"
#import "ios/web/public/thread/web_thread.h"
#import "net/cookies/cookie_monster.h"
#import "net/cookies/cookie_store.h"
#import "net/extras/sqlite/sqlite_persistent_cookie_store.h"
#import "net/log/net_log.h"
#import "net/url_request/url_request_context.h"
#import "net/url_request/url_request_context_getter.h"

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
          path, web::GetIOThreadTaskRunner({}),
          base::ThreadPool::CreateSequencedTaskRunner(
              {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
               base::TaskShutdownBehavior::BLOCK_SHUTDOWN}),
          restore_old_session_cookies, crypto_delegate,
          /*enable_exclusive_access=*/false));
}

// Creates a CookieMonster configured by `config`.
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

// Clears the session cookies for `browser_state`.
void ClearSessionCookies(web::BrowserState* browser_state) {
  scoped_refptr<net::URLRequestContextGetter> getter =
      browser_state->GetRequestContext();
  web::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(^{
        getter->GetURLRequestContext()
            ->cookie_store()
            ->DeleteSessionCookiesAsync(base::DoNothing());
      }));
}

}  // namespace cookie_util
