// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/net/network_quality_observer_impl.h"

#include "base/metrics/histogram_macros.h"
#include "content/common/renderer.mojom.h"
#include "content/common/view_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/network_quality_observer_factory.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"

namespace {

// Returns true if the |current_value| is meaningfully different from the
// |past_value|. May be called with either RTT or throughput values to compare.
bool MetricChangedMeaningfully(int32_t past_value, int32_t current_value) {
  if ((past_value == net::nqe::internal::INVALID_RTT_THROUGHPUT) !=
      (current_value == net::nqe::internal::INVALID_RTT_THROUGHPUT)) {
    return true;
  }

  if (past_value == net::nqe::internal::INVALID_RTT_THROUGHPUT &&
      current_value == net::nqe::internal::INVALID_RTT_THROUGHPUT) {
    return false;
  }

  DCHECK_LE(0, past_value);
  DCHECK_LE(0, current_value);

  // Metric has changed meaningfully only if (i) the difference between the two
  // values exceed the threshold; and, (ii) the ratio of the values also exceeds
  // the threshold.
  static const int kMinDifferenceInMetrics = 100;
  static const float kMinRatio = 1.2f;

  if (std::abs(past_value - current_value) < kMinDifferenceInMetrics) {
    // The absolute change in the value is not sufficient enough.
    return false;
  }

  if (past_value < (kMinRatio * current_value) &&
      current_value < (kMinRatio * past_value)) {
    // The relative change in the value is not sufficient enough.
    return false;
  }

  return true;
}

}  // namespace

namespace content {

NetworkQualityObserverImpl::NetworkQualityObserverImpl(
    network::NetworkQualityTracker* network_quality_tracker)
    : network_quality_tracker_(network_quality_tracker),
      last_notified_type_(net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  registrar_.Add(this, NOTIFICATION_RENDERER_PROCESS_CREATED,
                 NotificationService::AllSources());
  network_quality_tracker_->AddRTTAndThroughputEstimatesObserver(this);
  network_quality_tracker_->AddEffectiveConnectionTypeObserver(this);
}

NetworkQualityObserverImpl::~NetworkQualityObserverImpl() {
  DCHECK(thread_checker_.CalledOnValidThread());
  network_quality_tracker_->RemoveRTTAndThroughputEstimatesObserver(this);
  network_quality_tracker_->RemoveEffectiveConnectionTypeObserver(this);
}

void NetworkQualityObserverImpl::OnEffectiveConnectionTypeChanged(
    net::EffectiveConnectionType type) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (last_notified_type_ == type)
    return;

  last_notified_type_ = type;

  // Notify all the existing renderers of the change in the network quality.
  for (RenderProcessHost::iterator it(RenderProcessHost::AllHostsIterator());
       !it.IsAtEnd(); it.Advance()) {
    if (it.GetCurrentValue()->IsInitializedAndNotDead()) {
      it.GetCurrentValue()->GetRendererInterface()->OnNetworkQualityChanged(
          last_notified_type_, last_notified_network_quality_.http_rtt(),
          last_notified_network_quality_.transport_rtt(),
          last_notified_network_quality_.downstream_throughput_kbps());
    }
  }
}

void NetworkQualityObserverImpl::Observe(int type,
                                         const NotificationSource& source,
                                         const NotificationDetails& details) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(NOTIFICATION_RENDERER_PROCESS_CREATED, type);

  RenderProcessHost* rph = Source<RenderProcessHost>(source).ptr();

  // Notify the newly created renderer of the current network quality.
  rph->GetRendererInterface()->OnNetworkQualityChanged(
      last_notified_type_, last_notified_network_quality_.http_rtt(),
      last_notified_network_quality_.transport_rtt(),
      last_notified_network_quality_.downstream_throughput_kbps());
}

void NetworkQualityObserverImpl::OnRTTOrThroughputEstimatesComputed(
    base::TimeDelta http_rtt,
    base::TimeDelta transport_rtt,
    int32_t downstream_throughput_kbps) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Check if any of the network quality metrics changed meaningfully.
  bool http_rtt_changed = MetricChangedMeaningfully(
      last_notified_network_quality_.http_rtt().InMilliseconds(),
      http_rtt.InMilliseconds());

  bool transport_rtt_changed = MetricChangedMeaningfully(
      last_notified_network_quality_.transport_rtt().InMilliseconds(),
      transport_rtt.InMilliseconds());
  bool kbps_changed = MetricChangedMeaningfully(
      last_notified_network_quality_.downstream_throughput_kbps(),
      downstream_throughput_kbps);

  bool network_quality_meaningfully_changed =
      http_rtt_changed || transport_rtt_changed || kbps_changed;
  UMA_HISTOGRAM_BOOLEAN("NQE.ContentObserver.NetworkQualityMeaningfullyChanged",
                        network_quality_meaningfully_changed);

  if (!network_quality_meaningfully_changed) {
    // Return since none of the metrics changed meaningfully. This reduces
    // the number of notifications to the different renderers every time
    // the network quality is recomputed.
    return;
  }

  last_notified_network_quality_ = net::nqe::internal::NetworkQuality(
      http_rtt, transport_rtt, downstream_throughput_kbps);

  // Notify all the existing renderers of the change in the network quality.
  for (RenderProcessHost::iterator it(RenderProcessHost::AllHostsIterator());
       !it.IsAtEnd(); it.Advance()) {
    if (it.GetCurrentValue()->IsInitializedAndNotDead()) {
      it.GetCurrentValue()->GetRendererInterface()->OnNetworkQualityChanged(
          last_notified_type_, last_notified_network_quality_.http_rtt(),
          last_notified_network_quality_.transport_rtt(),
          last_notified_network_quality_.downstream_throughput_kbps());
    }
  }
}

std::unique_ptr<
    network::NetworkQualityTracker::RTTAndThroughputEstimatesObserver>
CreateNetworkQualityObserver(
    network::NetworkQualityTracker* network_quality_tracker) {
  return std::make_unique<NetworkQualityObserverImpl>(network_quality_tracker);
}

}  // namespace content
