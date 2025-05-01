// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_NAVIGATION_THROTTLE_REGISTRY_H_
#define CONTENT_PUBLIC_BROWSER_NAVIGATION_THROTTLE_REGISTRY_H_

#include <memory>

#include "content/common/content_export.h"

namespace content {
class NavigationHandle;
class NavigationThrottle;

// This class provides an interface to register NavigationThrottles for a new
// navigation.
class CONTENT_EXPORT NavigationThrottleRegistry {
 public:
  virtual ~NavigationThrottleRegistry() = default;

  // Retrieves the NavigationHandle associated with this registry.
  virtual NavigationHandle& GetNavigationHandle() = 0;

  // Takes ownership of `navigation_throttle`. Following this call, any
  // NavigationThrottle event processed for the associated NavigationHandle will
  // be called on `navigation_throttle`.
  // AddThrottle() disallows `navigation_throttle` to be nullptr. If you may
  // pass a nullptr, use MaybeAddThrottle() instead. It just ignores calls with
  // a nullptr.
  virtual void AddThrottle(
      std::unique_ptr<NavigationThrottle> navigation_throttle) = 0;
  virtual void MaybeAddThrottle(
      std::unique_ptr<NavigationThrottle> navigation_throttle) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_NAVIGATION_THROTTLE_REGISTRY_H_
