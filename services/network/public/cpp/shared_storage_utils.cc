// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/shared_storage_utils.h"

namespace network {

namespace {

size_t MaxChar16StringLength() {
  // Each char16_t takes 2 bytes.
  return kMaxSharedStorageBytesPerOrigin / 2u;
}

}  // namespace

bool IsValidSharedStorageKeyStringLength(size_t length) {
  return length != 0u && length <= MaxChar16StringLength();
}

bool IsValidSharedStorageValueStringLength(size_t length) {
  return length <= MaxChar16StringLength();
}

}  // namespace network
