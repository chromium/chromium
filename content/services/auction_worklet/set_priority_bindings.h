// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SERVICES_AUCTION_WORKLET_SET_PRIORITY_BINDINGS_H_
#define CONTENT_SERVICES_AUCTION_WORKLET_SET_PRIORITY_BINDINGS_H_

#include "base/callback.h"
#include "base/memory/raw_ptr.h"
#include "content/services/auction_worklet/auction_v8_helper.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"
#include "v8/include/v8-forward.h"

namespace auction_worklet {

// Class to manage bindings for setting interest group priority. Expected to be
// used for a short-lived v8::Context. Allows only a single call for to set a
// priority. On any subequent calls, clears the set priority and throws an
// exception.
class SetPriorityBindings {
 public:
  // Add report method to `global_template`. The ReportBindings must outlive
  // the template.
  SetPriorityBindings(AuctionV8Helper* v8_helper,
                      v8::Local<v8::ObjectTemplate> global_template);
  SetPriorityBindings(const SetPriorityBindings&) = delete;
  SetPriorityBindings& operator=(const SetPriorityBindings&) = delete;
  ~SetPriorityBindings();

  const absl::optional<double>& set_priority() const { return set_priority_; }

 private:
  static void SetPriority(const v8::FunctionCallbackInfo<v8::Value>& args);

  const raw_ptr<AuctionV8Helper> v8_helper_;

  // This cleared if an exception is thrown.
  absl::optional<double> set_priority_;

  // Once an exception has been thrown, `set_priority_` will be permanently
  // cleared.
  bool exception_thrown_ = false;
};

}  // namespace auction_worklet

#endif  // CONTENT_SERVICES_AUCTION_WORKLET_SET_PRIORITY_BINDINGS_H_