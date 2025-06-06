// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/mock_navigation_throttle_registry.h"

#include "base/check_deref.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/test/mock_navigation_handle.h"

namespace content {

// A wrapper class for the NavigationRequest that holds a weak pointer to the
// NavigationRequest internally, and hide the NavigationRequest in the public
// header to avoid unexpected forbidden use outside //content.
// TODO(https://crbug.com/412524375): Update existing tests that needs this
// WeakPtr protection, and remove this class.
class MockNavigationThrottleRegistry::NavigationHandleHolder {
 public:
  explicit NavigationHandleHolder(NavigationHandle* navigation_handle)
      : navigation_request_(
            NavigationRequest::From(navigation_handle)->GetWeakPtr()) {}
  ~NavigationHandleHolder() = default;

  NavigationHandle* Get() {
    CHECK(navigation_request_);
    return navigation_request_.get();
  }

 private:
  base::WeakPtr<NavigationRequest> navigation_request_;
};

MockNavigationThrottleRegistry::MockNavigationThrottleRegistry(
    NavigationHandle* navigation_handle,
    RegistrationMode registration_mode)
    : navigation_handle_holder_(
          std::make_unique<NavigationHandleHolder>(navigation_handle)),
      registration_mode_(registration_mode) {}

MockNavigationThrottleRegistry::MockNavigationThrottleRegistry(
    MockNavigationHandle* mock_navigation_handle,
    RegistrationMode registration_mode)
    : mock_navigation_handle_(mock_navigation_handle),
      registration_mode_(registration_mode) {}


MockNavigationThrottleRegistry::~MockNavigationThrottleRegistry() = default;

NavigationHandle& MockNavigationThrottleRegistry::GetNavigationHandle() {
  if (mock_navigation_handle_) {
    return *mock_navigation_handle_;
  }
  return *navigation_handle_holder_->Get();
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

  auto& handle = GetNavigationHandle();
  for (auto& it : throttles_) {
    handle.RegisterThrottleForTesting(std::move(it));
  }
  throttles_.clear();
}

}  // namespace content
