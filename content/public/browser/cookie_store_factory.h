// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_COOKIE_STORE_FACTORY_H_
#define CONTENT_PUBLIC_BROWSER_COOKIE_STORE_FACTORY_H_

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"

namespace base {
class SequencedTaskRunner;
}

namespace net {
class CookieCryptoDelegate;
class CookieStore;
class NetLog;
}

namespace content {

struct CONTENT_EXPORT CookieStoreConfig {
  // Convenience constructor for an in-memory cookie store with no delegate.
  CookieStoreConfig();

  // This struct is move-only but also intentionally deletes the move assignment
  // operator as base::FilePath does not implement this operator.
  CookieStoreConfig(CookieStoreConfig&&);
  CookieStoreConfig& operator=(CookieStoreConfig&&) = delete;

  // If |path| is empty, then this specifies an in-memory cookie store.
  // With in-memory cookie stores, |session_cookie_mode| must be
  // EPHEMERAL_SESSION_COOKIES.
  //
  // Note: If |crypto_delegate| is non-nullptr, it must outlive any CookieStores
  // created using this config.
  CookieStoreConfig(const base::FilePath& path,
                    bool restore_old_session_cookies,
                    bool persist_session_cookies);
  ~CookieStoreConfig();

  const base::FilePath path;
  const bool restore_old_session_cookies;
  const bool persist_session_cookies;
  // The following are infrequently used cookie store parameters.
  // Rather than clutter the constructor API, these are assigned a default
  // value on CookieStoreConfig construction. Clients should then override
  // them as necessary.

  // Used to provide encryption hooks for the cookie store.
  std::unique_ptr<net::CookieCryptoDelegate> crypto_delegate;

  // Callbacks for data load events will be performed on |client_task_runner|.
  // If nullptr, uses the task runner for BrowserThread::IO.
  //
  // Only used for persistent cookie stores.
  scoped_refptr<base::SequencedTaskRunner> client_task_runner;

  // All blocking database accesses will be performed on
  // |background_task_runner|.  If nullptr, uses a SequencedTaskRunner from the
  // BrowserThread blocking pool.
  //
  // Only used for persistent cookie stores.
  scoped_refptr<base::SequencedTaskRunner> background_task_runner;

  // If non-empty, overrides the default list of schemes that support cookies.
  std::vector<std::string> cookieable_schemes;
};

CONTENT_EXPORT std::unique_ptr<net::CookieStore> CreateCookieStore(
    CookieStoreConfig config,
    net::NetLog* net_log);

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_COOKIE_STORE_FACTORY_H_
