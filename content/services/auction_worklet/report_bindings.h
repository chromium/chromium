// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SERVICES_AUCTION_WORKLET_REPORT_BINDINGS_H_
#define CONTENT_SERVICES_AUCTION_WORKLET_REPORT_BINDINGS_H_

#include "base/callback.h"
#include "base/memory/raw_ptr.h"
#include "content/services/auction_worklet/auction_v8_helper.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"
#include "v8/include/v8-forward.h"

namespace auction_worklet {

// Class to manage bindings for setting a report URL. Expected to be used for a
// short-lived v8::Context. Allows only a single call for a report URL. On any
// subequent calls, clears the report URL and throws an exception. Also throws
// on invalid URLs or non-HTTPS URLs.
class ReportBindings {
 public:
  // Add report method to `global_template`. The ReportBindings must outlive
  // the template.
  ReportBindings(AuctionV8Helper* v8_helper,
                 v8::Local<v8::ObjectTemplate> global_template);
  ReportBindings(const ReportBindings&) = delete;
  ReportBindings& operator=(const ReportBindings&) = delete;
  ~ReportBindings();

  const absl::optional<GURL>& report_url() const { return report_url_; }

 private:
  static void SendReportTo(const v8::FunctionCallbackInfo<v8::Value>& args);

  const raw_ptr<AuctionV8Helper> v8_helper_;

  // This cleared if an exception is thrown.
  absl::optional<GURL> report_url_;

  // Once an exception has been thrown, `report_url_` will be permanently
  // cleared.
  bool exception_thrown_ = false;
};

}  // namespace auction_worklet

#endif  // CONTENT_SERVICES_AUCTION_WORKLET_REPORT_BINDINGS_H_
