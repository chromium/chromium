// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_STORAGE_SETTINGS_TEST_UTIL_H_
#define EXTENSIONS_BROWSER_API_STORAGE_SETTINGS_TEST_UTIL_H_

#include <memory>
#include <set>
#include <string>

#include "base/compiler_specific.h"
#include "base/memory/scoped_refptr.h"
#include "components/value_store/value_store_factory.h"
#include "extensions/browser/api/storage/settings_namespace.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/mock_extension_system.h"
#include "extensions/common/extension.h"

namespace base {
class Value;
}

namespace value_store {
class ValueStore;
}

namespace extensions {

class StorageFrontend;
// Utilities for extension settings API tests.
namespace settings_test_util {

// Creates a kilobyte of data.
base::Value CreateKilobyte();

// Creates a megabyte of data.
base::Value CreateMegabyte();

// Synchronously gets the storage area for an extension from |frontend|.
value_store::ValueStore* GetStorage(
    scoped_refptr<const Extension> extension,
    settings_namespace::Namespace setting_namespace,
    StorageFrontend* frontend);

// Synchronously gets the SYNC storage for an extension from |frontend|.
value_store::ValueStore* GetStorage(scoped_refptr<const Extension> extension,
                                    StorageFrontend* frontend);

// Creates an extension with |id| and adds it to the registry for |context|.
scoped_refptr<const Extension> AddExtensionWithId(
    content::BrowserContext* context,
    const std::string& id,
    Manifest::Type type);

// Creates an extension with |id| with a set of |permissions| and adds it to
// the registry for |context|.
scoped_refptr<const Extension> AddExtensionWithIdAndPermissions(
    content::BrowserContext* context,
    const std::string& id,
    Manifest::Type type,
    const std::set<std::string>& permissions);

}  // namespace settings_test_util

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_STORAGE_SETTINGS_TEST_UTIL_H_
