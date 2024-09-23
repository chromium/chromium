// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/deprecated_url_lazy_filler.h"

#include <string_view>

#include "base/memory/raw_ptr.h"
#include "content/services/auction_worklet/auction_v8_helper.h"
#include "content/services/auction_worklet/auction_v8_logger.h"
#include "content/services/auction_worklet/lazy_filler.h"
#include "gin/converter.h"
#include "url/gurl.h"
#include "v8/include/v8-object.h"

namespace auction_worklet {

DeprecatedUrlLazyFiller::DeprecatedUrlLazyFiller(AuctionV8Helper* v8_helper,
                                                 AuctionV8Logger* v8_logger,
                                                 const GURL* url,
                                                 const char* warning)
    : LazyFiller(v8_helper),
      v8_logger_(v8_logger),
      url_(url),
      warning_(warning) {}

DeprecatedUrlLazyFiller::~DeprecatedUrlLazyFiller() = default;

bool DeprecatedUrlLazyFiller::AddDeprecatedUrlGetter(
    v8::Local<v8::Object> object,
    std::string_view name) {
  return !url_ ||
         DefineLazyAttribute(object, name,
                             &DeprecatedUrlLazyFiller::HandleDeprecatedUrl);
}

void DeprecatedUrlLazyFiller::HandleDeprecatedUrl(
    v8::Local<v8::Name> name,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  DeprecatedUrlLazyFiller* self = GetSelf<DeprecatedUrlLazyFiller>(info);
  self->v8_logger_->LogConsoleWarning(self->warning_.get());

  AuctionV8Helper* v8_helper = self->v8_helper();
  v8::Isolate* isolate = v8_helper->isolate();
  v8::Local<v8::Value> value;
  if (gin::TryConvertToV8(isolate, self->url_->spec(), &value)) {
    SetResult(info, value);
  } else {
    SetResult(info, v8::Null(isolate));
  }
}

}  // namespace auction_worklet
