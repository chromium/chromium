// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_PRE_PREFETCH_HANDLE_H_
#define CONTENT_PUBLIC_BROWSER_PRE_PREFETCH_HANDLE_H_

#include "content/common/content_export.h"

namespace content {

// The interface to control PrePrefetch resources associated with this, like
// `PrefetchHandle`.
// It will own the resources needed for PrePrefetch (will be introduced as
// `PrePrefetchContainer`), which can be cancelled by `this dtor, or consumed on
// the main thread `PrefetchService`.
//
// Thread model:
//
// - Can be created/destroyed on any thread.
// - Can be passed across threads.
// TODO(crbug.com/452406598): Add more details once `PrePrefetchContainer` and
// its related logic is introduced.
class CONTENT_EXPORT PrePrefetchHandle {
 public:
  PrePrefetchHandle() = default;
  virtual ~PrePrefetchHandle() = default;

  PrePrefetchHandle(const PrePrefetchHandle& other) = delete;
  PrePrefetchHandle& operator=(const PrePrefetchHandle& other) = delete;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_PRE_PREFETCH_HANDLE_H_
