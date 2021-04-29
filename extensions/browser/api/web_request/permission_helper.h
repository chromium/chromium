// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_WEB_REQUEST_PERMISSION_HELPER_H_
#define EXTENSIONS_BROWSER_API_WEB_REQUEST_PERMISSION_HELPER_H_

#include "base/macros.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"

namespace extensions {
class ExtensionRegistry;
class ProcessMap;
struct WebRequestInfo;

// Intermediate keyed service used to declare dependencies on other services
// that are needed for WebRequest permissions.
class PermissionHelper : public BrowserContextKeyedAPI {
 public:
  explicit PermissionHelper(content::BrowserContext* context);
  ~PermissionHelper() override;

  // Convenience method to get the PermissionHelper for a profile.
  static PermissionHelper* Get(content::BrowserContext* context);

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<PermissionHelper>* GetFactoryInstance();

  bool ShouldHideBrowserNetworkRequest(const WebRequestInfo& request) const;
  bool CanCrossIncognito(const Extension* extension) const;

  const ProcessMap* process_map() const { return process_map_; }

  const ExtensionRegistry* extension_registry() const {
    return extension_registry_;
  }

 private:
  friend class BrowserContextKeyedAPIFactory<PermissionHelper>;

  content::BrowserContext* const browser_context_;
  ProcessMap* const process_map_;
  ExtensionRegistry* const extension_registry_;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "PermissionHelper"; }
  static const bool kServiceRedirectedInIncognito = true;

  DISALLOW_COPY_AND_ASSIGN(PermissionHelper);
};

template <>
void BrowserContextKeyedAPIFactory<
    PermissionHelper>::DeclareFactoryDependencies();

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_WEB_REQUEST_PERMISSION_HELPER_H_
