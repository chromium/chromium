// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_IDENTITY_TEST_ENVIRONMENT_CHROME_BROWSER_STATE_ADAPTOR_H_
#define IOS_CHROME_BROWSER_SIGNIN_IDENTITY_TEST_ENVIRONMENT_CHROME_BROWSER_STATE_ADAPTOR_H_

#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "services/identity/public/cpp/identity_test_environment.h"

// Adaptor that supports identity::IdentityTestEnvironment's usage in testing
// contexts where the relevant fake objects must be injected via the
// BrowserStateKeyedServiceFactory infrastructure as the production code
// accesses IdentityManager via that infrastructure. Before using this
// class, please consider whether you can change the production code in question
// to take in the relevant dependencies directly rather than obtaining them from
// the ChromeBrowserState; this is both cleaner in general and allows for direct
// usage of identity::IdentityTestEnvironment in the test.
class IdentityTestEnvironmentChromeBrowserStateAdaptor {
 public:
  // Creates and returns a TestChromeBrowserState that has been configured with
  // the set of testing factories that IdentityTestEnvironment requires.
  static std::unique_ptr<TestChromeBrowserState>
  CreateChromeBrowserStateForIdentityTestEnvironment();

  // Like the above, but additionally configures the returned ChromeBrowserState
  // with |input_factories|.
  static std::unique_ptr<TestChromeBrowserState>
  CreateChromeBrowserStateForIdentityTestEnvironment(
      const TestChromeBrowserState::TestingFactories& input_factories);

  // Appends the set of testing factories that identity::IdentityTestEnvironment
  // requires to |factories_to_append_to|, which should be the set of testing
  // factories supplied to TestChromeBrowserState (via one of the various
  // mechanisms for doing so). Prefer the above API if possible, as it is less
  // fragile. This API is primarily for use in tests that do not create the
  // TestChromeBrowserState internally but rather simply supply the set of
  // TestingFactories to some external facility (e.g., a superclass).
  static void AppendIdentityTestEnvironmentFactories(
      TestChromeBrowserState::TestingFactories* factories_to_append_to);

  // Constructs an adaptor that associates an IdentityTestEnvironment instance
  // with |browser_state| via the relevant backing objects. Note that
  // |browser_state| must have been configured with the IdentityTestEnvironment
  // testing factories, either because it was created via
  // CreateChromeBrowserStateForIdentityTestEnvironment() or because
  // AppendIdentityTestEnvironmentFactories() was invoked on the set of
  // factories supplied to it.
  // |browser_state| must outlive this object.
  explicit IdentityTestEnvironmentChromeBrowserStateAdaptor(
      ios::ChromeBrowserState* browser_state);
  ~IdentityTestEnvironmentChromeBrowserStateAdaptor() {}

  // Returns the IdentityTestEnvironment associated with this object (and
  // implicitly with the ChromeBrowserState passed to this object's
  // constructor).
  identity::IdentityTestEnvironment* identity_test_env() {
    return &identity_test_env_;
  }

 private:
  identity::IdentityTestEnvironment identity_test_env_;

  DISALLOW_COPY_AND_ASSIGN(IdentityTestEnvironmentChromeBrowserStateAdaptor);
};

#endif  // IOS_CHROME_BROWSER_SIGNIN_IDENTITY_TEST_ENVIRONMENT_CHROME_BROWSER_STATE_ADAPTOR_H_
