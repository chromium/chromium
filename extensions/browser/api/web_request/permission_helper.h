// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_WEB_REQUEST_PERMISSION_HELPER_H_
#define EXTENSIONS_BROWSER_API_WEB_REQUEST_PERMISSION_HELPER_H_

#include "base/memory/raw_ptr.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/process_map.h"

namespace extensions {
class ExtensionRegistry;
class ProcessMap;
struct WebRequestInfo;

// Intermediate keyed service used to declare dependencies on other services
// that are needed for WebRequest permissions.
class PermissionHelper : public BrowserContextKeyedAPI {
 public:
  explicit PermissionHelper(content::BrowserContext* context);

  PermissionHelper(const PermissionHelper&) = delete;
  PermissionHelper& operator=(const PermissionHelper&) = delete;

  ~PermissionHelper() override;

  // Convenience method to get the PermissionHelper for a profile.
  static PermissionHelper* Get(content::BrowserContext* context);

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<PermissionHelper>* GetFactoryInstance();

  bool ShouldHideBrowserNetworkRequest(const WebRequestInfo& request) const;
  bool CanCrossIncognito(const Extension* extension) const;

  const ProcessMap* process_map() const {
    return ProcessMap::Get(browser_context_);
  }

  const ExtensionRegistry* extension_registry() const {
    return extension_registry_;
  }

 private:
  friend class BrowserContextKeyedAPIFactory<PermissionHelper>;

  const raw_ptr<content::BrowserContext> browser_context_;
  const raw_ptr<ExtensionRegistry> extension_registry_;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "PermissionHelper"; }
  static const bool kServiceRedirectedInIncognito = true;
};

template <>
void BrowserContextKeyedAPIFactory<
    PermissionHelper>::DeclareFactoryDependencies();

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_WEB_REQUEST_PERMISSION_HELPER_H_
