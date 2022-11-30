// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_REPORTING_REPORTING_NETWORK_CHANGE_OBSERVER_H_
#define NET_REPORTING_REPORTING_NETWORK_CHANGE_OBSERVER_H_

#include <memory>

#include "net/base/net_export.h"

namespace net {

class ReportingContext;

// Clears queued reports and/or configured endpoints on network changes if
// enabled in ReportingPolicy.
class NET_EXPORT ReportingNetworkChangeObserver {
 public:
  // Creates a ReportingNetworkChangeObserver. |context| must outlive it.
  static std::unique_ptr<ReportingNetworkChangeObserver> Create(
      ReportingContext* context);

  virtual ~ReportingNetworkChangeObserver();
};

}  // namespace net

#endif  // NET_REPORTING_REPORTING_NETWORK_CHANGE_OBSERVER_H_
