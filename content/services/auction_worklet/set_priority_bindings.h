// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SERVICES_AUCTION_WORKLET_SET_PRIORITY_BINDINGS_H_
#define CONTENT_SERVICES_AUCTION_WORKLET_SET_PRIORITY_BINDINGS_H_

#include <optional>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "content/services/auction_worklet/auction_v8_helper.h"
#include "content/services/auction_worklet/context_recycler.h"
#include "url/gurl.h"
#include "v8/include/v8-forward.h"

namespace auction_worklet {

// Class to manage bindings for setting interest group priority. Expected to be
// used for a for a context managed by ContextRecycler. Allows only a single
// call to set a priority. On any subsequent calls, clears the set priority and
// throws an exception.
class SetPriorityBindings : public Bindings {
 public:
  explicit SetPriorityBindings(AuctionV8Helper* v8_helper);
  SetPriorityBindings(const SetPriorityBindings&) = delete;
  SetPriorityBindings& operator=(const SetPriorityBindings&) = delete;
  ~SetPriorityBindings() override;

  // Add report method to the global context. The ReportBindings must outlive
  // the context.
  void AttachToContext(v8::Local<v8::Context> context) override;
  void Reset() override;

  const std::optional<double>& set_priority() const { return set_priority_; }

 private:
  static void SetPriority(const v8::FunctionCallbackInfo<v8::Value>& args);

  const raw_ptr<AuctionV8Helper> v8_helper_;

  // This is cleared if an exception is thrown.
  std::optional<double> set_priority_;

  // setPriority() can only be called once.
  bool already_called_ = false;
};

}  // namespace auction_worklet

#endif  // CONTENT_SERVICES_AUCTION_WORKLET_SET_PRIORITY_BINDINGS_H_
