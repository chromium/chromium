// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/shared_storage/shared_storage_saved_query_data.h"

#include <stdint.h>

#include <queue>

#include "base/functional/callback.h"

namespace content {

SharedStorageSavedQueryData::SharedStorageSavedQueryData() : index(-1) {}

SharedStorageSavedQueryData::SharedStorageSavedQueryData(uint32_t index)
    : index(index) {}

SharedStorageSavedQueryData::SharedStorageSavedQueryData(
    SharedStorageSavedQueryData&& other)
    : index(other.index), callbacks(std::move(other.callbacks)) {}

SharedStorageSavedQueryData::~SharedStorageSavedQueryData() = default;

SharedStorageSavedQueryData& SharedStorageSavedQueryData::operator=(
    SharedStorageSavedQueryData&&) = default;

}  // namespace content
