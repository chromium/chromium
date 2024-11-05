// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_SHARED_STORAGE_UTILS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_SHARED_STORAGE_UTILS_H_

#include <cstdlib>

#include "base/component_export.h"

namespace network {

// We use a max of 5 MB = 5 * 1024 * 1024 B = 5242880 B.
static constexpr size_t kMaxSharedStorageBytesPerOrigin = 5242880;

// Whether the length of a shared storage's key is valid.
COMPONENT_EXPORT(NETWORK_CPP_SHARED_STORAGE)
bool IsValidSharedStorageKeyStringLength(size_t length);

// Whether the length of shared storage's value is valid.
COMPONENT_EXPORT(NETWORK_CPP_SHARED_STORAGE)
bool IsValidSharedStorageValueStringLength(size_t length);

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_SHARED_STORAGE_UTILS_H_
