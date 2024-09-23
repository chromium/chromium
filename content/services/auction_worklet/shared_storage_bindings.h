// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SERVICES_AUCTION_WORKLET_SHARED_STORAGE_BINDINGS_H_
#define CONTENT_SERVICES_AUCTION_WORKLET_SHARED_STORAGE_BINDINGS_H_

#include "base/memory/raw_ptr.h"
#include "content/common/content_export.h"
#include "content/services/auction_worklet/context_recycler.h"
#include "content/services/auction_worklet/public/mojom/auction_shared_storage_host.mojom-forward.h"
#include "v8/include/v8-forward.h"

namespace auction_worklet {

class AuctionV8Helper;

// Class to manage bindings for the Shared Storage API. Expected to be used
// for a context managed by `ContextRecycler`. Throws exceptions when invalid
// arguments are detected.
class CONTENT_EXPORT SharedStorageBindings : public Bindings {
 public:
  explicit SharedStorageBindings(
      AuctionV8Helper* v8_helper,
      mojom::AuctionSharedStorageHost* shared_storage_host,
      mojom::AuctionWorkletFunction source_auction_worklet_function,
      bool shared_storage_permissions_policy_allowed);
  SharedStorageBindings(const SharedStorageBindings&) = delete;
  SharedStorageBindings& operator=(const SharedStorageBindings&) = delete;
  ~SharedStorageBindings() override;

  // Add privateAggregation object to global context. `this` must outlive the
  // context.
  void AttachToContext(v8::Local<v8::Context> context) override;
  void Reset() override;

 private:
  static void Set(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Append(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Delete(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Clear(const v8::FunctionCallbackInfo<v8::Value>& args);

  const raw_ptr<AuctionV8Helper> v8_helper_;

  const raw_ptr<mojom::AuctionSharedStorageHost> shared_storage_host_;

  mojom::AuctionWorkletFunction source_auction_worklet_function_;

  bool shared_storage_permissions_policy_allowed_;
};

}  // namespace auction_worklet

#endif  // CONTENT_SERVICES_AUCTION_WORKLET_SHARED_STORAGE_BINDINGS_H_
