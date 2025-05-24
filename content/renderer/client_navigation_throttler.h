// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_CLIENT_NAVIGATION_THROTTLER_H_
#define CONTENT_RENDERER_CLIENT_NAVIGATION_THROTTLER_H_

#include <vector>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"

namespace content {

// This class manages client-side navigations by either dispatching them
// instantly or deferring for the duration of a handle issued by
// `DeferNavigations()` being alive.
class CONTENT_EXPORT ClientNavigationThrottler {
 public:
  ClientNavigationThrottler();
  ~ClientNavigationThrottler();

  // Navigations will be deferred until the returned handle goes out of scope.
  base::ScopedClosureRunner DeferNavigations();

  // The closure will be invoked synchronously by default, or, in case there
  // were calls to `DeferNavigations()` before, deferred until there are no
  // pending handles left.
  void DispatchOrScheduleNavigation(base::OnceClosure navigation);

 private:
  void UndeferNavigations();
  void DispatchPendingCallbacks();

  // Not using base::ScopedClosureRunner below, since we need specific
  // firing order, which neither std::vector<> destructor
  // nor std::vector::clear() can guarantee.
  std::vector<base::OnceClosure> deferred_navigations_;
  int defer_counter_ = 0;
  base::WeakPtrFactory<ClientNavigationThrottler> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_RENDERER_CLIENT_NAVIGATION_THROTTLER_H_
