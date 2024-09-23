// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/local_session_policies_provider.h"

#include <memory>

#include "base/callback_list.h"
#include "base/test/mock_callback.h"
#include "base/time/time.h"
#include "remoting/base/session_policies.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

class LocalSessionPoliciesProviderTest : public testing::Test {
 protected:
  LocalSessionPoliciesProvider provider_;
};

TEST_F(LocalSessionPoliciesProviderTest, ProvideEmptyPoliciesByDefault) {
  EXPECT_EQ(provider_.get_local_policies(), SessionPolicies());
}

TEST_F(LocalSessionPoliciesProviderTest,
       PoliciesChanged_UpdatePoliciesAndNotifyCallbacks) {
  base::MockCallback<LocalSessionPoliciesProvider::LocalPoliciesChangedCallback>
      callback;
  auto subscription = provider_.AddLocalPoliciesChangedCallback(callback.Get());
  SessionPolicies new_policies = {.maximum_session_duration = base::Hours(10)};
  EXPECT_CALL(callback, Run(new_policies)).Times(1);
  provider_.set_local_policies(new_policies);
  EXPECT_EQ(provider_.get_local_policies(), new_policies);
}

TEST_F(LocalSessionPoliciesProviderTest,
       PoliciesNotChanged_CallbacksNotNotified) {
  SessionPolicies new_policies = {.maximum_session_duration = base::Hours(10)};
  provider_.set_local_policies(new_policies);

  base::MockCallback<LocalSessionPoliciesProvider::LocalPoliciesChangedCallback>
      callback;
  auto subscription = provider_.AddLocalPoliciesChangedCallback(callback.Get());
  provider_.set_local_policies(new_policies);
  EXPECT_CALL(callback, Run(new_policies)).Times(0);
}

TEST_F(LocalSessionPoliciesProviderTest,
       SubscriptionDiscarded_CallbackNoLongerNotified) {
  base::MockCallback<LocalSessionPoliciesProvider::LocalPoliciesChangedCallback>
      callback;
  auto subscription = std::make_unique<base::CallbackListSubscription>(
      provider_.AddLocalPoliciesChangedCallback(callback.Get()));
  subscription.reset();
  SessionPolicies new_policies = {.maximum_session_duration = base::Hours(10)};
  EXPECT_CALL(callback, Run(new_policies)).Times(0);
  provider_.set_local_policies(new_policies);
}

}  // namespace remoting
