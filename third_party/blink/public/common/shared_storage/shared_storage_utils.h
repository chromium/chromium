// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_SHARED_STORAGE_SHARED_STORAGE_UTILS_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_SHARED_STORAGE_SHARED_STORAGE_UTILS_H_

#include <cstdlib>

#include "third_party/blink/public/common/common_export.h"

namespace blink {

// Whether the length of the urls input parameter (of the
// sharedStorage.runURLSelectionOperation method) is valid.
BLINK_COMMON_EXPORT bool IsValidSharedStorageURLsArrayLength(size_t length);

// Whether the length of a shared storage's key is valid.
BLINK_COMMON_EXPORT bool IsValidSharedStorageKeyStringLength(size_t length);

// Whether the length of shared storage's value is valid.
BLINK_COMMON_EXPORT bool IsValidSharedStorageValueStringLength(size_t length);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_SHARED_STORAGE_SHARED_STORAGE_UTILS_H_
