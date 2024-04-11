// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SERVICES_AUCTION_WORKLET_LAZY_FILLER_H_
#define CONTENT_SERVICES_AUCTION_WORKLET_LAZY_FILLER_H_

#include <string_view>

#include "base/memory/raw_ptr.h"
#include "v8/include/v8-external.h"
#include "v8/include/v8-forward.h"
#include "v8/include/v8-function-callback.h"
#include "v8/include/v8-object.h"

namespace auction_worklet {

class AuctionV8Helper;

// Helper class to populate fields of one or more v8::Objects on first access.
// The associated v8::Context must be destroyed immediately after the LazyFiller
// to avoid a UAF. Before would be better, but LazyFiller subclasses often have
// raw pointers to other context-scoped variables (e.g., an AuctionV8Logger)
// which must be destroyed after the LazyFiller.
//
// API for implementers is as follows:
//
// 1) Call DefineLazyAttribute() and/or DefineLazyAttributeWithMetadata()
//     for all relevant attributes. There's no requirement that all calls be
//     on the same object (e.g., can defined lazy attributes on objects
//     contained within the same top-level object).
//
// 2) In the static helpers registered with DefineLazyAttribute
//    (which take (v8::Local<v8::Name> name,
//                 const v8::PropertyCallbackInfo<v8::Value>& info)
//    Use GetSelf() / GetSelfWithMetadata() and SetResult() to provide value.
//
//    If you use the JSON parser, make sure to eat exceptions with v8::TryCatch.
class LazyFiller {
 public:
  virtual ~LazyFiller();

 protected:
  explicit LazyFiller(AuctionV8Helper* v8_helper);
  AuctionV8Helper* v8_helper() { return v8_helper_.get(); }

  // Returns the C++ object DefineLazyAttribute() was invoked on, from the
  // PropertyCallbackInfo passed a lazy callback configured via
  // DefineLazyAttribute().
  //
  // Does not work with attributes set by DefineLazyAttributeWithMetadata().
  template <typename T>
  static T* GetSelf(const v8::PropertyCallbackInfo<v8::Value>& info) {
    return static_cast<T*>(v8::External::Cast(*info.Data())->Value());
  }

  // Like GetSelf(), but for DefineLazyAttributeWithMetadata().
  //
  // Does not work with attributes set by DefineLazyAttribute().
  template <typename T>
  static T* GetSelfWithMetadata(const v8::PropertyCallbackInfo<v8::Value>& info,
                                v8::Local<v8::Value>& metadata) {
    return static_cast<T*>(GetSelfWithMetadataInternal(info, metadata));
  }

  static void SetResult(const v8::PropertyCallbackInfo<v8::Value>& info,
                        v8::Local<v8::Value> result);

  bool DefineLazyAttribute(v8::Local<v8::Object> object,
                           std::string_view name,
                           v8::AccessorNameGetterCallback getter);

  // `lazy_filler_template` is used to construct an internal v8 object with a
  // pointer to `this` and `metadata` stored internally. It should be empty on
  // the first call, and reused across calls, to avoid having to repeatedly
  // create a new template.
  bool DefineLazyAttributeWithMetadata(
      v8::Local<v8::Object> object,
      v8::Local<v8::Value> metadata,
      std::string_view name,
      v8::AccessorNameGetterCallback getter,
      v8::Local<v8::ObjectTemplate>& lazy_filler_template);

 private:
  static void* GetSelfWithMetadataInternal(
      const v8::PropertyCallbackInfo<v8::Value>& info,
      v8::Local<v8::Value>& metadata);

  const raw_ptr<AuctionV8Helper> v8_helper_;
};

}  // namespace auction_worklet

#endif  // CONTENT_SERVICES_AUCTION_WORKLET_LAZY_FILLER_H_
