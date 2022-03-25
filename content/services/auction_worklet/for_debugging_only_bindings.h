// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SERVICES_AUCTION_WORKLET_FOR_DEBUGGING_ONLY_BINDINGS_H_
#define CONTENT_SERVICES_AUCTION_WORKLET_FOR_DEBUGGING_ONLY_BINDINGS_H_

#include "base/callback.h"
#include "base/memory/raw_ptr.h"
#include "content/services/auction_worklet/auction_v8_helper.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"
#include "v8/include/v8-forward.h"

namespace auction_worklet {

// Class to manage bindings for setting a debugging report URL. Expected to be
// used for a short-lived v8::Context. The URL passed to the last successful
// call will be used as the reporting URL. Throws on invalid URLs or non-HTTPS
// URLs.
class ForDebuggingOnlyBindings {
 public:
  // Add forDebuggingOnly object to `global_template`. The
  // ForDebuggingOnlyBindings must outlive the template.
  ForDebuggingOnlyBindings(AuctionV8Helper* v8_helper,
                           v8::Local<v8::ObjectTemplate> global_template);
  ForDebuggingOnlyBindings(const ForDebuggingOnlyBindings&) = delete;
  ForDebuggingOnlyBindings& operator=(const ForDebuggingOnlyBindings&) = delete;
  ~ForDebuggingOnlyBindings();

  absl::optional<GURL> TakeLossReportUrl() {
    return std::move(loss_report_url_);
  }
  absl::optional<GURL> TakeWinReportUrl() { return std::move(win_report_url_); }

 private:
  static void ReportAdAuctionLoss(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  static void ReportAdAuctionWin(
      const v8::FunctionCallbackInfo<v8::Value>& args);

  const raw_ptr<AuctionV8Helper> v8_helper_;

  absl::optional<GURL> loss_report_url_;
  absl::optional<GURL> win_report_url_;
};

}  // namespace auction_worklet

#endif  // CONTENT_SERVICES_AUCTION_WORKLET_FOR_DEBUGGING_ONLY_BINDINGS_H_
