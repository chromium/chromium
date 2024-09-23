// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/mock_browsing_data_remover_delegate.h"

#include <sstream>

#include "base/functional/callback.h"
#include "base/notreached.h"
#include "content/browser/browsing_data/browsing_data_filter_builder_impl.h"
#include "content/public/browser/browsing_data_filter_builder.h"
#include "net/cookies/cookie_partition_key.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

MockBrowsingDataRemoverDelegate::MockBrowsingDataRemoverDelegate() {}

MockBrowsingDataRemoverDelegate::~MockBrowsingDataRemoverDelegate() {
  DCHECK(!expected_calls_.size()) << "Expectations were set but not verified.";
}

BrowsingDataRemoverDelegate::EmbedderOriginTypeMatcher
MockBrowsingDataRemoverDelegate::GetOriginTypeMatcher() {
  return BrowsingDataRemoverDelegate::EmbedderOriginTypeMatcher();
}

bool MockBrowsingDataRemoverDelegate::MayRemoveDownloadHistory() {
  return true;
}

std::vector<std::string>
MockBrowsingDataRemoverDelegate::GetDomainsForDeferredCookieDeletion(
    StoragePartition* storage_partition,
    uint64_t remove_mask) {
  return {};
}

void MockBrowsingDataRemoverDelegate::RemoveEmbedderData(
    const base::Time& delete_begin,
    const base::Time& delete_end,
    uint64_t remove_mask,
    BrowsingDataFilterBuilder* filter_builder,
    uint64_t origin_type_mask,
    base::OnceCallback<void(uint64_t)> callback) {
  actual_calls_.emplace_back(delete_begin, delete_end, remove_mask,
                             origin_type_mask, filter_builder->Copy(),
                             true /* should_compare_filter */);
  std::move(callback).Run(/*failed_data_types=*/0);
}

void MockBrowsingDataRemoverDelegate::ExpectCall(
    const base::Time& delete_begin,
    const base::Time& delete_end,
    uint64_t remove_mask,
    uint64_t origin_type_mask,
    BrowsingDataFilterBuilder* filter_builder) {
  expected_calls_.emplace_back(delete_begin, delete_end, remove_mask,
                               origin_type_mask, filter_builder->Copy(),
                               true /* should_compare_filter */);
}

void MockBrowsingDataRemoverDelegate::ExpectCallDontCareAboutFilterBuilder(
    const base::Time& delete_begin,
    const base::Time& delete_end,
    uint64_t remove_mask,
    uint64_t origin_type_mask) {
  expected_calls_.emplace_back(delete_begin, delete_end, remove_mask,
                               origin_type_mask,
                               BrowsingDataFilterBuilder::Create(
                                   BrowsingDataFilterBuilder::Mode::kPreserve),
                               false /* should_compare_filter */);
}

void MockBrowsingDataRemoverDelegate::VerifyAndClearExpectations() {
  EXPECT_EQ(expected_calls_.size(), actual_calls_.size())
      << expected_calls_.size() << " calls were expected, but "
      << actual_calls_.size() << " were made.";

  if (expected_calls_.size() == actual_calls_.size()) {
    auto actual = actual_calls_.begin();
    int count = 0;
    for (auto expected = expected_calls_.begin();
         expected != expected_calls_.end(); expected++, actual++, count++) {
      EXPECT_EQ(*expected, *actual) << "Call #" << count << " differs.";
    }
  }

  expected_calls_.clear();
  actual_calls_.clear();
}

MockBrowsingDataRemoverDelegate::CallParameters::CallParameters(
    const base::Time& delete_begin,
    const base::Time& delete_end,
    uint64_t remove_mask,
    uint64_t origin_type_mask,
    std::unique_ptr<BrowsingDataFilterBuilder> filter_builder,
    bool should_compare_filter)
    : delete_begin_(delete_begin),
      delete_end_(delete_end),
      remove_mask_(remove_mask),
      origin_type_mask_(origin_type_mask),
      filter_builder_(std::move(filter_builder)),
      should_compare_filter_(should_compare_filter) {}

MockBrowsingDataRemoverDelegate::CallParameters::~CallParameters() {}

bool MockBrowsingDataRemoverDelegate::CallParameters::operator==(
    const CallParameters& other) const {
  const CallParameters& a = *this;
  const CallParameters& b = other;

  if (a.delete_begin_ != b.delete_begin_ || a.delete_end_ != b.delete_end_ ||
      a.remove_mask_ != b.remove_mask_ ||
      a.origin_type_mask_ != b.origin_type_mask_) {
    return false;
  }

  if (!a.should_compare_filter_ || !b.should_compare_filter_)
    return true;
  return *a.filter_builder_ == *b.filter_builder_;
}

std::ostream& operator<<(
    std::ostream& os,
    const MockBrowsingDataRemoverDelegate::CallParameters& p) {
  os << "BrowsingDataFilterBuilder: " << std::endl;
  os << "  delete_begin: " << p.delete_begin_ << std::endl;
  os << "  delete_end: " << p.delete_end_ << std::endl;
  os << "  remove_mask: " << p.remove_mask_ << std::endl;
  os << "  origin_type_mask: " << p.origin_type_mask_ << std::endl;
  if (p.should_compare_filter_) {
    std::string mode_string;
    switch (p.filter_builder_->GetMode()) {
      case BrowsingDataFilterBuilder::Mode::kDelete:
        mode_string = "delete";
        break;
      case BrowsingDataFilterBuilder::Mode::kPreserve:
        mode_string = "preserve";
        break;
    }
    os << "  filter_builder: " << std::endl;
    os << "    mode: " << mode_string << std::endl;

    auto* filter_builder =
        static_cast<BrowsingDataFilterBuilderImpl*>(p.filter_builder_.get());

    std::string origin_mode_string;
    switch (filter_builder->GetOriginModeForTesting()) {
      case BrowsingDataFilterBuilder::OriginMatchingMode::kThirdPartiesIncluded:
        origin_mode_string = "kThirdPartiesIncluded";
        break;
      case BrowsingDataFilterBuilder::OriginMatchingMode::kOriginInAllContexts:
        origin_mode_string = "kOriginInAllContexts";
        break;
      default:
        NOTREACHED();
    }
    os << "    origin mode: " << origin_mode_string << "\n";

    os << "    cookie partition keys: "
       << testing::PrintToString(
              filter_builder->GetCookiePartitionKeyCollectionForTesting())
       << "\n";
    os << "    partitioned cookies only: "
       << (filter_builder->PartitionedCookiesOnly() ? "true\n" : "false\n");

    auto config = p.filter_builder_->GetStoragePartitionConfig();
    os << "    StoragePartition: ";
    if (config.has_value()) {
      os << config.value();
    } else {
      os << "NULL";
    }
    os << std::endl;
  }
  return os;
}

}  // content
