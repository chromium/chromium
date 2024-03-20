// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SERVICES_AUCTION_WORKLET_DEPRECATED_URL_LAZY_FILLER_H_
#define CONTENT_SERVICES_AUCTION_WORKLET_DEPRECATED_URL_LAZY_FILLER_H_

#include <string_view>

#include "base/memory/raw_ptr.h"
#include "content/services/auction_worklet/lazy_filler.h"
#include "v8/include/v8-forward.h"

class GURL;

namespace auction_worklet {

class AuctionV8Helper;
class AuctionV8Logger;

// Helper class to display a warning when a deprecated field of an object
// containing a URL is accessed.
class DeprecatedUrlLazyFiller : public LazyFiller {
 public:
  // Creates an object that can set a field on passed in objects to a string
  // containing `url`, and display `warning` on first access. If `url` is
  // nullptr, does nothing. Name of the field is specified when calling
  // SetLazyUrl().
  //
  // All passed in pointers must outlive the created
  // DeprecatedUrlLazyFiller. Additionally, `url` and `warning` must not be
  // modified until the DeprecatedUrlLazyFiller is destroyed.
  DeprecatedUrlLazyFiller(AuctionV8Helper* v8_helper,
                          AuctionV8Logger* v8_logger,
                          const GURL* url,
                          const char* warning);

  ~DeprecatedUrlLazyFiller() override;

  // Adds a getter to `object` that, when the `name` field is first accessed,
  // will display a warning and return the URL `this` was created with, as a
  // string. Subsequent accesses will not display a warning. Does nothing if
  // this was constructed with nullptr for a URL.
  //
  // Returns success/failure.
  bool AddDeprecatedUrlGetter(v8::Local<v8::Object> object,
                              std::string_view name);

 private:
  static void HandleDeprecatedUrl(
      v8::Local<v8::Name> name,
      const v8::PropertyCallbackInfo<v8::Value>& info);

  const raw_ptr<AuctionV8Logger> v8_logger_;
  const raw_ptr<const GURL> url_;
  const raw_ptr<const char> warning_;
};

}  // namespace auction_worklet

#endif  // CONTENT_SERVICES_AUCTION_WORKLET_DEPRECATED_URL_LAZY_FILLER_H_
