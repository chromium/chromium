// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SERVICES_AUCTION_WORKLET_CONTEXT_RECYCLER_H_
#define CONTENT_SERVICES_AUCTION_WORKLET_CONTEXT_RECYCLER_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "content/common/content_export.h"
#include "content/services/auction_worklet/auction_v8_helper.h"
#include "v8/include/v8-forward.h"

namespace auction_worklet {

class ForDebuggingOnlyBindings;
class PrivateAggregationBindings;
class RegisterAdBeaconBindings;
class ReportBindings;
class SetBidBindings;
class SetPriorityBindings;
class SetPrioritySignalsOverrideBindings;

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

  void AddPrivateAggregationBindings();
  PrivateAggregationBindings* private_aggregation_bindings() {
    return private_aggregation_bindings_.get();
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
  std::unique_ptr<RegisterAdBeaconBindings> register_ad_beacon_bindings_;
  std::unique_ptr<ReportBindings> report_bindings_;
  std::unique_ptr<SetBidBindings> set_bid_bindings_;
  std::unique_ptr<SetPriorityBindings> set_priority_bindings_;
  std::unique_ptr<SetPrioritySignalsOverrideBindings>
      set_priority_signals_override_bindings_;

  // everything here is owned by one of the unique_ptr's above.
  std::vector<Bindings*> bindings_list_;
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
  ContextRecycler& context_recycler_;
  v8::Local<v8::Context> context_;
  v8::Context::Scope context_scope_;
};

}  // namespace auction_worklet

#endif  // CONTENT_SERVICES_AUCTION_WORKLET_CONTEXT_RECYCLER_H_
