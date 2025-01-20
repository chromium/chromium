// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SPDY_MULTIPLEXED_SESSION_CREATION_INITIATOR_H_
#define NET_SPDY_MULTIPLEXED_SESSION_CREATION_INITIATOR_H_

namespace net {

// The reason why multiplexed session was created. It is used to distinguish
// between preconnect initiated session and other sessions.
//
// These values are persisted to logs. Entries should not be renumbered
// and numeric values should never be reused.
//
// LINT.IfChange(MultiplexedSessionCreationInitiator)
enum class MultiplexedSessionCreationInitiator {
  kUnknown,
  kPreconnect,
  kMaxValue = kPreconnect
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/net/enums.xml:MultiplexedSessionCreationInitiator)

}  // namespace net

#endif  // NET_SPDY_MULTIPLEXED_SESSION_CREATION_INITIATOR_H_
