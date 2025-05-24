// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_MOCK_NAVIGATION_THROTTLE_REGISTRY_H_
#define CONTENT_PUBLIC_TEST_MOCK_NAVIGATION_THROTTLE_REGISTRY_H_

#include "base/memory/raw_ref.h"
#include "content/public/browser/navigation_throttle_registry.h"

namespace content {

// This class implements NavigationThrottleRegistry functionalities with
// testing features. Tests that needs one of following functions may use
// this class.
// - Register a testing purpose NavigationThrottle after other
//   NavigationThrottles so that the throttle can run after them all.
// - Pass it instead of the real implementation to check if a module
//   under testing registers a target throttle.
// If you want to register your testing throttle to the real registry, consider
// using content::TestNavigationThrottleInserter instead.
class MockNavigationThrottleRegistry : public NavigationThrottleRegistry {
 public:
  enum class RegistrationMode {
    // AddThrottle() and MaybeAddThrottle() register the passed throttle as a
    // testing purpose throttle that runs after other production throttles.
    kAutoRegistrationForTesting,

    // AddThrottle() and MaybeAddThrottle() don't register the passed throttle
    // actually, but hold it in the mock. Users can query the hold throttles by
    // throttles(), or call ContainsHeldThrottle() to check if AddThrottle() or
    // MaybeAddThrottle() was called with a specific throttle. The held
    // throttles can be registered manually via RegisterHeldThrottles().
    kHold,
  };
  explicit MockNavigationThrottleRegistry(
      NavigationHandle* navigation_handle,
      RegistrationMode registration_mode =
          RegistrationMode::kAutoRegistrationForTesting);
  ~MockNavigationThrottleRegistry() override;

  // Implements NavigationThrottleRegistry:
  NavigationHandle& GetNavigationHandle() override;
  void AddThrottle(std::unique_ptr<NavigationThrottle> throttle) override;
  void MaybeAddThrottle(std::unique_ptr<NavigationThrottle> throttle) override;

  // Checks if the registry running with `kHold` mode contains a throttle with
  // the given name.
  bool ContainsHeldThrottle(const std::string& name);

  // Registers the held `throttles_` that are added in the registry running with
  /// `kHold` mode. The throttles are removed from `throttles_` and will run for
  // the underlying navigation.
  void RegisterHeldThrottles();

  std::vector<std::unique_ptr<NavigationThrottle>>& throttles() {
    return throttles_;
  }

 private:
  const raw_ref<NavigationHandle> navigation_handle_;
  const RegistrationMode registration_mode_;
  std::vector<std::unique_ptr<NavigationThrottle>> throttles_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_MOCK_NAVIGATION_THROTTLE_REGISTRY_H_
