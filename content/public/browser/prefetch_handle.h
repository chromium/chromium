// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_PREFETCH_HANDLE_H_
#define CONTENT_PUBLIC_BROWSER_PREFETCH_HANDLE_H_

#include <optional>

#include "base/functional/callback_forward.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"

namespace network {
struct URLLoaderCompletionStatus;
}  // namespace network

namespace content {

// The interface to control prefetch resources associated with this.
// If the handle is destructed, it will notify PrefetchService that the
// corresponding PrefetchContainer is no longer needed. PrefetchService will try
// to release relevant resources by its own decision with best-effort.
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

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_PREFETCH_HANDLE_H_
