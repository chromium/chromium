// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_QUIC_CONNECTIVITY_MONITOR_H_
#define NET_QUIC_QUIC_CONNECTIVITY_MONITOR_H_

#include <set>

#include "base/memory/raw_ptr.h"
#include "base/numerics/clamped_math.h"
#include "net/base/network_handle.h"
#include "net/quic/quic_chromium_client_session.h"

namespace net {

// Responsible for monitoring path degrading detection/recovery events on the
// default network interface.
// Reset all raw observations (reported by sessions) when the default network
// is changed, which happens either:
// - via OnDefaultNetworkUpdated if handles::NetworkHandle is supported on the
// platform;
// - via OnIPAddressChanged otherwise.
class NET_EXPORT_PRIVATE QuicConnectivityMonitor
    : public QuicChromiumClientSession::ConnectivityObserver {
 public:
  explicit QuicConnectivityMonitor(handles::NetworkHandle default_network);

  QuicConnectivityMonitor(const QuicConnectivityMonitor&) = delete;
  QuicConnectivityMonitor& operator=(const QuicConnectivityMonitor&) = delete;

  ~QuicConnectivityMonitor() override;

  // Records connectivity related stats to histograms.
  void RecordConnectivityStatsToHistograms(
      const std::string& platform_notification,
      handles::NetworkHandle affected_network) const;

  // Returns the number of sessions that are currently degrading on the default
  // network interface.
  size_t GetNumDegradingSessions() const;

  // Returns the number of reports received for |write_error_code| on
  // |default_network|.
  size_t GetCountForWriteErrorCode(int write_error_code) const;

  // Called to set up the initial default network, which happens when the
  // default network tracking is lost upon |this| creation.
  void SetInitialDefaultNetwork(handles::NetworkHandle default_network);

  // Called when handles::NetworkHandle is supported and the default network
  // interface used by the platform is updated.
  void OnDefaultNetworkUpdated(handles::NetworkHandle default_network);

  // Called when handles::NetworkHandle is NOT supported and the IP address of
  // the primary interface changes. This includes when the primary interface
  // itself changes.
  void OnIPAddressChanged();

  // Called when |session| is marked as going away due to IP address change.
  void OnSessionGoingAwayOnIPAddressChange(QuicChromiumClientSession* session);

  // QuicChromiumClientSession::ConnectivityObserver implementation.
  void OnSessionPathDegrading(QuicChromiumClientSession* session,
                              handles::NetworkHandle network) override;

  void OnSessionResumedPostPathDegrading(
      QuicChromiumClientSession* session,
      handles::NetworkHandle network) override;

  void OnSessionEncounteringWriteError(QuicChromiumClientSession* session,
                                       handles::NetworkHandle network,
                                       int error_code) override;

  void OnSessionClosedAfterHandshake(QuicChromiumClientSession* session,
                                     handles::NetworkHandle network,
                                     quic::ConnectionCloseSource source,
                                     quic::QuicErrorCode error_code) override;

  void OnSessionRegistered(QuicChromiumClientSession* session,
                           handles::NetworkHandle network) override;

  void OnSessionRemoved(QuicChromiumClientSession* session) override;

 private:
  // Size chosen per net.QuicSession.WriteError histogram.
  using WriteErrorMap = base::flat_map<int, size_t>;
  // The most common QuicErrorCode cared by this monitor is:
  // QUIC_PUBLIC_RESET by the peer, or
  // QUIC_PACKET_WRITE_ERROR/QUIC_TOO_MANY_RTOS by self.
  using QuicErrorCodeMap = base::flat_map<quic::QuicErrorCode, size_t>;

  // If handles::NetworkHandle is not supported, always set to
  // handles::kInvalidNetworkHandle.
  handles::NetworkHandle default_network_;
  // Sessions that are currently degrading on the |default_network_|.
  std::set<raw_ptr<QuicChromiumClientSession, SetExperimental>>
      degrading_sessions_;
  // Sessions that are currently active on the |default_network_|.
  std::set<raw_ptr<QuicChromiumClientSession, SetExperimental>>
      active_sessions_;

  // Number of sessions that have been active or created during the period of
  // a speculative connectivity failure.
  // The period of a speculative connectivity failure
  // - starts by the earliest detection of path degradation or a connectivity
  //   related packet write error,
  // - ends immediately by the detection of path recovery or a network change.
  // Use clamped math to cap number of sessions at INT_MAX.
  std::optional<base::ClampedNumeric<int>>
      num_sessions_active_during_current_speculative_connectivity_failure_;
  // Total number of sessions that has been degraded before any recovery,
  // including no longer active sessions.
  // Use clamped math to cap number of sessions at INT_MAX.
  base::ClampedNumeric<int> num_all_degraded_sessions_{0};

  // Map from the write error code to the corresponding number of reports.
  WriteErrorMap write_error_map_;
  QuicErrorCodeMap quic_error_map_;

  base::WeakPtrFactory<QuicConnectivityMonitor> weak_factory_{this};
};

}  // namespace net

#endif  // NET_QUIC_QUIC_CONNECTIVITY_MONITOR_H_
