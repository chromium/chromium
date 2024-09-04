// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/context_recycler.h"

#include <memory>

#include "base/check.h"
#include "content/services/auction_worklet/auction_v8_helper.h"
#include "content/services/auction_worklet/auction_v8_logger.h"
#include "content/services/auction_worklet/bidder_lazy_filler.h"
#include "content/services/auction_worklet/for_debugging_only_bindings.h"
#include "content/services/auction_worklet/private_aggregation_bindings.h"
#include "content/services/auction_worklet/real_time_reporting_bindings.h"
#include "content/services/auction_worklet/register_ad_beacon_bindings.h"
#include "content/services/auction_worklet/register_ad_macro_bindings.h"
#include "content/services/auction_worklet/report_bindings.h"
#include "content/services/auction_worklet/seller_lazy_filler.h"
#include "content/services/auction_worklet/set_bid_bindings.h"
#include "content/services/auction_worklet/set_priority_bindings.h"
#include "content/services/auction_worklet/set_priority_signals_override_bindings.h"
#include "content/services/auction_worklet/shared_storage_bindings.h"
#include "v8/include/v8-context.h"

namespace auction_worklet {

Bindings::Bindings() = default;
Bindings::~Bindings() = default;

PersistedLazyFiller::~PersistedLazyFiller() = default;

PersistedLazyFiller::PersistedLazyFiller(AuctionV8Helper* v8_helper)
    : LazyFiller(v8_helper) {}

ContextRecycler::ContextRecycler(AuctionV8Helper* v8_helper)
    : v8_helper_(v8_helper) {}

ContextRecycler::~ContextRecycler() = default;

void ContextRecycler::AddForDebuggingOnlyBindings() {
  DCHECK(!for_debugging_only_bindings_);
  for_debugging_only_bindings_ =
      std::make_unique<ForDebuggingOnlyBindings>(v8_helper_, v8_logger_.get());
  AddBindings(for_debugging_only_bindings_.get());
}

void ContextRecycler::AddPrivateAggregationBindings(
    bool private_aggregation_permissions_policy_allowed,
    bool reserved_once_allowed) {
  DCHECK(!private_aggregation_bindings_);
  private_aggregation_bindings_ = std::make_unique<PrivateAggregationBindings>(
      v8_helper_, v8_logger_.get(),
      private_aggregation_permissions_policy_allowed, reserved_once_allowed);
  AddBindings(private_aggregation_bindings_.get());
}

void ContextRecycler::AddRealTimeReportingBindings() {
  DCHECK(!real_time_reporting_bindings_);
  real_time_reporting_bindings_ =
      std::make_unique<RealTimeReportingBindings>(v8_helper_);
  AddBindings(real_time_reporting_bindings_.get());
}

void ContextRecycler::AddRegisterAdBeaconBindings() {
  DCHECK(!register_ad_beacon_bindings_);
  register_ad_beacon_bindings_ =
      std::make_unique<RegisterAdBeaconBindings>(v8_helper_, v8_logger_.get());
  AddBindings(register_ad_beacon_bindings_.get());
}

void ContextRecycler::AddRegisterAdMacroBindings() {
  DCHECK(!register_ad_macro_bindings_);
  register_ad_macro_bindings_ =
      std::make_unique<RegisterAdMacroBindings>(v8_helper_);
  AddBindings(register_ad_macro_bindings_.get());
}

void ContextRecycler::AddReportBindings() {
  DCHECK(!report_bindings_);
  report_bindings_ =
      std::make_unique<ReportBindings>(v8_helper_, v8_logger_.get());
  AddBindings(report_bindings_.get());
}

void ContextRecycler::AddSetBidBindings() {
  DCHECK(!set_bid_bindings_);
  set_bid_bindings_ = std::make_unique<SetBidBindings>(v8_helper_);
  AddBindings(set_bid_bindings_.get());
}

void ContextRecycler::AddSetPriorityBindings() {
  DCHECK(!set_priority_bindings_);
  set_priority_bindings_ = std::make_unique<SetPriorityBindings>(v8_helper_);
  AddBindings(set_priority_bindings_.get());
}

void ContextRecycler::AddSharedStorageBindings(
    mojom::AuctionSharedStorageHost* shared_storage_host,
    mojom::AuctionWorkletFunction source_auction_worklet_function,
    bool shared_storage_permissions_policy_allowed) {
  DCHECK(!shared_storage_bindings_);
  shared_storage_bindings_ = std::make_unique<SharedStorageBindings>(
      v8_helper_, shared_storage_host, source_auction_worklet_function,
      shared_storage_permissions_policy_allowed);
  AddBindings(shared_storage_bindings_.get());
}

void ContextRecycler::AddInterestGroupLazyFiller() {
  DCHECK(!interest_group_lazy_filler_);
  interest_group_lazy_filler_ =
      std::make_unique<InterestGroupLazyFiller>(v8_helper_, v8_logger_.get());
}

void ContextRecycler::AddBiddingBrowserSignalsLazyFiller() {
  DCHECK(!bidding_browser_signals_lazy_filler_);
  bidding_browser_signals_lazy_filler_ =
      std::make_unique<BiddingBrowserSignalsLazyFiller>(v8_helper_);
}

void ContextRecycler::AddSellerBrowserSignalsLazyFiller() {
  DCHECK(!seller_browser_signals_lazy_filler_);
  seller_browser_signals_lazy_filler_ =
      std::make_unique<SellerBrowserSignalsLazyFiller>(v8_helper_,
                                                       v8_logger_.get());
}

void ContextRecycler::EnsureAuctionConfigLazyFillers(size_t required) {
  // We may see different limits in the same worklet if it's used for multiple
  // auctions.  In that case, we have to be sure to never shrink the vector
  // since there may be pointers to entries beyond the current need floating
  // around.
  size_t cur_size = auction_config_lazy_fillers_.size();
  if (cur_size >= required) {
    return;
  }
  auction_config_lazy_fillers_.resize(required);
  for (size_t pos = cur_size; pos < required; ++pos) {
    auction_config_lazy_fillers_[pos] =
        std::make_unique<AuctionConfigLazyFiller>(v8_helper_, v8_logger_.get());
  }
}

void ContextRecycler::AddSetPrioritySignalsOverrideBindings() {
  DCHECK(!set_priority_signals_override_bindings_);
  set_priority_signals_override_bindings_ =
      std::make_unique<SetPrioritySignalsOverrideBindings>(v8_helper_);
  AddBindings(set_priority_signals_override_bindings_.get());
}

void ContextRecycler::AddBindings(Bindings* bindings) {
  DCHECK(!context_.IsEmpty());  // should be called after GetContext()
  bindings->AttachToContext(context_.Get(v8_helper_->isolate()));
  bindings_list_.push_back(bindings);
}

v8::Local<v8::Context> ContextRecycler::GetContext() {
  v8::Isolate* isolate = v8_helper_->isolate();
  if (context_.IsEmpty()) {
    v8::Local<v8::Context> context = v8_helper_->CreateContext();
    context_.Reset(isolate, context);
    v8_logger_ = std::make_unique<AuctionV8Logger>(v8_helper_, context);
  }

  return context_.Get(isolate);
}

void ContextRecycler::ResetForReuse() {
  for (Bindings* bindings : bindings_list_)
    bindings->Reset();
  if (bidding_browser_signals_lazy_filler_)
    bidding_browser_signals_lazy_filler_->Reset();
  if (interest_group_lazy_filler_)
    interest_group_lazy_filler_->Reset();
  if (seller_browser_signals_lazy_filler_) {
    seller_browser_signals_lazy_filler_->Reset();
  }
  for (const auto& auction_config_lazy_filler : auction_config_lazy_fillers_) {
    auction_config_lazy_filler->Reset();
  }
}

ContextRecyclerScope::ContextRecyclerScope(ContextRecycler& context_recycler)
    : context_recycler_(context_recycler),
      context_(context_recycler_->GetContext()),
      context_scope_(context_) {}

ContextRecyclerScope::~ContextRecyclerScope() {
  context_recycler_->ResetForReuse();
}

v8::Local<v8::Context> ContextRecyclerScope::GetContext() {
  return context_;
}

}  // namespace auction_worklet
