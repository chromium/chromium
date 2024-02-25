// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/state_store_test_observer.h"

#include "base/containers/contains.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension_id.h"

namespace extensions {

StateStoreTestObserver::StateStoreTestObserver(content::BrowserContext* context)
    : state_store_(ExtensionSystem::Get(context)->state_store()) {
  observed_.Observe(state_store_.get());
}

StateStoreTestObserver::~StateStoreTestObserver() = default;

void StateStoreTestObserver::WaitForExtensionAndKey(
    const ExtensionId& extension_id,
    const std::string& key) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  waiting_for_map_key_ = std::make_pair(extension_id, key);
  // If the key has already been reported, flush the state store.
  if (base::Contains(ids_and_keys_with_writes_, waiting_for_map_key_)) {
    state_store_->FlushForTesting(run_loop_.QuitWhenIdleClosure());
  }
  run_loop_.Run();
}

void StateStoreTestObserver::WillSetExtensionValue(
    const ExtensionId& extension_id,
    const std::string& key) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto map_key = std::make_pair(extension_id, key);
  // If the key matches, flush the state store. Otherwise, add it to the
  // map.
  if (map_key == waiting_for_map_key_) {
    state_store_->FlushForTesting(run_loop_.QuitWhenIdleClosure());
  } else {
    ids_and_keys_with_writes_.emplace(std::move(map_key));
  }
}

}  // namespace extensions
