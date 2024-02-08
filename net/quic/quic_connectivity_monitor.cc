// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_connectivity_monitor.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"

namespace net {

namespace {

bool IsErrorRelatedToConnectivity(int error_code) {
  return (error_code == ERR_ADDRESS_UNREACHABLE ||
          error_code == ERR_ACCESS_DENIED ||
          error_code == ERR_INTERNET_DISCONNECTED);
}

}  // namespace

QuicConnectivityMonitor::QuicConnectivityMonitor(
    handles::NetworkHandle default_network)
    : default_network_(default_network) {}

QuicConnectivityMonitor::~QuicConnectivityMonitor() = default;

void QuicConnectivityMonitor::RecordConnectivityStatsToHistograms(
    const std::string& notification,
    handles::NetworkHandle affected_network) const {
  if (notification == "OnNetworkSoonToDisconnect" ||
      notification == "OnNetworkDisconnected") {
    // If the disconnected network is not the default network, ignore
    // stats collections.
    if (affected_network != default_network_)
      return;
  }

  base::ClampedNumeric<int> num_degrading_sessions = GetNumDegradingSessions();

  if (num_sessions_active_during_current_speculative_connectivity_failure_) {
    UMA_HISTOGRAM_COUNTS_100(
        "Net.QuicConnectivityMonitor.NumSessionsTrackedSinceSpeculativeError",
        num_sessions_active_during_current_speculative_connectivity_failure_
            .value());
  }

  UMA_HISTOGRAM_COUNTS_100(
      "Net.QuicConnectivityMonitor.NumActiveQuicSessionsAtNetworkChange",
      active_sessions_.size());

  int percentage = 0;
  if (num_sessions_active_during_current_speculative_connectivity_failure_ &&
      num_sessions_active_during_current_speculative_connectivity_failure_
              .value() > 0) {
    percentage = base::saturated_cast<int>(
        num_all_degraded_sessions_ * 100.0 /
        num_sessions_active_during_current_speculative_connectivity_failure_
            .value());
  }

  UMA_HISTOGRAM_COUNTS_100(
      "Net.QuicConnectivityMonitor.NumAllSessionsDegradedAtNetworkChange",
      num_all_degraded_sessions_);

  const std::string raw_histogram_name1 =
      "Net.QuicConnectivityMonitor.NumAllDegradedSessions." + notification;
  base::UmaHistogramCustomCounts(raw_histogram_name1,
                                 num_all_degraded_sessions_, 1, 100, 50);

  const std::string percentage_histogram_name1 =
      "Net.QuicConnectivityMonitor.PercentageAllDegradedSessions." +
      notification;

  base::UmaHistogramPercentageObsoleteDoNotUse(percentage_histogram_name1,
                                               percentage);

  // Skip degrading session collection if there are less than two sessions.
  if (active_sessions_.size() < 2u)
    return;

  const std::string raw_histogram_name =
      "Net.QuicConnectivityMonitor.NumActiveDegradingSessions." + notification;

  base::UmaHistogramCustomCounts(raw_histogram_name, num_degrading_sessions, 1,
                                 100, 50);

  percentage = base::saturated_cast<double>(num_degrading_sessions * 100.0 /
                                            active_sessions_.size());

  const std::string percentage_histogram_name =
      "Net.QuicConnectivityMonitor.PercentageActiveDegradingSessions." +
      notification;
  base::UmaHistogramPercentageObsoleteDoNotUse(percentage_histogram_name,
                                               percentage);
}

size_t QuicConnectivityMonitor::GetNumDegradingSessions() const {
  return degrading_sessions_.size();
}

size_t QuicConnectivityMonitor::GetCountForWriteErrorCode(
    int write_error_code) const {
  auto it = write_error_map_.find(write_error_code);
  return it == write_error_map_.end() ? 0u : it->second;
}

void QuicConnectivityMonitor::SetInitialDefaultNetwork(
    handles::NetworkHandle default_network) {
  default_network_ = default_network;
}

void QuicConnectivityMonitor::OnSessionPathDegrading(
    QuicChromiumClientSession* session,
    handles::NetworkHandle network) {
  if (network != default_network_)
    return;

  degrading_sessions_.insert(session);
  num_all_degraded_sessions_++;
  // If the degrading session used to be on the previous default network, it is
  // possible that the session is no longer tracked in |active_sessions_| due
  // to the recent default network change.
  active_sessions_.insert(session);

  if (!num_sessions_active_during_current_speculative_connectivity_failure_) {
    num_sessions_active_during_current_speculative_connectivity_failure_ =
        active_sessions_.size();
  } else {
    // Before seeing session degrading, PACKET_WRITE_ERROR has been observed.
    UMA_HISTOGRAM_COUNTS_100(
        "Net.QuicConnectivityMonitor.NumWriteErrorsSeenBeforeDegradation",
        quic_error_map_[quic::QUIC_PACKET_WRITE_ERROR]);
  }
}

void QuicConnectivityMonitor::OnSessionResumedPostPathDegrading(
    QuicChromiumClientSession* session,
    handles::NetworkHandle network) {
  if (network != default_network_)
    return;

  degrading_sessions_.erase(session);

  // If the resumed session used to be on the previous default network, it is
  // possible that the session is no longer tracked in |active_sessions_| due
  // to the recent default network change.
  active_sessions_.insert(session);

  num_all_degraded_sessions_ = 0u;
  num_sessions_active_during_current_speculative_connectivity_failure_ =
      std::nullopt;
}

void QuicConnectivityMonitor::OnSessionEncounteringWriteError(
    QuicChromiumClientSession* session,
    handles::NetworkHandle network,
    int error_code) {
  if (network != default_network_)
    return;

  // If the session used to be on the previous default network, it is
  // possible that the session is no longer tracked in |active_sessions_| due
  // to the recent default network change.
  active_sessions_.insert(session);

  ++write_error_map_[error_code];

  bool is_session_degraded =
      degrading_sessions_.find(session) != degrading_sessions_.end();

  UMA_HISTOGRAM_BOOLEAN(
      "Net.QuicConnectivityMonitor.SessionDegradedBeforeWriteError",
      is_session_degraded);

  if (!num_sessions_active_during_current_speculative_connectivity_failure_ &&
      IsErrorRelatedToConnectivity(error_code)) {
    num_sessions_active_during_current_speculative_connectivity_failure_ =
        active_sessions_.size();
  }
}

void QuicConnectivityMonitor::OnSessionClosedAfterHandshake(
    QuicChromiumClientSession* session,
    handles::NetworkHandle network,
    quic::ConnectionCloseSource source,
    quic::QuicErrorCode error_code) {
  if (network != default_network_)
    return;

  if (source == quic::ConnectionCloseSource::FROM_PEER) {
    // Connection closed by the peer post handshake with PUBLIC RESET
    // is most likely a NAT rebinding issue.
    if (error_code == quic::QUIC_PUBLIC_RESET)
      quic_error_map_[error_code]++;
    return;
  }

  if (error_code == quic::QUIC_PACKET_WRITE_ERROR ||
      error_code == quic::QUIC_TOO_MANY_RTOS) {
    // Connection close by self with PACKET_WRITE_ERROR or TOO_MANY_RTOS
    // is likely a connectivity issue.
    quic_error_map_[error_code]++;
  }
}

void QuicConnectivityMonitor::OnSessionRegistered(
    QuicChromiumClientSession* session,
    handles::NetworkHandle network) {
  if (network != default_network_)
    return;

  active_sessions_.insert(session);
  if (num_sessions_active_during_current_speculative_connectivity_failure_) {
    num_sessions_active_during_current_speculative_connectivity_failure_
        .value()++;
  }
}

void QuicConnectivityMonitor::OnSessionRemoved(
    QuicChromiumClientSession* session) {
  degrading_sessions_.erase(session);
  active_sessions_.erase(session);
}

void QuicConnectivityMonitor::OnDefaultNetworkUpdated(
    handles::NetworkHandle default_network) {
  default_network_ = default_network;
  active_sessions_.clear();
  degrading_sessions_.clear();
  num_sessions_active_during_current_speculative_connectivity_failure_ =
      std::nullopt;
  write_error_map_.clear();
  quic_error_map_.clear();
}

void QuicConnectivityMonitor::OnIPAddressChanged() {
  // If handles::NetworkHandle is supported, connectivity monitor will receive
  // notifications via OnDefaultNetworkUpdated.
  if (NetworkChangeNotifier::AreNetworkHandlesSupported())
    return;

  DCHECK_EQ(default_network_, handles::kInvalidNetworkHandle);
  degrading_sessions_.clear();
  write_error_map_.clear();
}

void QuicConnectivityMonitor::OnSessionGoingAwayOnIPAddressChange(
    QuicChromiumClientSession* session) {
  // This should only be called after ConnectivityMonitor gets notified via
  // OnIPAddressChanged().
  DCHECK(degrading_sessions_.empty());
  // |session| that encounters IP address change will lose track which network
  // it is bound to. Future connectivity monitoring may be misleading.
  session->RemoveConnectivityObserver(this);
}

}  // namespace net
