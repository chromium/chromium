// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_HANDLE_IMPL_H_
#define CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_HANDLE_IMPL_H_

#include "base/memory/weak_ptr.h"
#include "content/browser/preloading/prefetch/prefetch_container.h"
#include "content/browser/preloading/prefetch/prefetch_service.h"
#include "content/public/browser/prefetch_handle.h"
#include "content/public/browser/preloading.h"

namespace content {

class PrefetchHandleImpl final : public PrefetchHandle {
 public:
  PrefetchHandleImpl(base::WeakPtr<PrefetchService> prefetch_service,
                     base::WeakPtr<PrefetchContainer> prefetch_container);
  ~PrefetchHandleImpl() override;

 private:
  base::WeakPtr<PrefetchService> prefetch_service_;
  base::WeakPtr<PrefetchContainer> prefetch_container_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_HANDLE_IMPL_H_
