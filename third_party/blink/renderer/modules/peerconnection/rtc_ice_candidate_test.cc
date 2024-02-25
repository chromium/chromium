// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_ice_candidate.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_ice_candidate_platform.h"

namespace blink {

constexpr char kUdpRelayCandidateStr[] =
    "candidate:a0+B/3 1 udp 41623807 8.8.8.8 2345 typ relay raddr "
    "192.168.1.5 rport 12345";
constexpr char kUrl[] = "bogusurl";
constexpr char kMid[] = "somemid";
constexpr char kUsernameFragment[] = "u";
constexpr int kSdpMLineIndex = 0;

TEST(RTCIceCandidateTest, Url) {
  RTCIceCandidate* candidate(
      RTCIceCandidate::Create(MakeGarbageCollected<RTCIceCandidatePlatform>(
          kUdpRelayCandidateStr, kMid, kSdpMLineIndex, kUsernameFragment,
          kUrl)));
  EXPECT_EQ(candidate->url(), String(kUrl));
}

TEST(RTCIceCandidateTest, RelayProtocol) {
  RTCIceCandidate* candidate(
      RTCIceCandidate::Create(MakeGarbageCollected<RTCIceCandidatePlatform>(
          kUdpRelayCandidateStr, kMid, kSdpMLineIndex, kUsernameFragment,
          kUrl)));
  EXPECT_EQ(candidate->relayProtocol(), String("udp"));
}

}  // namespace blink
