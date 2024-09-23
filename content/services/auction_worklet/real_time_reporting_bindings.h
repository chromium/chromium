// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SERVICES_AUCTION_WORKLET_REAL_TIME_REPORTING_BINDINGS_H_
#define CONTENT_SERVICES_AUCTION_WORKLET_REAL_TIME_REPORTING_BINDINGS_H_

#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "content/common/content_export.h"
#include "content/services/auction_worklet/auction_v8_helper.h"
#include "content/services/auction_worklet/context_recycler.h"
#include "content/services/auction_worklet/public/mojom/real_time_reporting.mojom.h"
#include "v8/include/v8-forward.h"

namespace auction_worklet {

class AuctionV8Helper;

// Class to manage bindings for the Real Time Reporting API. Expected to be used
// for a context managed by `ContextRecycler`. Throws exceptions when invalid
// arguments are detected.
class CONTENT_EXPORT RealTimeReportingBindings : public Bindings {
 public:
  explicit RealTimeReportingBindings(AuctionV8Helper* v8_helper);
  RealTimeReportingBindings(const RealTimeReportingBindings&) = delete;
  RealTimeReportingBindings& operator=(const RealTimeReportingBindings&) =
      delete;
  ~RealTimeReportingBindings() override;

  // Add realTimeReporting object to the global context. The
  // RealTimeReportingBindings must outlive the context.
  void AttachToContext(v8::Local<v8::Context> context) override;
  void Reset() override;

  std::vector<auction_worklet::mojom::RealTimeReportingContributionPtr>
  TakeRealTimeReportingContributions() {
    return std::move(real_time_reporting_contributions_);
  }

 private:
  static void ContributeToHistogram(
      const v8::FunctionCallbackInfo<v8::Value>& args);

  const raw_ptr<AuctionV8Helper> v8_helper_;
  const raw_ptr<AuctionV8Logger> v8_logger_;

  // Contributions from calling Real Time Reporting API.
  std::vector<auction_worklet::mojom::RealTimeReportingContributionPtr>
      real_time_reporting_contributions_;
};

}  // namespace auction_worklet

#endif  // CONTENT_SERVICES_AUCTION_WORKLET_REAL_TIME_REPORTING_BINDINGS_H_
