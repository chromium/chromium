// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/shared_dictionary/shared_dictionary_manager_in_memory.h"

#include "base/functional/callback_helpers.h"
#include "services/network/shared_dictionary/shared_dictionary_storage_in_memory.h"

namespace network {

scoped_refptr<SharedDictionaryStorage>
SharedDictionaryManagerInMemory::CreateStorage(
    const SharedDictionaryStorageIsolationKey& isolation_key) {
  return base::MakeRefCounted<SharedDictionaryStorageInMemory>(
      base::ScopedClosureRunner(
          base::BindOnce(&SharedDictionaryManager::OnStorageDeleted,
                         GetWeakPtr(), isolation_key)));
}

}  // namespace network
