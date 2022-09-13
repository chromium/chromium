// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/webengine/browser/fake_navigation_policy_provider.h"

#include "base/notreached.h"
#include "testing/gtest/include/gtest/gtest.h"

FakeNavigationPolicyProvider::FakeNavigationPolicyProvider() = default;

FakeNavigationPolicyProvider::~FakeNavigationPolicyProvider() = default;

void FakeNavigationPolicyProvider::EvaluateRequestedNavigation(
    fuchsia::web::RequestedNavigation requested_navigation,
    EvaluateRequestedNavigationCallback callback) {
  fuchsia::web::NavigationDecision decision;
  if (should_abort_navigation_) {
    callback(decision.WithAbort(fuchsia::web::NoArgumentsAction()));
  } else {
    callback(decision.WithProceed(fuchsia::web::NoArgumentsAction()));
  }

  requested_navigation_ = std::move(requested_navigation);
  num_evaluated_navigations_++;
}

void FakeNavigationPolicyProvider::NotImplemented_(const std::string& name) {
  NOTIMPLEMENTED() << name;
}
