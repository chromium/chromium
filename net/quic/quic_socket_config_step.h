// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_QUIC_SOCKET_CONFIG_STEP_H_
#define NET_QUIC_QUIC_SOCKET_CONFIG_STEP_H_

#include <string_view>

#include "net/base/net_export.h"

namespace net {

// Steps in socket creation and configuration.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(QuicSocketConfigStep)
enum class QuicSocketConfigStep {
  kConnect = 0,
  kSetReceiveBufferSize = 1,
  kSetDoNotFragment = 2,
  kSetReceiveEcn = 3,
  kSetSendBufferSize = 4,
  kMaxValue = kSetSendBufferSize,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/net/enums.xml:QuicSocketConfigStep,//tools/metrics/histograms/metadata/net/histograms.xml:QuicSocketConfigStep)

NET_EXPORT_PRIVATE std::string_view QuicSocketConfigStepToString(
    QuicSocketConfigStep step);

}  // namespace net

#endif  // NET_QUIC_QUIC_SOCKET_CONFIG_STEP_H_
