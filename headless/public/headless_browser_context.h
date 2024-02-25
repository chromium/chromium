// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_PUBLIC_HEADLESS_BROWSER_CONTEXT_H_
#define HEADLESS_PUBLIC_HEADLESS_BROWSER_CONTEXT_H_

#include <list>
#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "headless/public/headless_export.h"
#include "headless/public/headless_web_contents.h"
#include "net/proxy_resolution/proxy_resolution_service.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"

namespace base {
class FilePath;
}

namespace headless {
class HeadlessBrowserImpl;
class HeadlessBrowserContextOptions;

// Imported into headless namespace for
// Builder::SetOverrideWebPreferencesCallback().
using blink::web_pref::WebPreferences;

// Represents an isolated session with a unique cache, cookies, and other
// profile/session related data.
// When browser context is deleted, all associated web contents are closed.
class HEADLESS_EXPORT HeadlessBrowserContext {
 public:
  class Builder;

  HeadlessBrowserContext(const HeadlessBrowserContext&) = delete;
  HeadlessBrowserContext& operator=(const HeadlessBrowserContext&) = delete;

  virtual ~HeadlessBrowserContext() {}

  // Open a new tab. Returns a builder object which can be used to set
  // properties for the new tab.
  // Pointer to HeadlessWebContents becomes invalid after:
  // a) Calling HeadlessWebContents::Close, or
  // b) Calling HeadlessBrowserContext::Close on associated browser context, or
  // c) Calling HeadlessBrowser::Shutdown.
  virtual HeadlessWebContents::Builder CreateWebContentsBuilder() = 0;

  // Returns all web contents owned by this browser context.
  virtual std::vector<HeadlessWebContents*> GetAllWebContents() = 0;

  // See HeadlessBrowser::GetWebContentsForDevToolsAgentHostId.
  virtual HeadlessWebContents* GetWebContentsForDevToolsAgentHostId(
      const std::string& devtools_agent_host_id) = 0;

  // Destroy this BrowserContext and all WebContents associated with it.
  virtual void Close() = 0;

  // GUID for this browser context.
  virtual const std::string& Id() = 0;

  // TODO(skyostil): Allow saving and restoring contexts (crbug.com/617931).

 protected:
  HeadlessBrowserContext() {}
};

class HEADLESS_EXPORT HeadlessBrowserContext::Builder {
 public:
  Builder(Builder&&);

  Builder(const Builder&) = delete;
  Builder& operator=(const Builder&) = delete;

  ~Builder();

  // By default if you add mojo bindings, http and https are disabled because
  // its almost certinly unsafe for arbitary sites on the internet to have
  // access to these bindings.  If you know what you're doing it may be OK to
  // turn them back on. E.g. if headless_lib is being used in a testing
  // framework which serves the web content from disk that's likely ok.
  //
  // That said, best pratice is to add a ProtocolHandler to serve the
  // webcontent over a custom protocol. That way you can be sure that only the
  // things you intend have access to mojo.
  Builder& EnableUnsafeNetworkAccessWithMojoBindings(
      bool enable_http_and_https_if_mojo_used);

  Builder& SetAcceptLanguage(const std::string& accept_language);
  Builder& SetUserAgent(const std::string& user_agent);
  Builder& SetProxyConfig(std::unique_ptr<net::ProxyConfig> proxy_config);
  Builder& SetWindowSize(const gfx::Size& window_size);
  Builder& SetUserDataDir(const base::FilePath& user_data_dir);
  Builder& SetDiskCacheDir(const base::FilePath& disk_cache_dir);
  Builder& SetIncognitoMode(bool incognito_mode);
  Builder& SetBlockNewWebContents(bool block_new_web_contents);

  HeadlessBrowserContext* Build();

 private:
  friend class HeadlessBrowserImpl;
  friend class HeadlessBrowserContextImpl;

  explicit Builder(HeadlessBrowserImpl* browser);

  raw_ptr<HeadlessBrowserImpl> browser_;
  std::unique_ptr<HeadlessBrowserContextOptions> options_;
};

}  // namespace headless

#endif  // HEADLESS_PUBLIC_HEADLESS_BROWSER_CONTEXT_H_
