// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/mock_navigation_throttle_registry.h"

#include "base/check_deref.h"
#include "base/notimplemented.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/test/mock_navigation_handle.h"

namespace content {

MockNavigationThrottleRegistry::MockNavigationThrottleRegistry(
    NavigationHandle* navigation_handle,
    RegistrationMode registration_mode)
    : navigation_handle_(navigation_handle),
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
      GetNavigationHandle().RegisterThrottleForTesting(std::move(throttle));
      break;
    case RegistrationMode::kHold:
      throttles_.push_back(std::move(throttle));
      break;
  }
}

bool MockNavigationThrottleRegistry::IsHTTPOrHTTPS() {
  return GetNavigationHandle().GetURL().SchemeIsHTTPOrHTTPS();
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

  auto& handle = GetNavigationHandle();
  for (auto& it : throttles_) {
    handle.RegisterThrottleForTesting(std::move(it));
  }
  throttles_.clear();
}

bool MockNavigationThrottleRegistry::HasThrottle(const std::string& name) {
  NOTIMPLEMENTED();
  return false;
}

bool MockNavigationThrottleRegistry::EraseThrottleForTesting(
    const std::string& name) {
  NOTIMPLEMENTED();
  return false;
}

}  // namespace content
