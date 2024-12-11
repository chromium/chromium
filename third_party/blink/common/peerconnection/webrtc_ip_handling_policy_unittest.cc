// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/peerconnection/webrtc_ip_handling_policy.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/peerconnection/webrtc_ip_handling_policy.mojom-shared.h"

namespace blink {

TEST(WebRtcIpHandlingPolicyTest, ToWebRTCIPHandlingPolicy) {
  EXPECT_EQ(ToWebRTCIPHandlingPolicy(kWebRTCIPHandlingDefault),
            mojom::WebRtcIpHandlingPolicy::kDefault);
  EXPECT_EQ(ToWebRTCIPHandlingPolicy("random-string"),
            mojom::WebRtcIpHandlingPolicy::kDefault);
  EXPECT_EQ(ToWebRTCIPHandlingPolicy(std::string()),
            mojom::WebRtcIpHandlingPolicy::kDefault);
  EXPECT_EQ(ToWebRTCIPHandlingPolicy(
                kWebRTCIPHandlingDefaultPublicAndPrivateInterfaces),
            mojom::WebRtcIpHandlingPolicy::kDefaultPublicAndPrivateInterfaces);
  EXPECT_EQ(
      ToWebRTCIPHandlingPolicy(kWebRTCIPHandlingDefaultPublicInterfaceOnly),
      mojom::WebRtcIpHandlingPolicy::kDefaultPublicInterfaceOnly);
  EXPECT_EQ(ToWebRTCIPHandlingPolicy(kWebRTCIPHandlingDisableNonProxiedUdp),
            mojom::WebRtcIpHandlingPolicy::kDisableNonProxiedUdp);
}

TEST(WebRtcIpHandlingPolicyTest, ToString) {
  EXPECT_EQ(ToString(mojom::WebRtcIpHandlingPolicy::kDefault),
            kWebRTCIPHandlingDefault);
  EXPECT_EQ(
      ToString(
          mojom::WebRtcIpHandlingPolicy::kDefaultPublicAndPrivateInterfaces),
      kWebRTCIPHandlingDefaultPublicAndPrivateInterfaces);
  EXPECT_EQ(
      ToString(mojom::WebRtcIpHandlingPolicy::kDefaultPublicInterfaceOnly),
      kWebRTCIPHandlingDefaultPublicInterfaceOnly);
  EXPECT_EQ(ToString(mojom::WebRtcIpHandlingPolicy::kDisableNonProxiedUdp),
            kWebRTCIPHandlingDisableNonProxiedUdp);
}

}  // namespace blink
