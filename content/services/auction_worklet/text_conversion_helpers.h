// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SERVICES_AUCTION_WORKLET_TEXT_CONVERSION_HELPERS_H_
#define CONTENT_SERVICES_AUCTION_WORKLET_TEXT_CONVERSION_HELPERS_H_

#include "content/common/content_export.h"
#include "content/services/auction_worklet/context_recycler.h"
#include "v8/include/v8-forward.h"

namespace auction_worklet {

class AuctionV8Helper;

// Utilities to add JS functions to help convert between JS Strings and utf-8
// arrays.
class CONTENT_EXPORT TextConversionHelpers : public Bindings {
 public:
  explicit TextConversionHelpers(AuctionV8Helper* v8_helper);

  void AttachToContext(v8::Local<v8::Context> context) override;
  void Reset() override;

 private:
  static void EncodeUtf8(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void DecodeUtf8(const v8::FunctionCallbackInfo<v8::Value>& args);

  const raw_ptr<AuctionV8Helper> v8_helper_;
};

}  // namespace auction_worklet

#endif  // CONTENT_SERVICES_AUCTION_WORKLET_TEXT_CONVERSION_HELPERS_H_
