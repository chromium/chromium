// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SERVICES_AUCTION_WORKLET_SET_PRIORITY_SIGNALS_OVERRIDE_BINDINGS_H_
#define CONTENT_SERVICES_AUCTION_WORKLET_SET_PRIORITY_SIGNALS_OVERRIDE_BINDINGS_H_

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "content/services/auction_worklet/auction_v8_helper.h"
#include "content/services/auction_worklet/context_recycler.h"
#include "content/services/auction_worklet/public/mojom/bidder_worklet.mojom.h"
#include "url/gurl.h"
#include "v8/include/v8-forward.h"

namespace auction_worklet {

// Class to manage bindings for setting interest group priority signals override
// values. Expected to be used for a context managed by ContextRecycler.
// Allows multiple calls setting the same or different values. The values will
// be combined the interest group's previously set overrides in the browser
// process, as opposed to completely replacing them.
class SetPrioritySignalsOverrideBindings : public Bindings {
 public:
  explicit SetPrioritySignalsOverrideBindings(AuctionV8Helper* v8_helper);
  SetPrioritySignalsOverrideBindings(const SetPriorityBindings&) = delete;
  SetPrioritySignalsOverrideBindings& operator=(const SetPriorityBindings&) =
      delete;
  ~SetPrioritySignalsOverrideBindings() override;

  // Add report method to global context. `this` must outlive the context.
  void AttachToContext(v8::Local<v8::Context> context) override;
  void Reset() override;

  base::flat_map<std::string, mojom::PrioritySignalsDoublePtr>
  TakeSetPrioritySignalsOverrides();

 private:
  static void SetPrioritySignalsOverride(
      const v8::FunctionCallbackInfo<v8::Value>& args);

  const raw_ptr<AuctionV8Helper> v8_helper_;

  base::flat_map<std::string, mojom::PrioritySignalsDoublePtr>
      update_priority_signals_overrides_;
};

}  // namespace auction_worklet

#endif  // CONTENT_SERVICES_AUCTION_WORKLET_SET_PRIORITY_SIGNALS_OVERRIDE_BINDINGS_H_
