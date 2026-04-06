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
};

}  // namespace net

#endif  // NET_BASE_LOAD_TIMING_INTERNAL_INFO_H_
