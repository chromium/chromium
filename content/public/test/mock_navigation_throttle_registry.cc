// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/mock_navigation_throttle_registry.h"

#include "base/check_deref.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"

namespace content {

MockNavigationThrottleRegistry::MockNavigationThrottleRegistry(
    NavigationHandle* navigation_handle,
    RegistrationMode registration_mode)
    : navigation_handle_(CHECK_DEREF(navigation_handle)),
      registration_mode_(registration_mode) {}

MockNavigationThrottleRegistry::~MockNavigationThrottleRegistry() = default;

NavigationHandle& MockNavigationThrottleRegistry::GetNavigationHandle() {
  return *navigation_handle_;
}

void MockNavigationThrottleRegistry::AddThrottle(
    std::unique_ptr<NavigationThrottle> throttle) {
  CHECK(throttle);
  switch (registration_mode_) {
    case RegistrationMode::kAutoRegistrationForTesting:
      navigation_handle_->RegisterThrottleForTesting(std::move(throttle));
      break;
    case RegistrationMode::kHold:
      throttles_.push_back(std::move(throttle));
      break;
  }
}

void MockNavigationThrottleRegistry::MaybeAddThrottle(
    std::unique_ptr<NavigationThrottle> throttle) {
  if (throttle) {
    AddThrottle(std::move(throttle));
  }
}

bool MockNavigationThrottleRegistry::ContainsHeldThrottle(
    const std::string& name) {
  CHECK_EQ(registration_mode_, RegistrationMode::kHold);

  for (auto& it : throttles_) {
    if (it->GetNameForLogging() == name) {
      return true;
    }
  }
  return false;
}

void MockNavigationThrottleRegistry::RegisterHeldThrottles() {
  CHECK_EQ(registration_mode_, RegistrationMode::kHold);

  for (auto& it : throttles_) {
    navigation_handle_->RegisterThrottleForTesting(std::move(it));
  }
  throttles_.clear();
}

}  // namespace content
