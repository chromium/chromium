// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/sdp_message.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace remoting::protocol {

// Verify that SDP is normalized by removing empty lines and normalizing
// line-endings to \r\n.
TEST(SdpMessages, Normalize) {
  SdpMessage sdp_message("a=foo\n\r\nb=bar");
  EXPECT_EQ("a=foo\r\nb=bar\r\n", sdp_message.ToString());
}

// Verify that the normalized form of SDP for signature calculation has
// line-endings of \n, for compatibility with existing clients.
TEST(SdpMessages, NormalizedForSignature) {
  SdpMessage sdp_message("a=foo\nb=bar\r\n");
  EXPECT_EQ("a=foo\nb=bar\n", sdp_message.NormalizedForSignature());
}

TEST(SdpMessages, AddCodecParameter) {
  std::string kSourceSdp =
      "a=group:BUNDLE video\n"
      "m=video 9 UDP/TLS/RTP/SAVPF 96\n"
      "a=rtpmap:96 VP8/90000\n"
      "a=rtcp-fb:96 transport-cc\n";
  SdpMessage sdp_message(kSourceSdp);
  EXPECT_TRUE(sdp_message.AddCodecParameter("VP8", "test_param=1"));
  EXPECT_EQ(
      "a=group:BUNDLE video\r\n"
      "m=video 9 UDP/TLS/RTP/SAVPF 96\r\n"
      "a=rtpmap:96 VP8/90000\r\n"
      "a=fmtp:96 test_param=1\r\n"
      "a=rtcp-fb:96 transport-cc\r\n",
      sdp_message.ToString());
}

TEST(SdpMessages, AddCodecParameterMissingCodec) {
  std::string kSourceSdp =
      "a=group:BUNDLE audio video\r\n"
      "m=audio 9 UDP/TLS/RTP/SAVPF 111\r\n"
      "a=extmap:1 urn:ietf:params:rtp-hdrext:ssrc-audio-level\r\n"
      "a=rtpmap:111 opus/48000/2\r\n"
      "a=rtcp-fb:111 transport-cc\r\n"
      "m=video 9 UDP/TLS/RTP/SAVPF 96\r\n"
      "a=rtpmap:96 VP9/90000\r\n"
      "a=rtcp-fb:96 transport-cc\r\n"
      "m=application 9 DTLS/SCTP 5000\r\n";
  SdpMessage sdp_message(kSourceSdp);
  EXPECT_FALSE(sdp_message.AddCodecParameter("VP8", "test_param=1"));
  EXPECT_EQ(kSourceSdp, sdp_message.ToString());
}

TEST(SdpMessages, AddCodecParameter_MultipleTypes) {
  const std::string kSourceSdp =
      "a=group:BUNDLE video\n"
      "m=video 9 UDP/TLS/RTP/SAVPF 96 97 98 99\n"
      "a=rtpmap:96 VP8/90000\n"
      "a=rtcp-fb:96 transport-cc\n"
      "a=rtpmap:97 VP9/90000\n"
      "a=rtcp-fb:97 transport-cc\n"
      "a=rtpmap:98 VP8/90000\n"
      "a=rtcp-fb:98 transport-cc\n"
      "a=rtpmap:99 VP8/90000\n"
      "a=rtcp-fb:99 transport-cc\n";
  SdpMessage sdp_message(kSourceSdp);
  EXPECT_TRUE(sdp_message.AddCodecParameter("VP8", "test_param=1"));
  EXPECT_EQ(
      "a=group:BUNDLE video\r\n"
      "m=video 9 UDP/TLS/RTP/SAVPF 96 97 98 99\r\n"
      "a=rtpmap:96 VP8/90000\r\n"
      "a=fmtp:96 test_param=1\r\n"
      "a=rtcp-fb:96 transport-cc\r\n"
      "a=rtpmap:97 VP9/90000\r\n"
      "a=rtcp-fb:97 transport-cc\r\n"
      "a=rtpmap:98 VP8/90000\r\n"
      "a=fmtp:98 test_param=1\r\n"
      "a=rtcp-fb:98 transport-cc\r\n"
      "a=rtpmap:99 VP8/90000\r\n"
      "a=fmtp:99 test_param=1\r\n"
      "a=rtcp-fb:99 transport-cc\r\n",
      sdp_message.ToString());
}

}  // namespace remoting::protocol
