// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_SHARED_STORAGE_UTILS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_SHARED_STORAGE_UTILS_H_

#include <cstdlib>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/types/optional_ref.h"
#include "services/network/public/mojom/shared_storage.mojom-forward.h"

namespace network {

// We use a max of 5 MB = 5 * 1024 * 1024 B = 5242880 B.
static constexpr size_t kMaxSharedStorageBytesPerOrigin = 5242880;

COMPONENT_EXPORT(NETWORK_CPP_SHARED_STORAGE)
extern const char kReservedLockNameErrorMessage[];

COMPONENT_EXPORT(NETWORK_CPP_SHARED_STORAGE)
extern const char kBatchUpdateMethodsArgumentValidationErrorMessage[];

// Whether `lock_name` is a reserved lock resource name.
// See https://w3c.github.io/web-locks/#resource-name
COMPONENT_EXPORT(NETWORK_CPP_SHARED_STORAGE)
bool IsReservedLockName(base::optional_ref<const std::string> lock_name);

// Whether `methods_with_options` is a valid batchUpdate() argument: according
// to the specification (https://wicg.github.io/shared-storage/#batch-update),
// none of the inner methods should specify the `with_lock` option.
COMPONENT_EXPORT(NETWORK_CPP_SHARED_STORAGE)
bool IsValidSharedStorageBatchUpdateMethodsArgument(
    const std::vector<
        network::mojom::SharedStorageModifierMethodWithOptionsPtr>&
        methods_with_options);

// Whether the length of a shared storage's key is valid.
COMPONENT_EXPORT(NETWORK_CPP_SHARED_STORAGE)
bool IsValidSharedStorageKeyStringLength(size_t length);

// Whether the length of shared storage's value is valid.
COMPONENT_EXPORT(NETWORK_CPP_SHARED_STORAGE)
bool IsValidSharedStorageValueStringLength(size_t length);

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_SHARED_STORAGE_UTILS_H_
