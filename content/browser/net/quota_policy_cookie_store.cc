// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/net/quota_policy_cookie_store.h"

#include <list>
#include <memory>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner.h"
#include "base/task/post_task.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/cookie_store_factory.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_constants.h"
#include "net/cookies/cookie_util.h"
#include "net/extras/sqlite/cookie_crypto_delegate.h"
#include "storage/browser/quota/special_storage_policy.h"

namespace content {

QuotaPolicyCookieStore::QuotaPolicyCookieStore(
    const scoped_refptr<net::SQLitePersistentCookieStore>& cookie_store,
    storage::SpecialStoragePolicy* special_storage_policy)
    : SessionCleanupCookieStore(cookie_store),
      special_storage_policy_(special_storage_policy) {}

QuotaPolicyCookieStore::~QuotaPolicyCookieStore() {
  if (!special_storage_policy_.get() ||
      !special_storage_policy_->HasSessionOnlyOrigins()) {
    return;
  }

  DeleteSessionCookies(
      special_storage_policy_->CreateDeleteCookieOnExitPredicate());
}

CookieStoreConfig::CookieStoreConfig()
    : restore_old_session_cookies(false),
      persist_session_cookies(false),
      crypto_delegate(nullptr) {
  // Default to an in-memory cookie store.
}

CookieStoreConfig::CookieStoreConfig(
    const base::FilePath& path,
    bool restore_old_session_cookies,
    bool persist_session_cookies,
    storage::SpecialStoragePolicy* storage_policy)
    : path(path),
      restore_old_session_cookies(restore_old_session_cookies),
      persist_session_cookies(persist_session_cookies),
      storage_policy(storage_policy),
      crypto_delegate(nullptr) {
  CHECK(!path.empty() ||
        (!restore_old_session_cookies && !persist_session_cookies));
}

CookieStoreConfig::~CookieStoreConfig() {
}

std::unique_ptr<net::CookieStore> CreateCookieStore(
    const CookieStoreConfig& config,
    net::NetLog* net_log) {
  std::unique_ptr<net::CookieMonster> cookie_monster;

  if (config.path.empty()) {
    // Empty path means in-memory store.
    cookie_monster =
        std::make_unique<net::CookieMonster>(nullptr /* store */, net_log);
  } else {
    scoped_refptr<base::SequencedTaskRunner> client_task_runner =
        config.client_task_runner;
    scoped_refptr<base::SequencedTaskRunner> background_task_runner =
        config.background_task_runner;

    if (!client_task_runner.get()) {
      client_task_runner =
          base::CreateSingleThreadTaskRunner({BrowserThread::IO});
    }

    if (!background_task_runner.get()) {
      background_task_runner = base::CreateSequencedTaskRunner(
          {base::ThreadPool(), base::MayBlock(),
           net::GetCookieStoreBackgroundSequencePriority(),
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN});
    }

    scoped_refptr<net::SQLitePersistentCookieStore> sqlite_store(
        new net::SQLitePersistentCookieStore(
            config.path, client_task_runner, background_task_runner,
            config.restore_old_session_cookies, config.crypto_delegate));

    QuotaPolicyCookieStore* persistent_store =
        new QuotaPolicyCookieStore(
            sqlite_store.get(),
            config.storage_policy.get());

    cookie_monster =
        std::make_unique<net::CookieMonster>(persistent_store, net_log);
    if (config.persist_session_cookies)
      cookie_monster->SetPersistSessionCookies(true);
  }

  if (!config.cookieable_schemes.empty())
    // No need to wait for callback, the work happens synchronously.
    cookie_monster->SetCookieableSchemes(config.cookieable_schemes,
                                         base::DoNothing());

  return std::move(cookie_monster);
}

}  // namespace content
