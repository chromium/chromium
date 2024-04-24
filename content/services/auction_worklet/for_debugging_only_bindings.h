// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SERVICES_AUCTION_WORKLET_FOR_DEBUGGING_ONLY_BINDINGS_H_
#define CONTENT_SERVICES_AUCTION_WORKLET_FOR_DEBUGGING_ONLY_BINDINGS_H_

#include <optional>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "content/common/content_export.h"
#include "content/services/auction_worklet/auction_v8_helper.h"
#include "content/services/auction_worklet/context_recycler.h"
#include "url/gurl.h"
#include "v8/include/v8-forward.h"

namespace auction_worklet {

class AuctionV8Logger;

// Class to manage bindings for setting a debugging report URL. Expected to be
// used for a context managed by ContextRecycler. The URL passed to the last
// successful call will be used as the reporting URL. Throws on invalid URLs or
// non-HTTPS URLs.
class CONTENT_EXPORT ForDebuggingOnlyBindings : public Bindings {
 public:
  ForDebuggingOnlyBindings(AuctionV8Helper* v8_helper,
                           AuctionV8Logger* v8_logger);
  ForDebuggingOnlyBindings(const ForDebuggingOnlyBindings&) = delete;
  ForDebuggingOnlyBindings& operator=(const ForDebuggingOnlyBindings&) = delete;
  ~ForDebuggingOnlyBindings() override;

  // Add forDebuggingOnly object to the global context. The
  // ForDebuggingOnlyBindings must outlive the context.
  void AttachToContext(v8::Local<v8::Context> context) override;
  void Reset() override;

  std::optional<GURL> TakeLossReportUrl();
  std::optional<GURL> TakeWinReportUrl();

 private:
  static void ReportAdAuctionLoss(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  static void ReportAdAuctionWin(
      const v8::FunctionCallbackInfo<v8::Value>& args);

  const raw_ptr<AuctionV8Helper> v8_helper_;
  const raw_ptr<AuctionV8Logger> v8_logger_;

  std::optional<GURL> loss_report_url_;
  std::optional<GURL> win_report_url_;
};

}  // namespace auction_worklet

#endif  // CONTENT_SERVICES_AUCTION_WORKLET_FOR_DEBUGGING_ONLY_BINDINGS_H_
