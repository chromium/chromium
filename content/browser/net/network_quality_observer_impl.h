// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_NET_NETWORK_QUALITY_OBSERVER_IMPL_H_
#define CONTENT_BROWSER_NET_NETWORK_QUALITY_OBSERVER_IMPL_H_

#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "content/common/content_export.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "net/nqe/effective_connection_type.h"
#include "net/nqe/network_quality.h"
#include "services/network/public/cpp/network_quality_tracker.h"

namespace content {

// Listens for changes to the network quality and manages sending updates to
// each RenderProcess via mojo.
class CONTENT_EXPORT NetworkQualityObserverImpl
    : public network::NetworkQualityTracker::EffectiveConnectionTypeObserver,
      public network::NetworkQualityTracker::RTTAndThroughputEstimatesObserver,
      public content::NotificationObserver {
 public:
  explicit NetworkQualityObserverImpl(
      network::NetworkQualityTracker* network_quality_tracker);

  ~NetworkQualityObserverImpl() override;

 private:
  // content::NotificationObserver:
  void Observe(int type,
               const NotificationSource& source,
               const NotificationDetails& details) override;

  // net::EffectiveConnectionTypeObserver implementation:
  void OnEffectiveConnectionTypeChanged(
      net::EffectiveConnectionType type) override;

  // net::RTTAndThroughputEstimatesObserver implementation:
  void OnRTTOrThroughputEstimatesComputed(
      base::TimeDelta http_rtt,
      base::TimeDelta transport_rtt,
      int32_t downstream_throughput_kbps) override;

  // |network_quality_tracker_| is guaranteed to be non-null during the
  // lifetime of |this|.
  network::NetworkQualityTracker* network_quality_tracker_;

  //  The network quality when the |ui_thread_observer_| was last notified.
  net::EffectiveConnectionType last_notified_type_;
  net::nqe::internal::NetworkQuality last_notified_network_quality_;

  content::NotificationRegistrar registrar_;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(NetworkQualityObserverImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_NET_NETWORK_QUALITY_OBSERVER_IMPL_H_
