// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_NAVIGATION_THROTTLE_REGISTRY_H_
#define CONTENT_PUBLIC_BROWSER_NAVIGATION_THROTTLE_REGISTRY_H_

#include <memory>
#include <string>

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
  // AddThrottle() disallows `navigation_throttle` to be nullptr.
  virtual void AddThrottle(
      std::unique_ptr<NavigationThrottle> navigation_throttle) = 0;

  // Checks if the registry contains a throttle with the given name. Returns
  // true if the throttle is found.
  virtual bool HasThrottle(const std::string& name) = 0;

  // Erases the throttle with the given name from the registry. This is only
  // used for testing. Returns true if the throttle is found and erased.
  virtual bool EraseThrottleForTesting(const std::string& name) = 0;

  // Attribute check APIs follow. Recommend to use them instead of directly
  // accessing the NavigationHandle as they could be optimized for repeated
  // queries.

  // Returns true if the navigation request is a request that will be sent to
  // the network over HTTP(S).
  // This is an experimental attribute, and should be called only in the
  // NavigationThrottle registration phase as URL could be changed later.
  virtual bool IsHTTPOrHTTPS() = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_NAVIGATION_THROTTLE_REGISTRY_H_
