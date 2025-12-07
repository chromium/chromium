// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_ALTERNATE_PROTOCOL_USAGE_H_
#define NET_HTTP_ALTERNATE_PROTOCOL_USAGE_H_

namespace net {

// The reason why Chrome uses a specific transport protocol for HTTP semantics.
enum AlternateProtocolUsage {
  // Alternate Protocol was used without racing a normal connection.
  ALTERNATE_PROTOCOL_USAGE_NO_RACE = 0,
  // Alternate Protocol was used by winning a race with a normal connection.
  ALTERNATE_PROTOCOL_USAGE_WON_RACE = 1,
  // Alternate Protocol was not used by losing a race with a normal connection.
  ALTERNATE_PROTOCOL_USAGE_MAIN_JOB_WON_RACE = 2,
  // Alternate Protocol was not used because no Alternate-Protocol information
  // was available when the request was issued, but an Alternate-Protocol header
  // was present in the response.
  ALTERNATE_PROTOCOL_USAGE_MAPPING_MISSING = 3,
  // Alternate Protocol was not used because it was marked broken.
  ALTERNATE_PROTOCOL_USAGE_BROKEN = 4,
  // HTTPS DNS protocol upgrade job was used without racing with a normal
  // connection and an Alternate Protocol job.
  ALTERNATE_PROTOCOL_USAGE_DNS_ALPN_H3_JOB_WON_WITHOUT_RACE = 5,
  // HTTPS DNS protocol upgrade job won a race with a normal connection and
  // an Alternate Protocol job.
  ALTERNATE_PROTOCOL_USAGE_DNS_ALPN_H3_JOB_WON_RACE = 6,
  // This value is used when the reason is unknown and also used as the default
  // value.
  ALTERNATE_PROTOCOL_USAGE_UNSPECIFIED_REASON = 7,
  // Maximum value for the enum.
  ALTERNATE_PROTOCOL_USAGE_MAX,
};

// The state of the advertised alternative service.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(AdvertisedAltSvcState)
enum class AdvertisedAltSvcState {
  // The state is unknown.
  kUnknown = 0,
  // QUIC is advertised and it's not broken.
  kQuicNotBroken = 1,
  // QUIC is advertised but it's broken.
  kQuicBroken = 2,
  kMaxValue = kQuicBroken,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/page/enums.xml:AdvertisedAltSvcState)

}  // namespace net

#endif  // NET_HTTP_ALTERNATE_PROTOCOL_USAGE_H_
