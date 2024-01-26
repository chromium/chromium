// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/test_interest_group_observer.h"

#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "content/browser/interest_group/interest_group_manager_impl.h"
#include "content/common/content_export.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

namespace content {

TestInterestGroupObserver::Entry::Entry(
    std::string devtools_auction_id,
    InterestGroupManagerImpl::InterestGroupObserver::AccessType access_type,
    url::Origin owner_origin,
    std::string ig_name,
    std::optional<double> bid,
    std::optional<std::string> bid_currency,
    std::optional<url::Origin> component_seller_origin)
    : devtools_auction_id(std::move(devtools_auction_id)),
      access_type(access_type),
      owner_origin(std::move(owner_origin)),
      ig_name(std::move(ig_name)),
      bid(std::move(bid)),
      bid_currency(std::move(bid_currency)),
      component_seller_origin(std::move(component_seller_origin)) {}

TestInterestGroupObserver::Entry::Entry(const Entry&) = default;
TestInterestGroupObserver::Entry::~Entry() = default;

TestInterestGroupObserver::Entry& TestInterestGroupObserver::Entry::operator=(
    const Entry&) = default;
bool TestInterestGroupObserver::Entry::operator==(const Entry&) const = default;

TestInterestGroupObserver::TestInterestGroupObserver() = default;

TestInterestGroupObserver::~TestInterestGroupObserver() = default;

void TestInterestGroupObserver::OnInterestGroupAccessed(
    base::optional_ref<const std::string> devtools_auction_id,
    base::Time access_time,
    InterestGroupManagerImpl::InterestGroupObserver::AccessType type,
    const url::Origin& owner_origin,
    const std::string& name,
    base::optional_ref<const url::Origin> component_seller_origin,
    std::optional<double> bid,
    base::optional_ref<const std::string> bid_currency) {
  // Hide the randomness of auction IDs for easier testing by renaming them to
  // sequential integers (and "global" if not set)
  std::string normalized_auction_id = "global";
  if (devtools_auction_id.has_value()) {
    auto it = seen_auction_ids_.find(*devtools_auction_id);
    if (it != seen_auction_ids_.end()) {
      normalized_auction_id = it->second;
    } else {
      normalized_auction_id =
          base::NumberToString(seen_auction_ids_.size() + 1);
      seen_auction_ids_[*devtools_auction_id] = normalized_auction_id;
    }
  }

  accesses_.emplace_back(normalized_auction_id, type, owner_origin, name, bid,
                         bid_currency.CopyAsOptional(),
                         component_seller_origin.CopyAsOptional());

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

void PrintTo(const TestInterestGroupObserver::Entry& e, std::ostream* os) {
  *os << "(";
  *os << e.devtools_auction_id << ", ";
  *os << e.access_type << ", ";
  *os << e.owner_origin.Serialize() << ", ";
  *os << e.ig_name << ", ";
  if (e.bid.has_value()) {
    *os << e.bid.value() << ", ";
  } else {
    *os << "(no bid), ";
  }
  if (e.bid_currency.has_value()) {
    *os << e.bid_currency.value() << ", ";
  } else {
    *os << "(no bid_currency), ";
  }
  if (e.component_seller_origin.has_value()) {
    *os << e.component_seller_origin->Serialize();
  } else {
    *os << "(no component_seller_origin)";
  }
  *os << ")";
}

}  // namespace content
