// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_EXTENSION_NAVIGATION_REGISTRY_H_
#define EXTENSIONS_BROWSER_EXTENSION_NAVIGATION_REGISTRY_H_

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "url/gurl.h"

namespace extensions {

// Associate navigation_handle id's with metadata for the purpose of determining
// whether an extension triggered a redirect.
class ExtensionNavigationRegistry : public BrowserContextKeyedAPI {
 public:
  explicit ExtensionNavigationRegistry(
      content::BrowserContext* browser_context);

  ExtensionNavigationRegistry(const ExtensionNavigationRegistry&) = delete;
  ExtensionNavigationRegistry& operator=(const ExtensionNavigationRegistry&) =
      delete;

  ~ExtensionNavigationRegistry() override;

  struct Metadata {
    explicit Metadata(GURL gurl, ExtensionId extension_id)
        : gurl(gurl), extension_id(extension_id) {}

    GURL gurl;
    ExtensionId extension_id;
  };

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<ExtensionNavigationRegistry>*
  GetFactoryInstance();

  // Returns the instance for the given `browser_context`.
  static ExtensionNavigationRegistry* Get(
      content::BrowserContext* browser_context);

  // Remove navigation handle id, if it exists.
  void Erase(int64_t navigation_handle_id);

  // Record specific metadata about a navigation.
  void RecordExtensionRedirect(int64_t navigation_handle_id,
                               const GURL& target_url,
                               const ExtensionId& extension_id);

  // Return metadata if it exists and remove it from memory.
  std::optional<Metadata> GetAndErase(int64_t navigation_handle_id);

  // Determine if the server redirect is allowed to succeed. This is not
  // idempotent because it erases the corresponding record on the first call, so
  // this should only be called once for a given stage in the navigation.
  bool CanRedirect(int64_t navigation_id,
                   const GURL& gurl,
                   const Extension& extension);

 private:
  // BrowserContextKeyedAPIFactory or BrowserContextKeyedAPI related.
  friend class BrowserContextKeyedAPIFactory<ExtensionNavigationRegistry>;
  static const char* service_name() { return "ExtensionNavigationRegistry"; }
  static const bool kServiceHasOwnInstanceInIncognito = true;

  const raw_ptr<content::BrowserContext> browser_context_;

  // An ID in existence means that the navigation was intercepted by WebRequest.
  using NavigationId = int64_t;
  base::flat_map<NavigationId, Metadata> redirect_metadata_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_EXTENSION_NAVIGATION_REGISTRY_H_
