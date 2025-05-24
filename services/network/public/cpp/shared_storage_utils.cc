// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/shared_storage_utils.h"

#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/shared_storage.mojom.h"

namespace network {

namespace {

size_t MaxChar16StringLength() {
  // Each char16_t takes 2 bytes.
  return kMaxSharedStorageBytesPerOrigin / 2u;
}

}  // namespace

const char kReservedLockNameErrorMessage[] = "Lock name cannot start with '-'";

const char kBatchUpdateMethodsArgumentValidationErrorMessage[] =
    "The 'withLock' option is not allowed for methods within batchUpdate()";

bool IsReservedLockName(base::optional_ref<const std::string> lock_name) {
  return lock_name && lock_name->starts_with('-');
}

bool IsValidSharedStorageBatchUpdateMethodsArgument(
    const std::vector<
        network::mojom::SharedStorageModifierMethodWithOptionsPtr>&
        methods_with_options) {
  if (!base::FeatureList::IsEnabled(
          features::kSharedStorageTransactionalBatchUpdate)) {
    return true;
  }

  for (const auto& method : methods_with_options) {
    if (method->with_lock) {
      return false;
    }
  }
  return true;
}

bool IsValidSharedStorageKeyStringLength(size_t length) {
  return length != 0u && length <= MaxChar16StringLength();
}

bool IsValidSharedStorageValueStringLength(size_t length) {
  return length <= MaxChar16StringLength();
}

}  // namespace network
