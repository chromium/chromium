// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_STATE_STORE_TEST_OBSERVER_H_
#define EXTENSIONS_BROWSER_STATE_STORE_TEST_OBSERVER_H_

#include <set>
#include <string>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "extensions/browser/state_store.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace extensions {

// Observe when an extension's data is written to the state store.
class StateStoreTestObserver final : public StateStore::TestObserver {
 public:
  explicit StateStoreTestObserver(content::BrowserContext* context);

  ~StateStoreTestObserver() override;

  void WaitForExtensionAndKey(const ExtensionId& extension_id,
                              const std::string& key);

  // StateStore::TestObserver
  void WillSetExtensionValue(const ExtensionId& extension_id,
                             const std::string& key) override;

 private:
  const raw_ptr<StateStore> state_store_;
  // Contains a composite key of the extension ID and StateStore key
  // written.
  using MapKey = std::pair<ExtensionId, std::string>;
  std::set<MapKey> ids_and_keys_with_writes_;
  MapKey waiting_for_map_key_;
  base::RunLoop run_loop_;
  base::ScopedObservation<StateStore, StateStore::TestObserver> observed_{this};
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_STATE_STORE_TEST_OBSERVER_H_
