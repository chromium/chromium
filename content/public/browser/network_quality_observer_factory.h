// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_NETWORK_QUALITY_OBSERVER_FACTORY_H_
#define CONTENT_PUBLIC_BROWSER_NETWORK_QUALITY_OBSERVER_FACTORY_H_

#include "base/macros.h"
#include "content/common/content_export.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "net/nqe/rtt_throughput_estimates_observer.h"

namespace network {
class NetworkQualityTracker;
}

namespace content {

// Creates network quality observer that listens for changes to the network
// quality and manages sending updates to each RenderProcess.
CONTENT_EXPORT std::unique_ptr<
    network::NetworkQualityTracker::RTTAndThroughputEstimatesObserver>
CreateNetworkQualityObserver(
    network::NetworkQualityTracker* network_quality_tracker);

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_NETWORK_QUALITY_OBSERVER_FACTORY_H_
