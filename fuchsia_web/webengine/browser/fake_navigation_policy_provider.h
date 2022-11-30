// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_WEB_WEBENGINE_BROWSER_FAKE_NAVIGATION_POLICY_PROVIDER_H_
#define FUCHSIA_WEB_WEBENGINE_BROWSER_FAKE_NAVIGATION_POLICY_PROVIDER_H_

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

  void set_should_abort_navigation(bool should_abort_navigation) {
    should_abort_navigation_ = should_abort_navigation;
  }

  fuchsia::web::RequestedNavigation* requested_navigation() {
    return &requested_navigation_;
  }

  int num_evaluated_navigations() { return num_evaluated_navigations_; }

  // fuchsia::web::NavigationPolicyProvider implementation.
  void EvaluateRequestedNavigation(
      fuchsia::web::RequestedNavigation requested_navigation,
      EvaluateRequestedNavigationCallback callback) final;
  void NotImplemented_(const std::string& name) final;

 private:
  fuchsia::web::RequestedNavigation requested_navigation_;
  bool should_abort_navigation_ = false;
  int num_evaluated_navigations_ = 0;
};

#endif  // FUCHSIA_WEB_WEBENGINE_BROWSER_FAKE_NAVIGATION_POLICY_PROVIDER_H_