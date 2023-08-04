// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/context_recycler.h"

#include <memory>

#include "base/check.h"
#include "content/services/auction_worklet/auction_v8_helper.h"
#include "content/services/auction_worklet/bidder_lazy_filler.h"
#include "content/services/auction_worklet/for_debugging_only_bindings.h"
#include "content/services/auction_worklet/private_aggregation_bindings.h"
#include "content/services/auction_worklet/register_ad_beacon_bindings.h"
#include "content/services/auction_worklet/register_ad_macro_bindings.h"
#include "content/services/auction_worklet/report_bindings.h"
#include "content/services/auction_worklet/set_bid_bindings.h"
#include "content/services/auction_worklet/set_priority_bindings.h"
#include "content/services/auction_worklet/set_priority_signals_override_bindings.h"
#include "content/services/auction_worklet/shared_storage_bindings.h"
#include "gin/converter.h"
#include "v8/include/v8-external.h"
#include "v8/include/v8-template.h"

namespace auction_worklet {

Bindings::Bindings() = default;
Bindings::~Bindings() = default;

LazyFiller::~LazyFiller() = default;

LazyFiller::LazyFiller(AuctionV8Helper* v8_helper) : v8_helper_(v8_helper) {}

// static
void LazyFiller::SetResult(const v8::PropertyCallbackInfo<v8::Value>& info,
                           v8::Local<v8::Value> result) {
  info.GetReturnValue().Set(result);
}

bool LazyFiller::DefineLazyAttribute(v8::Local<v8::Object> object,
                                     base::StringPiece name,
                                     v8::AccessorNameGetterCallback getter) {
  v8::Isolate* isolate = v8_helper_->isolate();

  v8::Maybe<bool> success = object->SetLazyDataProperty(
      isolate->GetCurrentContext(), gin::StringToSymbol(isolate, name), getter,
      v8::External::New(isolate, this),
      /*attributes=*/v8::None,
      /*getter_side_effect_type=*/v8::SideEffectType::kHasNoSideEffect,
      /*setter_side_effect_type=*/v8::SideEffectType::kHasSideEffect);
  return success.IsJust() && success.FromJust();
}

ContextRecycler::ContextRecycler(AuctionV8Helper* v8_helper)
    : v8_helper_(v8_helper) {}

ContextRecycler::~ContextRecycler() = default;

void ContextRecycler::AddForDebuggingOnlyBindings() {
  DCHECK(!for_debugging_only_bindings_);
  for_debugging_only_bindings_ =
      std::make_unique<ForDebuggingOnlyBindings>(v8_helper_);
  AddBindings(for_debugging_only_bindings_.get());
}

void ContextRecycler::AddPrivateAggregationBindings(
    bool private_aggregation_permissions_policy_allowed) {
  DCHECK(!private_aggregation_bindings_);
  private_aggregation_bindings_ = std::make_unique<PrivateAggregationBindings>(
      v8_helper_, private_aggregation_permissions_policy_allowed);
  AddBindings(private_aggregation_bindings_.get());
}

void ContextRecycler::AddSharedStorageBindings(
    mojom::AuctionSharedStorageHost* shared_storage_host,
    bool shared_storage_permissions_policy_allowed) {
  DCHECK(!shared_storage_bindings_);
  shared_storage_bindings_ = std::make_unique<SharedStorageBindings>(
      v8_helper_, shared_storage_host,
      shared_storage_permissions_policy_allowed);
  AddBindings(shared_storage_bindings_.get());
}

void ContextRecycler::AddRegisterAdBeaconBindings() {
  DCHECK(!register_ad_beacon_bindings_);
  register_ad_beacon_bindings_ =
      std::make_unique<RegisterAdBeaconBindings>(v8_helper_);
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
  report_bindings_ = std::make_unique<ReportBindings>(v8_helper_);
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

void ContextRecycler::AddInterestGroupLazyFiller() {
  DCHECK(!interest_group_lazy_filler_);
  interest_group_lazy_filler_ =
      std::make_unique<InterestGroupLazyFiller>(v8_helper_);
}

void ContextRecycler::AddBiddingBrowserSignalsLazyFiller() {
  DCHECK(!bidding_browser_signals_lazy_filler_);
  bidding_browser_signals_lazy_filler_ =
      std::make_unique<BiddingBrowserSignalsLazyFiller>(v8_helper_);
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
    context_.Reset(isolate, v8_helper_->CreateContext());
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
