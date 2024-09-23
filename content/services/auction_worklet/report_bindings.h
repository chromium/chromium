// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SERVICES_AUCTION_WORKLET_REPORT_BINDINGS_H_
#define CONTENT_SERVICES_AUCTION_WORKLET_REPORT_BINDINGS_H_

#include <optional>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "content/services/auction_worklet/auction_v8_helper.h"
#include "content/services/auction_worklet/context_recycler.h"
#include "url/gurl.h"
#include "v8/include/v8-forward.h"

namespace auction_worklet {

class AuctionV8Logger;

// Class to manage bindings for setting a report URL. Expected to be used for a
// context managed by ContextRecycler. Allows only a single call for a report
// URL. On any subsequent calls, clears the report URL and throws an exception.
// Also throws on invalid URLs or non-HTTPS URLs.
class ReportBindings : public Bindings {
 public:
  ReportBindings(AuctionV8Helper* v8_helper, AuctionV8Logger* v8_logger);
  ReportBindings(const ReportBindings&) = delete;
  ReportBindings& operator=(const ReportBindings&) = delete;
  ~ReportBindings() override;

  // Add report method to global context. The ReportBindings must outlive
  // the context.
  void AttachToContext(v8::Local<v8::Context> context) override;
  void Reset() override;

  const std::optional<GURL>& report_url() const { return report_url_; }

 private:
  static void SendReportTo(const v8::FunctionCallbackInfo<v8::Value>& args);

  const raw_ptr<AuctionV8Helper> v8_helper_;
  const raw_ptr<AuctionV8Logger> v8_logger_;

  // This is cleared if an exception is thrown.
  std::optional<GURL> report_url_;

  // sendReportTo() can only be called once.
  bool already_called_ = false;
};

}  // namespace auction_worklet

#endif  // CONTENT_SERVICES_AUCTION_WORKLET_REPORT_BINDINGS_H_
