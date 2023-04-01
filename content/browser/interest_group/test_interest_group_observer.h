// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INTEREST_GROUP_TEST_INTEREST_GROUP_OBSERVER_H_
#define CONTENT_BROWSER_INTEREST_GROUP_TEST_INTEREST_GROUP_OBSERVER_H_

#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include "base/run_loop.h"
#include "base/time/time.h"
#include "content/browser/interest_group/interest_group_manager_impl.h"
#include "url/origin.h"

namespace content {

// Reusable InterestGroupObserver that logs all events that occur and can wait
// until an expected set of events is observed.
class TestInterestGroupObserver
    : public InterestGroupManagerImpl::InterestGroupObserver {
 public:
  using Entry =
      std::tuple<InterestGroupManagerImpl::InterestGroupObserver::AccessType,
                 url::Origin,
                 std::string>;

  TestInterestGroupObserver();
  ~TestInterestGroupObserver() override;

  TestInterestGroupObserver(const TestInterestGroupObserver&) = delete;
  TestInterestGroupObserver& operator=(const TestInterestGroupObserver&) =
      delete;

  // InterestGroupManagerImpl::InterestGroupObserver implementation:
  void OnInterestGroupAccessed(
      const base::Time& access_time,
      InterestGroupManagerImpl::InterestGroupObserver::AccessType type,
      const url::Origin& owner_origin,
      const std::string& name) override;

  // Waits until exactly the expected events have seen, in the passed in order.
  // Once they have been observed, clears the log of the previous events. Note
  // that the vector includes events that happened before WaitForAccesses() was
  // invoked.
  void WaitForAccesses(const std::vector<Entry>& expected);

 private:
  std::vector<Entry> accesses_;
  std::vector<Entry> expected_;
  std::unique_ptr<base::RunLoop> run_loop_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_INTEREST_GROUP_TEST_INTEREST_GROUP_OBSERVER_H_
