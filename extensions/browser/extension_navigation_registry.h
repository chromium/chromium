// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_EXTENSION_NAVIGATION_REGISTRY_H_
#define EXTENSIONS_BROWSER_EXTENSION_NAVIGATION_REGISTRY_H_

#include <queue>

#include "base/containers/flat_map.h"
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
                               const GURL& target_url);

  // Return metadata if it exists and remove it from memory.
  std::optional<GURL> GetAndErase(int64_t navigation_handle_id);

  // Determine whether the feature is enabled.
  bool IsEnabled();

 private:
  // BrowserContextKeyedAPIFactory or BrowserContextKeyedAPI related.
  friend class BrowserContextKeyedAPIFactory<ExtensionNavigationRegistry>;
  static const char* service_name() { return "ExtensionNavigationRegistry"; }
  static const bool kServiceHasOwnInstanceInIncognito = true;

  const raw_ptr<content::BrowserContext> browser_context_;

  // An ID in existence means that the navigation was intercepted by WebRequest.
  base::flat_map</*navigation_id=*/int64_t, /*new_url=*/GURL>
      redirect_metadata_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_EXTENSION_NAVIGATION_REGISTRY_H_
