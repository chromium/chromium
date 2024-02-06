// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SERVICES_AUCTION_WORKLET_CONTEXT_RECYCLER_H_
#define CONTENT_SERVICES_AUCTION_WORKLET_CONTEXT_RECYCLER_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "content/common/content_export.h"
#include "content/services/auction_worklet/auction_v8_helper.h"
#include "content/services/auction_worklet/lazy_filler.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-forward.h"

namespace auction_worklet {

namespace mojom {
class AuctionSharedStorageHost;
}  // namespace mojom

class AuctionV8Logger;
class ForDebuggingOnlyBindings;
class PrivateAggregationBindings;
class SharedStorageBindings;
class RegisterAdBeaconBindings;
class RegisterAdMacroBindings;
class ReportBindings;
class SetBidBindings;
class SetPriorityBindings;
class SetPrioritySignalsOverrideBindings;
class BiddingBrowserSignalsLazyFiller;
class InterestGroupLazyFiller;

// Base class for bindings used with contexts used with ContextRecycler.
// The expected lifecycle is:
// 1) AttachToContext()
// 2) Use by script
// 3) Reset()
// 4) Use by script
// 5) Reset()
// etc.
class Bindings {
 public:
  Bindings();
  virtual ~Bindings();
  virtual void AttachToContext(v8::Local<v8::Context> context) = 0;
  virtual void Reset() = 0;
};

// Base class for helper for lazily filling in objects, with lifetime tied to
// ContextRecycler. The connection to ContextRecycler is needed in case the
// parent object is (inappropriately) saved between reuses, to avoid dangling
// points to input data (in that case we return overly fresh data, but that's
// safe, and it doesn't seem worth the effort to aid in misuse).
//
// In addition to the basic LazyFillter pattern, implementors must also
// implement Reset(), which adjusts state for recycling.
//
// Implementations must be careful of `this` being in a recycled state, and
// re-check that the fields themselves are still present on access (values may
// have changed between defining an attribute and the invocation of the
// attribute's callback).
//
// Users should get PersistedLazyFiller from ContextRecycler and use it to
// populate objects only after ContextRecyclerScope is active.
class PersistedLazyFiller : public LazyFiller {
 public:
  ~PersistedLazyFiller() override;
  virtual void Reset() = 0;

 protected:
  PersistedLazyFiller(AuctionV8Helper* v8_helper);
};

// This helps manage the state of bindings on a context should we chose to
// recycle it, by calling Reset() after the current usage is done, to prepare
// for the next. Context is accessed via ContextRecyclerScope.
class CONTENT_EXPORT ContextRecycler {
 public:
  explicit ContextRecycler(AuctionV8Helper* v8_helper);
  ~ContextRecycler();

  void AddForDebuggingOnlyBindings();
  ForDebuggingOnlyBindings* for_debugging_only_bindings() {
    return for_debugging_only_bindings_.get();
  }

  void AddPrivateAggregationBindings(
      bool private_aggregation_permissions_policy_allowed);
  PrivateAggregationBindings* private_aggregation_bindings() {
    return private_aggregation_bindings_.get();
  }

  void AddSharedStorageBindings(
      mojom::AuctionSharedStorageHost* shared_storage_host,
      bool shared_storage_permissions_policy_allowed);
  SharedStorageBindings* shared_storage_bindings() {
    return shared_storage_bindings_.get();
  }

  void AddRegisterAdBeaconBindings();
  RegisterAdBeaconBindings* register_ad_beacon_bindings() {
    return register_ad_beacon_bindings_.get();
  }

  void AddRegisterAdMacroBindings();
  RegisterAdMacroBindings* register_ad_macro_bindings() {
    return register_ad_macro_bindings_.get();
  }

  void AddReportBindings();
  ReportBindings* report_bindings() { return report_bindings_.get(); }

  void AddSetBidBindings();
  SetBidBindings* set_bid_bindings() { return set_bid_bindings_.get(); }

  void AddSetPriorityBindings();
  SetPriorityBindings* set_priority_bindings() {
    return set_priority_bindings_.get();
  }

  void AddSetPrioritySignalsOverrideBindings();
  SetPrioritySignalsOverrideBindings* set_priority_signals_override_bindings() {
    return set_priority_signals_override_bindings_.get();
  }

  void AddInterestGroupLazyFiller();
  InterestGroupLazyFiller* interest_group_lazy_filler() {
    return interest_group_lazy_filler_.get();
  }

  void AddBiddingBrowserSignalsLazyFiller();
  BiddingBrowserSignalsLazyFiller* bidding_browser_signals_lazy_filler() {
    return bidding_browser_signals_lazy_filler_.get();
  }

 private:
  friend class ContextRecyclerScope;

  // Should be called after GetContext(); assumes `bindings` is already owned
  // by one of the fields.
  void AddBindings(Bindings* bindings);

  // Creates if necessary. Must be in full isolate scope to use this.
  v8::Local<v8::Context> GetContext();

  // Called by ContextRecyclerScope.
  void ResetForReuse();

  const raw_ptr<AuctionV8Helper> v8_helper_;
  v8::Global<v8::Context> context_;

  // Must be after `v8_helper` and `context_`, but before lazy bindings, which
  // may use it.
  std::unique_ptr<AuctionV8Logger> v8_logger_;

  std::unique_ptr<ForDebuggingOnlyBindings> for_debugging_only_bindings_;
  std::unique_ptr<PrivateAggregationBindings> private_aggregation_bindings_;
  std::unique_ptr<SharedStorageBindings> shared_storage_bindings_;
  std::unique_ptr<RegisterAdBeaconBindings> register_ad_beacon_bindings_;
  std::unique_ptr<RegisterAdMacroBindings> register_ad_macro_bindings_;
  std::unique_ptr<ReportBindings> report_bindings_;
  std::unique_ptr<SetBidBindings> set_bid_bindings_;
  std::unique_ptr<SetPriorityBindings> set_priority_bindings_;
  std::unique_ptr<SetPrioritySignalsOverrideBindings>
      set_priority_signals_override_bindings_;

  // everything here is owned by one of the unique_ptr's above.
  std::vector<raw_ptr<Bindings, VectorExperimental>> bindings_list_;

  std::unique_ptr<InterestGroupLazyFiller> interest_group_lazy_filler_;
  std::unique_ptr<BiddingBrowserSignalsLazyFiller>
      bidding_browser_signals_lazy_filler_;
};

// Helper to enter a context scope on creation and reset all bindings
// on destruction.
class CONTENT_EXPORT ContextRecyclerScope {
 public:
  // `context_recycler` must outlast `this`.
  explicit ContextRecyclerScope(ContextRecycler& context_recycler);
  ~ContextRecyclerScope();

  v8::Local<v8::Context> GetContext();

 private:
  const raw_ref<ContextRecycler> context_recycler_;
  v8::Local<v8::Context> context_;
  v8::Context::Scope context_scope_;
};

}  // namespace auction_worklet

#endif  // CONTENT_SERVICES_AUCTION_WORKLET_CONTEXT_RECYCLER_H_
