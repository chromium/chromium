// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SHARED_STORAGE_SHARED_STORAGE_SAVED_QUERY_DATA_H_
#define CONTENT_BROWSER_SHARED_STORAGE_SHARED_STORAGE_SAVED_QUERY_DATA_H_

#include <stdint.h>

#include <queue>

#include "base/functional/callback.h"
#include "content/common/content_export.h"

namespace content {

// Bundles `index`, which is either -1 (indicates that the query's result is
// still pending) or a nonnegative integer value denoting the saved index
// result, along with `callbacks` (if any) to be run when a pending query result
// is resolved.
struct CONTENT_EXPORT SharedStorageSavedQueryData {
  SharedStorageSavedQueryData();
  explicit SharedStorageSavedQueryData(uint32_t index);
  SharedStorageSavedQueryData(const SharedStorageSavedQueryData&) = delete;
  SharedStorageSavedQueryData(SharedStorageSavedQueryData&& other);
  ~SharedStorageSavedQueryData();
  SharedStorageSavedQueryData operator=(const SharedStorageSavedQueryData&) =
      delete;
  SharedStorageSavedQueryData& operator=(SharedStorageSavedQueryData&& other);

  // `index` is either -1 (denotes that the result is pending) or a nonnegative
  // integer denoting the actual index result value.
  int32_t index;

  // In the case where the `index` is pending, callbacks are pushed to
  // `callbacks` each time, excluding during the initial access, that the
  // pending result is accessed while it is still in a pending state. When the
  // result is resolved, these `callbacks` will be dequeued and run.
  std::queue<base::OnceCallback<void(uint32_t)>> callbacks;
};

}  // namespace content

#endif  // CONTENT_BROWSER_SHARED_STORAGE_SHARED_STORAGE_SAVED_QUERY_DATA_H_
