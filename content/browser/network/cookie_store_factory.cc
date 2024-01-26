// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/cookie_store_factory.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "net/cookies/cookie_constants.h"
#include "net/cookies/cookie_monster.h"
#include "net/extras/sqlite/cookie_crypto_delegate.h"
#include "net/extras/sqlite/sqlite_persistent_cookie_store.h"

namespace content {

CookieStoreConfig::CookieStoreConfig()
    : restore_old_session_cookies(false), persist_session_cookies(false) {
  // Default to an in-memory cookie store.
}

CookieStoreConfig::CookieStoreConfig(const base::FilePath& path,
                                     bool restore_old_session_cookies,
                                     bool persist_session_cookies)
    : path(path),
      restore_old_session_cookies(restore_old_session_cookies),
      persist_session_cookies(persist_session_cookies) {
  CHECK(!path.empty() ||
        (!restore_old_session_cookies && !persist_session_cookies));
}

CookieStoreConfig::CookieStoreConfig(CookieStoreConfig&&) = default;

CookieStoreConfig::~CookieStoreConfig() {}

std::unique_ptr<net::CookieStore> CreateCookieStore(CookieStoreConfig config,
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
      client_task_runner = GetIOThreadTaskRunner({});
    }

    if (!background_task_runner.get()) {
      background_task_runner = base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), net::GetCookieStoreBackgroundSequencePriority(),
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN});
    }

    scoped_refptr<net::SQLitePersistentCookieStore> sqlite_store(
        new net::SQLitePersistentCookieStore(
            config.path, client_task_runner, background_task_runner,
            config.restore_old_session_cookies,
            std::move(config.crypto_delegate),
            /*enable_exclusive_access=*/false));

    cookie_monster =
        std::make_unique<net::CookieMonster>(std::move(sqlite_store), net_log);
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
