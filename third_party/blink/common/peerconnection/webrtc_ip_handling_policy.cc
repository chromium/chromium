// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/peerconnection/webrtc_ip_handling_policy.h"

#include <string_view>

#include "third_party/blink/public/mojom/peerconnection/webrtc_ip_handling_policy.mojom.h"

namespace blink {

// The set of strings here need to match what's specified in privacy.json.
const char kWebRTCIPHandlingDefault[] = "default";
const char kWebRTCIPHandlingDefaultPublicAndPrivateInterfaces[] =
    "default_public_and_private_interfaces";
const char kWebRTCIPHandlingDefaultPublicInterfaceOnly[] =
    "default_public_interface_only";
const char kWebRTCIPHandlingDisableNonProxiedUdp[] = "disable_non_proxied_udp";

blink::mojom::WebRtcIpHandlingPolicy ToWebRTCIPHandlingPolicy(
    std::string_view preference) {
  if (preference == kWebRTCIPHandlingDefaultPublicAndPrivateInterfaces) {
    return blink::mojom::WebRtcIpHandlingPolicy::
        kDefaultPublicAndPrivateInterfaces;
  }
  if (preference == kWebRTCIPHandlingDefaultPublicInterfaceOnly) {
    return blink::mojom::WebRtcIpHandlingPolicy::kDefaultPublicInterfaceOnly;
  }
  if (preference == kWebRTCIPHandlingDisableNonProxiedUdp) {
    return blink::mojom::WebRtcIpHandlingPolicy::kDisableNonProxiedUdp;
  }
  return blink::mojom::WebRtcIpHandlingPolicy::kDefault;
}

const char* ToString(blink::mojom::WebRtcIpHandlingPolicy policy) {
  switch (policy) {
    case blink::mojom::WebRtcIpHandlingPolicy::kDefault:
      return kWebRTCIPHandlingDefault;
    case blink::mojom::WebRtcIpHandlingPolicy::
        kDefaultPublicAndPrivateInterfaces:
      return kWebRTCIPHandlingDefaultPublicAndPrivateInterfaces;
    case blink::mojom::WebRtcIpHandlingPolicy::kDefaultPublicInterfaceOnly:
      return kWebRTCIPHandlingDefaultPublicInterfaceOnly;
    case blink::mojom::WebRtcIpHandlingPolicy::kDisableNonProxiedUdp:
      return kWebRTCIPHandlingDisableNonProxiedUdp;
  }
}

}  // namespace blink
