// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_PREFETCH_HANDLE_H_
#define CONTENT_PUBLIC_BROWSER_PREFETCH_HANDLE_H_

#include <memory>
#include <optional>

#include "base/functional/callback_forward.h"
#include "content/common/content_export.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"

namespace network {
struct URLLoaderCompletionStatus;
}  // namespace network

namespace content {

// The interface to control prefetch resources associated with this.
// If the handle is destructed, it will notify PrefetchService that the
// corresponding PrefetchContainer is no longer needed. PrefetchService will try
// to release relevant resources by its own decision with best-effort.
//
// Threading model:
// - All methods (except for move ctor/operator) must be called on the UI
//   thread.
// - Must be destroyed on the UI thread.
// - The callbacks of `Set*Callback()` are called on the UI thread.
class PrefetchHandle {
 public:
  PrefetchHandle() = default;
  virtual ~PrefetchHandle() = default;

  PrefetchHandle(const PrefetchHandle& other) = delete;
  PrefetchHandle& operator=(const PrefetchHandle& other) = delete;
  PrefetchHandle(PrefetchHandle&& other) = default;
  PrefetchHandle& operator=(PrefetchHandle&& other) = default;

  // Sets a callback called when non-redirect header is successfully received.
  virtual void SetOnPrefetchHeadReceivedCallback(
      base::RepeatingCallback<void(const network::mojom::URLResponseHead&)>
          on_prefetch_head_received) = 0;
  // Sets a callback called when loading of prefetch failed.
  virtual void SetOnPrefetchCompletedOrFailedCallback(
      base::RepeatingCallback<
          void(const network::URLLoaderCompletionStatus& completion_status,
               const std::optional<int>& response_code)>
          on_prefetch_completed_or_failed) = 0;

  // Returns true if the underlying `PrefetchContainer` is alive.
  virtual bool IsAlive() const = 0;
};

// The cross-thread version of `PrefetchHandle` that can be owned by any thread.
// Due to the cross-thread nature, currently this is only for keep-alive, and
// other methods of `PrefetchHandle` are intentionally dropped as they can't be
// trivially called from non-UI thread.
//
// Threading model:
// - Can be created, destroyed or moved on any thread.
class CONTENT_EXPORT CrossThreadPrefetchHandle final {
 public:
  static std::unique_ptr<CrossThreadPrefetchHandle> Create(
      std::unique_ptr<content::PrefetchHandle> prefetch_handle);
  ~CrossThreadPrefetchHandle();

  CrossThreadPrefetchHandle(const CrossThreadPrefetchHandle& other) = delete;
  CrossThreadPrefetchHandle& operator=(const CrossThreadPrefetchHandle& other) =
      delete;
  CrossThreadPrefetchHandle(CrossThreadPrefetchHandle&& other);
  CrossThreadPrefetchHandle& operator=(CrossThreadPrefetchHandle&& other);

 private:
  explicit CrossThreadPrefetchHandle(
      std::unique_ptr<content::PrefetchHandle> prefetch_handle);
  void Reset();

  // Must be destructed and dereferenced only on the UI thread.
  std::unique_ptr<content::PrefetchHandle> prefetch_handle_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_PREFETCH_HANDLE_H_
