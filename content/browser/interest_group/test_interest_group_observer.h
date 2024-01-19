// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INTEREST_GROUP_TEST_INTEREST_GROUP_OBSERVER_H_
#define CONTENT_BROWSER_INTEREST_GROUP_TEST_INTEREST_GROUP_OBSERVER_H_

#include <map>
#include <memory>
#include <optional>
#include <ostream>
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
  struct Entry {
    Entry(
        std::string devtools_auction_id,
        InterestGroupManagerImpl::InterestGroupObserver::AccessType access_type,
        url::Origin owner_origin,
        std::string ig_name,
        std::optional<double> bid = std::nullopt,
        std::optional<std::string> bid_currency = std::nullopt,
        std::optional<url::Origin> component_seller_origin = std::nullopt);
    Entry(const Entry&);
    ~Entry();

    Entry& operator=(const Entry&);
    bool operator==(const Entry&) const;

    // The auction ID is replaced by either "global" or a number starting from
    // 1, strinfigied.
    std::string devtools_auction_id;
    InterestGroupManagerImpl::InterestGroupObserver::AccessType access_type;
    url::Origin owner_origin;
    std::string ig_name;
    std::optional<double> bid;
    std::optional<std::string> bid_currency;
    std::optional<url::Origin> component_seller_origin;
  };

  TestInterestGroupObserver();
  ~TestInterestGroupObserver() override;

  TestInterestGroupObserver(const TestInterestGroupObserver&) = delete;
  TestInterestGroupObserver& operator=(const TestInterestGroupObserver&) =
      delete;

  // InterestGroupManagerImpl::InterestGroupObserver implementation:
  void OnInterestGroupAccessed(
      base::optional_ref<const std::string> devtools_auction_id,
      base::Time access_time,
      InterestGroupManagerImpl::InterestGroupObserver::AccessType type,
      const url::Origin& owner_origin,
      const std::string& name,
      base::optional_ref<const url::Origin> component_seller_origin,
      std::optional<double> bid,
      base::optional_ref<const std::string> bid_currency) override;

  // Waits until exactly the expected events have seen, in the passed in order.
  // Once they have been observed, clears the log of the previous events. Note
  // that the vector includes events that happened before WaitForAccesses() was
  // invoked.
  void WaitForAccesses(const std::vector<Entry>& expected);

  // Just like WaitForAccesses(), but expects entries to be in the exact order
  // provided. Order of events logged on auction completion / using an auction
  // result (e.g., kBid, kWin) is not guaranteed. This should only be used for
  // events with a guaranteed order.
  void WaitForAccessesInOrder(const std::vector<Entry>& expected);

 private:
  std::map<std::string, std::string> seen_auction_ids_;
  std::vector<Entry> accesses_;
  std::vector<Entry> expected_;
  std::unique_ptr<base::RunLoop> run_loop_;
};

// Printer for gtest.
void PrintTo(const TestInterestGroupObserver::Entry& e, std::ostream* os);

}  // namespace content

#endif  // CONTENT_BROWSER_INTEREST_GROUP_TEST_INTEREST_GROUP_OBSERVER_H_
