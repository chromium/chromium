// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_SOCKET_WRITE_QUOTA_CHECKER_H_
#define EXTENSIONS_BROWSER_API_SOCKET_WRITE_QUOTA_CHECKER_H_

#include <stddef.h>

#include <map>

#include "base/auto_reset.h"
#include "base/scoped_observation.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/common/extension_id.h"

namespace extensions {

// Tracks bytes quota per extension.
class WriteQuotaChecker : public BrowserContextKeyedAPI,
                          public extensions::ExtensionRegistryObserver {
 public:
  class ScopedBytesLimitForTest {
   public:
    ScopedBytesLimitForTest(WriteQuotaChecker* checker, size_t new_bytes_limit);
    ~ScopedBytesLimitForTest();

   private:
    base::AutoReset<size_t> scoped_bytes_limit_;
  };

  static WriteQuotaChecker* Get(content::BrowserContext* context);

  // BrowserContextKeyedAPI:
  static BrowserContextKeyedAPIFactory<WriteQuotaChecker>* GetFactoryInstance();
  static const char* service_name() { return "WriteQuotaChecker"; }

  explicit WriteQuotaChecker(content::BrowserContext* context);
  ~WriteQuotaChecker() override;

  // Attempts to take more bytes to write. Returns false if the attempt would
  // exceed the limit.
  bool TakeBytes(const ExtensionId& extension_id, size_t bytes);

  // Puts bytes back to the pool after done with writing.
  void ReturnBytes(const ExtensionId& extension_id, size_t bytes);

 private:
  friend class BrowserContextKeyedAPIFactory<WriteQuotaChecker>;

  // ExtensionRegistryObserver:
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const Extension* extension,
                           UnloadedExtensionReason reason) override;

  // Max pending write bytes.
  size_t bytes_limit_ = 0;

  // Tracked writing bytes per extension.
  std::map<ExtensionId, size_t> bytes_used_map_;

  base::ScopedObservation<ExtensionRegistry, ExtensionRegistryObserver>
      extension_registry_observation_{this};
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_SOCKET_WRITE_QUOTA_CHECKER_H_
