// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_ENGINE_BROWSER_FAKE_NAVIGATION_POLICY_PROVIDER_H_
#define FUCHSIA_ENGINE_BROWSER_FAKE_NAVIGATION_POLICY_PROVIDER_H_

#include <fuchsia/web/cpp/fidl.h>
#include <fuchsia/web/cpp/fidl_test_base.h>

class FakeNavigationPolicyProvider
    : public fuchsia::web::testing::NavigationPolicyProvider_TestBase {
 public:
  FakeNavigationPolicyProvider();
  ~FakeNavigationPolicyProvider() override;

  FakeNavigationPolicyProvider(const FakeNavigationPolicyProvider&) = delete;
  FakeNavigationPolicyProvider& operator=(const FakeNavigationPolicyProvider&) =
      delete;

  void set_should_reject_request(bool reject) {
    should_reject_request_ = reject;
  }

  fuchsia::web::RequestedNavigation* requested_navigation() {
    return &requested_navigation_;
  }

  // fuchsia::web::NavigationPolicyProvider implementation.
  void EvaluateRequestedNavigation(
      fuchsia::web::RequestedNavigation requested_navigation,
      EvaluateRequestedNavigationCallback callback) final;
  void NotImplemented_(const std::string& name) final;

 private:
  fuchsia::web::RequestedNavigation requested_navigation_;
  bool should_reject_request_ = false;
};

#endif  // FUCHSIA_ENGINE_BROWSER_FAKE_NAVIGATION_POLICY_PROVIDER_H_