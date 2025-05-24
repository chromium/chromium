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

enum class PrefetchStatus;

class PrefetchHandleImpl final : public PrefetchHandle {
 public:
  PrefetchHandleImpl(base::WeakPtr<PrefetchService> prefetch_service,
                     base::WeakPtr<PrefetchContainer> prefetch_container);
  ~PrefetchHandleImpl() override;

  // `PrefetchHandle` implementations
  bool IsAlive() const override;

  // TODO(crbug.com/390329781): The following methods are tentative interface
  // for incrementally migrating `//content/` internal code to `PrefetchHandle`.
  // Revisit the semantics when exposing for the public API.

  // Sets the `PrefetchStatus` that will be set to the `PrefetchContainer` when
  // this handle is destroyed and the `PrefetchContainer`'s `LoadState` is
  // `kStarted` at that time.
  void SetPrefetchStatusOnReleaseStartedPrefetch(
      PrefetchStatus prefetch_status_on_release_started_prefetch);

 private:
  base::WeakPtr<PrefetchService> prefetch_service_;
  base::WeakPtr<PrefetchContainer> prefetch_container_;
  std::optional<PrefetchStatus> prefetch_status_on_release_started_prefetch_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_HANDLE_IMPL_H_
