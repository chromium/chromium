// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/test_interest_group_observer.h"

#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include "base/run_loop.h"
#include "base/time/time.h"
#include "content/browser/interest_group/interest_group_manager_impl.h"
#include "content/common/content_export.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

namespace content {

TestInterestGroupObserver::TestInterestGroupObserver() = default;

TestInterestGroupObserver::~TestInterestGroupObserver() = default;

void TestInterestGroupObserver::OnInterestGroupAccessed(
    const base::Time& access_time,
    InterestGroupManagerImpl::InterestGroupObserver::AccessType type,
    const url::Origin& owner_origin,
    const std::string& name) {
  accesses_.emplace_back(type, owner_origin, name);

  if (run_loop_ && accesses_.size() >= expected_.size()) {
    run_loop_->Quit();
  }
}

void TestInterestGroupObserver::WaitForAccesses(
    const std::vector<Entry>& expected) {
  DCHECK(!run_loop_);
  if (accesses_.size() < expected.size()) {
    run_loop_ = std::make_unique<base::RunLoop>();
    expected_ = expected;
    run_loop_->Run();
    run_loop_.reset();
  }
  EXPECT_THAT(accesses_, ::testing::UnorderedElementsAreArray(expected));

  // Clear accesses so can be reused.
  accesses_.clear();
}

void TestInterestGroupObserver::WaitForAccessesInOrder(
    const std::vector<Entry>& expected) {
  DCHECK(!run_loop_);
  if (accesses_.size() < expected.size()) {
    run_loop_ = std::make_unique<base::RunLoop>();
    expected_ = expected;
    run_loop_->Run();
    run_loop_.reset();
  }
  EXPECT_THAT(accesses_, ::testing::ElementsAreArray(expected));

  // Clear accesses so can be reused.
  accesses_.clear();
}

}  // namespace content
