// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_NETWORK_SENDER_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_NETWORK_SENDER_H_

#include "base/callback_forward.h"

namespace content {

class AttributionReport;

struct SendResult;

// This class is responsible for sending conversion reports to their
// configured endpoints over the network.
class AttributionNetworkSender {
 public:
  virtual ~AttributionNetworkSender() = default;

  // Callback used to notify caller that the requested report has been sent.
  using ReportSentCallback =
      base::OnceCallback<void(AttributionReport, SendResult)>;

  // Generates and sends a conversion report matching |report|. This should
  // generate a secure POST request with no-credentials.
  virtual void SendReport(AttributionReport report,
                          ReportSentCallback sent_callback) = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_NETWORK_SENDER_H_
