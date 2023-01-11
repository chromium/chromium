// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_LOCK_SCREEN_DATA_LOCK_SCREEN_VALUE_STORE_MIGRATOR_H_
#define EXTENSIONS_BROWSER_API_LOCK_SCREEN_DATA_LOCK_SCREEN_VALUE_STORE_MIGRATOR_H_

#include <set>
#include <string>

#include "base/functional/callback.h"
#include "extensions/common/extension_id.h"

namespace extensions {
namespace lock_screen_data {

// Interface used to migrate lock screen data items between value stores used by
// the lock screen data API.
class LockScreenValueStoreMigrator {
 public:
  virtual ~LockScreenValueStoreMigrator() = default;

  using ExtensionMigratedCallback =
      base::RepeatingCallback<void(const ExtensionId& extension_id)>;
  // Migrates lock screen item storage data items for extensions in the
  // |extensions_to_migrate| set. |callback| is called on migration completion
  // for each of the extensions.
  virtual void Run(const std::set<ExtensionId>& extensions_to_migrate,
                   ExtensionMigratedCallback callback) = 0;

  // Returns whether data migration is in progress for an extension.
  virtual bool IsMigratingExtensionData(
      const ExtensionId& extension_id) const = 0;

  // Clears data for an extension from both migration source and target value
  // stores. |callback| is called when the data for the extension has been
  // cleared.
  // Note that callback passed to |Run| is not expected to be run for the
  // extension after this method is called.
  virtual void ClearDataForExtension(const ExtensionId& extension_id,
                                     base::OnceClosure callback) = 0;
};

}  // namespace lock_screen_data
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_LOCK_SCREEN_DATA_LOCK_SCREEN_VALUE_STORE_MIGRATOR_H_
