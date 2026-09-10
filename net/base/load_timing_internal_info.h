// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_LOAD_TIMING_INTERNAL_INFO_H_
#define NET_BASE_LOAD_TIMING_INTERNAL_INFO_H_

#include <stdint.h>

#include <optional>

#include "base/time/time.h"
#include "net/base/net_export.h"
#include "net/dns/public/resolution_details.h"
#include "net/http/alternate_protocol_usage.h"
#include "net/spdy/multiplexed_session_creation_initiator.h"

namespace net {

// Indicates whether a request used an existing H2/H3 session or not.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(SessionSource)
enum class SessionSource {
  // Used a newly established session.
  kNew = 0,
  // Used an existing session.
  kExisting = 1,
  kMaxValue = kExisting,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/enums.xml:NetworkSessionSource)

// Classifies why a new QUIC session had to be created by checking if an
// established session or an in-flight session attempt already existed.
// Note: When kSessionExisted* is logged, it indicates that an established
// session existed in all_sessions_ but was excluded from active_sessions_ (most
// commonly because it received a GOAWAY frame or is draining during IP address
// migration). When kInflightSession* is logged, it indicates that an in-flight
// session attempt with the same session key was in progress when this session
// was requested. Granular breakdown of why the existing session could not be
// reused is tracked in follow-up metrics.
// LINT.IfChange(QuicSessionEstablishmentReason)
enum class QuicSessionEstablishmentReason {
  kUnknown = 0,
  kNoSessionExisted = 1,
  kSessionExistedButNotPreconnect = 2,
  kSessionExistedAndWasPreconnect = 3,
  kSessionExistedBoth = 4,
  kInflightSessionButNotPreconnect = 5,
  kInflightSessionAndWasPreconnect = 6,
  kMaxValue = kInflightSessionAndWasPreconnect,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/net/enums.xml:QuicSessionEstablishmentReason)

// LINT.IfChange(QuicSessionNonReuseReason)
enum class QuicSessionNonReuseReason {
  kNoSessionExisted_TrueColdStart = 0,
  kNoSessionExisted_KeyMismatch_SocketTag = 1,
  kNoSessionExisted_KeyMismatch_NetworkAnonymizationKey = 2,
  kNoSessionExisted_KeyMismatch_PrivacyMode = 3,
  kNoSessionExisted_KeyMismatch_SecureDnsPolicy = 4,
  kNoSessionExisted_KeyMismatch_Other = 5,
  kSessionExisted_ServerGoaway = 6,
  kSessionExisted_Disconnected = 7,
  kSessionExisted_OtherGoingAway = 8,
  kNoSessionExisted_KeyMismatch_MultipleFields = 9,
  kSessionExisted_MultipleReasons = 10,
  kMaxValue = kSessionExisted_MultipleReasons,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/net/enums.xml:QuicSessionNonReuseReason)

struct NET_EXPORT QuicConnectionReuseDetails {
  QuicConnectionReuseDetails();
  QuicConnectionReuseDetails(const QuicConnectionReuseDetails& other);
  bool operator==(const QuicConnectionReuseDetails& other) const;
  ~QuicConnectionReuseDetails();

  std::optional<QuicSessionEstablishmentReason> establishment_reason;
  std::optional<QuicSessionNonReuseReason> non_reuse_reason;
};

// Structure containing internal load timing information. This is similar to
// LoadTimingInfo, but contains extra information which shouldn't be exposed to
// the web. We use this structure for internal measurements.
struct NET_EXPORT LoadTimingInternalInfo {
  LoadTimingInternalInfo();
  LoadTimingInternalInfo(const LoadTimingInternalInfo& other);
  bool operator==(const LoadTimingInternalInfo& other) const;
  ~LoadTimingInternalInfo();

  // The time taken for a SPDY/QUIC session to create an active stream for this
  // request. Measures pending time due to max stream limits. This is only set
  // when SPDY/QUIC is used.
  std::optional<base::TimeDelta> max_stream_limit_pending_delay;

  // The time taken for HTTP stream creating to finish.
  base::TimeDelta create_stream_delay;

  // The time taken for HTTP transaction connected callback.
  base::TimeDelta connected_callback_delay;

  // WARNING: Unlike other fields in this struct, this one is set in
  // //services/network, which is a kind of layer violation. Intermediate
  // layers could potentially modify this value.
  // Whether the Accept-CH frame was received.
  bool accept_ch_frame_received = false;

  // The time taken for HTTP stream initialization to finish if the
  // initialization was blocked.
  base::TimeDelta initialize_stream_delay;

  // Indicates whether the request used an existing H2/H3 session or not.
  std::optional<SessionSource> session_source;

  // State of the advertised alternative service.
  AdvertisedAltSvcState advertised_alt_svc_state =
      AdvertisedAltSvcState::kUnknown;

  // Whether QUIC is enabled.
  bool http_network_session_quic_enabled = false;

  // The details of the DNS resolution that established the connection used by
  // this request. Can be nullopt when no resolution was performed, or
  // resolution failed.
  std::optional<ResolutionDetails> resolution_details;

  // Details about why a QUIC connection was established or not reused.
  // Populated for responses that used QUIC.
  std::optional<QuicConnectionReuseDetails> quic_connection_reuse_details;
  std::optional<MultiplexedSessionCreationInitiator> session_creation_initiator;
};

}  // namespace net

#endif  // NET_BASE_LOAD_TIMING_INTERNAL_INFO_H_
