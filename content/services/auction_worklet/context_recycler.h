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
#include "v8/include/v8-external.h"
#include "v8/include/v8-forward.h"

namespace auction_worklet {

namespace mojom {
class AuctionSharedStorageHost;
}  // namespace mojom

class ForDebuggingOnlyBindings;
class PrivateAggregationBindings;
class SharedStorageBindings;
class RegisterAdBeaconBindings;
class ReportBindings;
class SetBidBindings;
class SetPriorityBindings;
class SetPrioritySignalsOverrideBindings;
class BiddingBrowserSignalsLazyFiller;
class InterestGroupLazyFiller;

// Base class for bindings used with contexts used with ContextRecycler.
// The expected lifecycle is:
// 1) FillInGlobalTemplate()
// 2) Use by script
// 3) Reset()
// 4) Use by script
// 5) Reset()
// etc.
class Bindings {
 public:
  Bindings();
  virtual ~Bindings();
  virtual void FillInGlobalTemplate(
      v8::Local<v8::ObjectTemplate> global_template) = 0;
  virtual void Reset() = 0;
};

// Base class for helper for lazily filling in objects, with lifetime tied to
// ContextRecycler. The connection to ContextRecycler is needed in case the
// parent object is (inappropriately) saved between reuses, to avoid dangling
// points to input data (in that case we return overly fresh data, but that's
// safe, and it doesn't seem worth the effort to aid in misuse).
//
// API for implementers is as follows:
//
// 1) In FillInObject, call DefineLazyAttribute for all relevant attributes.
// 2) In the static helpers registered with DefineLazyAttribute
//    (which take (v8::Local<v8::Name> name,
//                 const v8::PropertyCallbackInfo<v8::Value>& info)
//    Use GetSelf and SetResult() to provide value.
//    The implementation must be careful of `this` being in recycled state,
//    and also re-check that the field itself is still there (the check in
//    FillInObject may have been from pre-recycling).
//
//    If you use the JSON parser, make sure to eat exceptions with v8::TryCatch.
//
// 3) In Reset(), adjust state for recycling.
//
// Users should get one from ContextRecycler and call FillInObject on it, after
// ContextRecyclerScope is active.
class LazyFiller {
 public:
  virtual ~LazyFiller();
  // Return success/failure.
  virtual bool FillInObject(v8::Local<v8::Object> object) = 0;
  virtual void Reset() = 0;

 protected:
  explicit LazyFiller(AuctionV8Helper* v8_helper);
  AuctionV8Helper* v8_helper() { return v8_helper_.get(); }

  template <typename T>
  static T* GetSelf(const v8::PropertyCallbackInfo<v8::Value>& info) {
    return static_cast<T*>(v8::External::Cast(*info.Data())->Value());
  }

  static void SetResult(const v8::PropertyCallbackInfo<v8::Value>& info,
                        v8::Local<v8::Value> result);

  bool DefineLazyAttribute(v8::Local<v8::Object> object,
                           base::StringPiece name,
                           v8::AccessorNameGetterCallback getter);

 private:
  const raw_ptr<AuctionV8Helper> v8_helper_;
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

  // Should be called before GetContext(); assumes `bindings` is already owned
  // by one of the fields.
  void AddBindings(Bindings* bindings);

  // Creates if necessary. Must be in full isolate scope to use this.
  v8::Local<v8::Context> GetContext();

  // Called by ContextRecyclerScope.
  void ResetForReuse();

  const raw_ptr<AuctionV8Helper> v8_helper_;
  v8::Global<v8::Context> context_;

  std::unique_ptr<ForDebuggingOnlyBindings> for_debugging_only_bindings_;
  std::unique_ptr<PrivateAggregationBindings> private_aggregation_bindings_;
  std::unique_ptr<SharedStorageBindings> shared_storage_bindings_;
  std::unique_ptr<RegisterAdBeaconBindings> register_ad_beacon_bindings_;
  std::unique_ptr<ReportBindings> report_bindings_;
  std::unique_ptr<SetBidBindings> set_bid_bindings_;
  std::unique_ptr<SetPriorityBindings> set_priority_bindings_;
  std::unique_ptr<SetPrioritySignalsOverrideBindings>
      set_priority_signals_override_bindings_;

  // everything here is owned by one of the unique_ptr's above.
  std::vector<Bindings*> bindings_list_;

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
