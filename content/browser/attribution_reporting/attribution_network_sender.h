// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_NETWORK_SENDER_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_NETWORK_SENDER_H_

#include "base/callback_forward.h"

class GURL;

namespace base {
class Value;
}  // namespace base

namespace content {

struct SendResult;

// This class is responsible for sending conversion reports to their
// configured endpoints over the network.
class AttributionNetworkSender {
 public:
  virtual ~AttributionNetworkSender() = default;

  // Callback used to notify caller that the requested report has been sent.
  using ReportSentCallback = base::OnceCallback<void(SendResult)>;

  // Generates and sends a conversion report matching |report|. This should
  // generate a secure POST request with no-credentials.
  virtual void SendReport(GURL report_url,
                          base::Value report_body,
                          ReportSentCallback sent_callback) = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_NETWORK_SENDER_H_
