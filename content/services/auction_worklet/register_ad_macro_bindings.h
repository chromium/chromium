// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SERVICES_AUCTION_WORKLET_REGISTER_AD_MACRO_BINDINGS_H_
#define CONTENT_SERVICES_AUCTION_WORKLET_REGISTER_AD_MACRO_BINDINGS_H_

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "content/services/auction_worklet/auction_v8_helper.h"
#include "content/services/auction_worklet/context_recycler.h"
#include "content/services/auction_worklet/public/mojom/bidder_worklet.mojom.h"
#include "v8/include/v8-forward.h"

namespace auction_worklet {

// Class to manage bindings for setting ad macros. Expected to be used for a
// context managed by ContextRecycler. Allowed to be called multiple times. Use
// the last valid call's macro value as the macro name's value if called
// multiple times for the same macro name.
class RegisterAdMacroBindings : public Bindings {
 public:
  explicit RegisterAdMacroBindings(AuctionV8Helper* v8_helper);
  RegisterAdMacroBindings(const RegisterAdMacroBindings&) = delete;
  RegisterAdMacroBindings& operator=(const RegisterAdMacroBindings&) = delete;
  ~RegisterAdMacroBindings() override;

  // Add registerAdMacroBindings object to the global context. The
  // RegisterAdMacroBindings must outlive the context.
  void AttachToContext(v8::Local<v8::Context> context) override;
  void Reset() override;

  base::flat_map<std::string, std::string> TakeAdMacroMap() {
    return std::move(ad_macro_map_);
  }

 private:
  static void RegisterAdMacro(const v8::FunctionCallbackInfo<v8::Value>& args);

  const raw_ptr<AuctionV8Helper> v8_helper_;

  // This is a map from the ad macro name to the macro's value.
  base::flat_map<std::string, std::string> ad_macro_map_;
};

}  // namespace auction_worklet

#endif  // CONTENT_SERVICES_AUCTION_WORKLET_REGISTER_AD_MACRO_BINDINGS_H_
