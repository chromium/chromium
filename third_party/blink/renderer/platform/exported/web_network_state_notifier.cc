/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/public/platform/web_network_state_notifier.h"

#include "third_party/blink/renderer/platform/network/network_state_notifier.h"

namespace blink {

void WebNetworkStateNotifier::SetOnLine(bool on_line) {
  GetNetworkStateNotifier().SetOnLine(on_line);
}

void WebNetworkStateNotifier::SetWebConnection(WebConnectionType type,
                                               double max_bandwidth_mbps) {
  GetNetworkStateNotifier().SetWebConnection(type, max_bandwidth_mbps);
}

void WebNetworkStateNotifier::SetNetworkQuality(WebEffectiveConnectionType type,
                                                base::TimeDelta http_rtt,
                                                base::TimeDelta transport_rtt,
                                                int downlink_throughput_kbps) {
  GetNetworkStateNotifier().SetNetworkQuality(type, http_rtt, transport_rtt,
                                              downlink_throughput_kbps);
}

void WebNetworkStateNotifier::SetNetworkQualityWebHoldback(
    WebEffectiveConnectionType type) {
  GetNetworkStateNotifier().SetNetworkQualityWebHoldback(type);
}

void WebNetworkStateNotifier::SetSaveDataEnabled(bool enabled) {
  GetNetworkStateNotifier().SetSaveDataEnabled(enabled);
}

bool WebNetworkStateNotifier::SaveDataEnabled() {
  return GetNetworkStateNotifier().SaveDataEnabled();
}

}  // namespace blink
