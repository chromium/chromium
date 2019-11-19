// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/netinfo/testing/internals_net_info.h"

#include "third_party/blink/public/platform/web_connection_type.h"
#include "third_party/blink/public/platform/web_effective_connection_type.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/testing/internals.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/network/network_state_notifier.h"

namespace blink {

void InternalsNetInfo::setNetworkConnectionInfoOverride(
    Internals& internals,
    bool on_line,
    const String& type,
    const String& effective_type,
    uint32_t http_rtt_msec,
    double downlink_max_mbps,
    ExceptionState& exception_state) {
  WebConnectionType webtype;
  if (type == "cellular2g") {
    webtype = kWebConnectionTypeCellular2G;
  } else if (type == "cellular3g") {
    webtype = kWebConnectionTypeCellular3G;
  } else if (type == "cellular4g") {
    webtype = kWebConnectionTypeCellular4G;
  } else if (type == "bluetooth") {
    webtype = kWebConnectionTypeBluetooth;
  } else if (type == "ethernet") {
    webtype = kWebConnectionTypeEthernet;
  } else if (type == "wifi") {
    webtype = kWebConnectionTypeWifi;
  } else if (type == "wimax") {
    webtype = kWebConnectionTypeWimax;
  } else if (type == "other") {
    webtype = kWebConnectionTypeOther;
  } else if (type == "none") {
    webtype = kWebConnectionTypeNone;
  } else if (type == "unknown") {
    webtype = kWebConnectionTypeUnknown;
  } else {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotFoundError,
        ExceptionMessages::FailedToEnumerate("connection type", type));
    return;
  }
  WebEffectiveConnectionType web_effective_type =
      WebEffectiveConnectionType::kTypeUnknown;
  if (effective_type == "offline") {
    web_effective_type = WebEffectiveConnectionType::kTypeOffline;
  } else if (effective_type == "slow-2g") {
    web_effective_type = WebEffectiveConnectionType::kTypeSlow2G;
  } else if (effective_type == "2g") {
    web_effective_type = WebEffectiveConnectionType::kType2G;
  } else if (effective_type == "3g") {
    web_effective_type = WebEffectiveConnectionType::kType3G;
  } else if (effective_type == "4g") {
    web_effective_type = WebEffectiveConnectionType::kType4G;
  } else if (effective_type != "unknown") {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotFoundError,
        ExceptionMessages::FailedToEnumerate("effective connection type",
                                             effective_type));
    return;
  }
  GetNetworkStateNotifier().SetNetworkConnectionInfoOverride(
      on_line, webtype, web_effective_type, http_rtt_msec, downlink_max_mbps);
}

void InternalsNetInfo::setSaveDataEnabled(Internals&, bool enabled) {
  GetNetworkStateNotifier().SetSaveDataEnabledOverride(enabled);
}

void InternalsNetInfo::clearNetworkConnectionInfoOverride(Internals&) {
  GetNetworkStateNotifier().ClearOverride();
}

}  // namespace blink
