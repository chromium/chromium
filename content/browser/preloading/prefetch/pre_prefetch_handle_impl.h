// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PREFETCH_PRE_PREFETCH_HANDLE_IMPL_H_
#define CONTENT_BROWSER_PRELOADING_PREFETCH_PRE_PREFETCH_HANDLE_IMPL_H_

#include "content/browser/preloading/prefetch/pre_prefetch_container.h"
#include "content/public/browser/pre_prefetch_handle.h"

namespace content {

// Please see pre_prefetch_handle.h for the thread model and more details.
class PrePrefetchHandleImpl final : public PrePrefetchHandle {
 public:
  explicit PrePrefetchHandleImpl(
      std::unique_ptr<PrePrefetchContainer> pre_prefetch_container);
  ~PrePrefetchHandleImpl() override;

 private:
  std::unique_ptr<PrePrefetchContainer> pre_prefetch_container_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PREFETCH_PRE_PREFETCH_HANDLE_IMPL_H_
