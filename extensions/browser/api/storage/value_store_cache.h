// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_STORAGE_VALUE_STORE_CACHE_H_
#define EXTENSIONS_BROWSER_API_STORAGE_VALUE_STORE_CACHE_H_

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "extensions/common/extension_id.h"

namespace value_store {
class ValueStore;
}

namespace extensions {

class Extension;

// Each namespace of the storage API implements this interface.
// Instances are created on the UI thread, but from then on live on the FILE
// thread. At shutdown, ShutdownOnUI() is first invoked on the UI thread, and
// the destructor is invoked soon after on the FILE thread. This gives
// implementations the chance to work with ValueStores on FILE but observe
// events on UI.
// It also means that any methods invoked on UI *before ShutdownOnUI()* can
// safely post other methods to the FILE thread, since the deletion task is only
// posted to FILE after ShutdownOnUI().
class ValueStoreCache {
 public:
  using StorageCallback = base::OnceCallback<void(value_store::ValueStore*)>;

  // Invoked on FILE.
  virtual ~ValueStoreCache();

  // This is invoked from the UI thread during destruction of the Profile that
  // ultimately owns this object. Any Profile-related cleanups should be
  // performed in this method, since the destructor will execute later, after
  // the Profile is already gone.
  virtual void ShutdownOnUI();

  // Requests the cache to invoke |callback| with the appropriate ValueStore
  // for the given |extension|. |callback| should be invoked with a NULL
  // ValueStore in case of errors.
  // |extension| is passed in a scoped_refptr<> because this method is
  // asynchronously posted as a task to the loop returned by GetMessageLoop(),
  // and this guarantees the Extension is still valid when the method executes.
  virtual void RunWithValueStoreForExtension(
      StorageCallback callback,
      scoped_refptr<const Extension> extension) = 0;

  // Requests the cache to delete any storage used by |extension_id|.
  virtual void DeleteStorageSoon(const ExtensionId& extension_id) = 0;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_STORAGE_VALUE_STORE_CACHE_H_
