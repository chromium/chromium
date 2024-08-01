// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/peerconnection/rtc_ice_candidate_platform.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

constexpr char kSrflxCandidateStr[] =
    "candidate:a0+B/3 1 udp 41623807 8.8.8.8 2345 typ srflx raddr "
    "192.168.1.5 rport 12345";
constexpr char kUdpRelayCandidateStr[] =
    "candidate:a0+B/3 1 udp 41623807 8.8.8.8 2345 typ relay raddr "
    "192.168.1.5 rport 12345";
constexpr char kTcpRelayCandidateStr[] =
    "candidate:a0+B/3 1 udp 24846335 8.8.8.8 2345 typ relay raddr "
    "192.168.1.5 rport 12345";
constexpr char kTlsRelayCandidateStr[] =
    "candidate:a0+B/3 1 udp 8069119 8.8.8.8 2345 typ relay raddr "
    "192.168.1.5 rport 12345";
constexpr char kUrl[] = "bogusurl";
constexpr char kMid[] = "somemid";
constexpr char kUsernameFragment[] = "u";
constexpr int kSdpMLineIndex = 0;

TEST(RTCIceCandidatePlatformTest, Url) {
  RTCIceCandidatePlatform* candidate =
      MakeGarbageCollected<RTCIceCandidatePlatform>(
          kSrflxCandidateStr, kMid, kSdpMLineIndex, kUsernameFragment, kUrl);
  EXPECT_EQ(candidate->Url(), String(kUrl));
}

TEST(RTCIceCandidatePlatformTest, LocalSrflxCandidateRelayProtocolUnset) {
  RTCIceCandidatePlatform* candidate =
      MakeGarbageCollected<RTCIceCandidatePlatform>(
          kSrflxCandidateStr, kMid, kSdpMLineIndex, kUsernameFragment, kUrl);
  EXPECT_EQ(candidate->RelayProtocol(), std::nullopt);
}

TEST(RTCIceCandidatePlatformTest, LocalRelayCandidateRelayProtocolSet) {
  RTCIceCandidatePlatform* udp = MakeGarbageCollected<RTCIceCandidatePlatform>(
      kUdpRelayCandidateStr, kMid, kSdpMLineIndex, kUsernameFragment, kUrl);
  EXPECT_EQ(udp->RelayProtocol(), "udp");

  RTCIceCandidatePlatform* tcp = MakeGarbageCollected<RTCIceCandidatePlatform>(
      kTcpRelayCandidateStr, kMid, kSdpMLineIndex, kUsernameFragment, kUrl);
  EXPECT_EQ(tcp->RelayProtocol(), "tcp");

  RTCIceCandidatePlatform* tls = MakeGarbageCollected<RTCIceCandidatePlatform>(
      kTlsRelayCandidateStr, kMid, kSdpMLineIndex, kUsernameFragment, kUrl);
  EXPECT_EQ(tls->RelayProtocol(), "tls");
}

TEST(RTCIceCandidatePlatformTest, RemoteRelayCandidateRelayProtocolUnset) {
  RTCIceCandidatePlatform* candidate =
      MakeGarbageCollected<RTCIceCandidatePlatform>(
          kUdpRelayCandidateStr, kMid, 1, kUsernameFragment, std::nullopt);
  EXPECT_EQ(candidate->RelayProtocol(), std::nullopt);
}

}  // namespace blink
