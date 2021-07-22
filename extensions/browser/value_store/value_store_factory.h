// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_VALUE_STORE_VALUE_STORE_FACTORY_H_
#define EXTENSIONS_BROWSER_VALUE_STORE_VALUE_STORE_FACTORY_H_

#include <memory>
#include <set>

#include "base/memory/ref_counted.h"
#include "extensions/browser/value_store/settings_namespace.h"
#include "extensions/browser/value_store/value_store_client_id.h"

class ValueStore;

namespace extensions {

// Create new value stores for rules, state, or settings. For settings will
// also create stores for the specified namespace and model type.
//
// Note: This factory creates the lower level stores that directly read/write to
//       disk. Sync/Managed stores are created directly, but delegate their
//       calls to a |ValueStore| created by this interface.
class ValueStoreFactory : public base::RefCountedThreadSafe<ValueStoreFactory> {
 public:
  enum class ModelType { APP, EXTENSION };

  // Create a |ValueStore| to contain rules data.
  virtual std::unique_ptr<ValueStore> CreateRulesStore() = 0;

  // Create a |ValueStore| to contain state data.
  virtual std::unique_ptr<ValueStore> CreateStateStore() = 0;

  // Create a |ValueStore| to contain settings data for a specific extension
  // namespace and model type.
  virtual std::unique_ptr<ValueStore> CreateSettingsStore(
      settings_namespace::Namespace settings_namespace,
      ModelType model_type,
      const ValueStoreClientId& id) = 0;

  // Delete all settings for specified given extension in the specified
  // namespace/model_type.
  virtual void DeleteSettings(settings_namespace::Namespace settings_namespace,
                              ModelType model_type,
                              const ValueStoreClientId& id) = 0;

  // Are there any settings stored in the specified namespace/model_type for
  // the given extension?
  virtual bool HasSettings(settings_namespace::Namespace settings_namespace,
                           ModelType model_type,
                           const ValueStoreClientId& id) = 0;

  // TODO(crbug.com/1226956): Remove reference to extensions.
  // Return all extension ID's with settings stored in the given
  // namespace/model_type.
  virtual std::set<ValueStoreClientId> GetKnownExtensionIDs(
      settings_namespace::Namespace settings_namespace,
      ModelType model_type) const = 0;

 protected:
  friend class base::RefCountedThreadSafe<ValueStoreFactory>;
  virtual ~ValueStoreFactory() = default;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_VALUE_STORE_VALUE_STORE_FACTORY_H_
