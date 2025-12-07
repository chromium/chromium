// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/sdp_message.h"

#include "base/strings/stringprintf.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/api/video_codecs/sdp_video_format.h"

namespace remoting::protocol {

namespace {
constexpr std::string_view kOriginalVideoLine =
    "m=video 9 UDP/TLS/RTP/SAVPF 96 97 98 99 35 36 45 46 47 48 119 120";
constexpr std::string_view kPreferredCodecFormatString =
    "a=group:BUNDLE video\r\n"
    "%s\r\n"  // Line ending is not set so tests can decide what to use.
    "a=rtpmap:96 VP8/90000\r\n"
    "a=rtcp-fb:96 goog-remb\r\n"
    "a=rtpmap:97 rtx/90000\r\n"
    "a=fmtp:97 apt=96\r\n"
    "a=rtpmap:98 VP9/90000\r\n"
    "a=rtcp-fb:98 goog-remb\r\n"
    "a=fmtp:98 profile-id=0\r\n"
    "a=rtpmap:99 rtx/90000\r\n"
    "a=fmtp:99 apt=98\r\n"
    "a=rtpmap:35 VP9/90000\r\n"
    "a=rtcp-fb:35 transport-cc\r\n"
    "a=fmtp:35 profile-id=1\r\n"
    "a=rtpmap:36 rtx/90000\r\n"
    "a=fmtp:36 apt=35\r\n"
    "a=rtpmap:45 AV1/90000\r\n"
    "a=rtcp-fb:45 transport-cc\r\n"
    "a=fmtp:45 level-idx=5;profile=0;tier=0\r\n"
    "a=rtpmap:46 rtx/90000\r\n"
    "a=fmtp:46 apt=45\r\n"
    "a=rtpmap:47 AV1/90000\r\n"
    "a=rtcp-fb:47 transport-cc\r\n"
    "a=fmtp:47 level-idx=5;profile=1;tier=0\r\n"
    "a=rtpmap:48 rtx/90000\r\n"
    "a=fmtp:48 apt=47\r\n"
    "a=rtpmap:119 red/90000\r\n"
    "a=rtpmap:120 rtx/90000\r\n"
    "a=fmtp:120 apt=119\r\n";
}  // namespace

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

TEST(SdpMessages, SetPreferredVideoFormat_PayloadExists_VP8) {
  const std::string original_sdp_contents =
      base::StringPrintf(kPreferredCodecFormatString, kOriginalVideoLine);
  SdpMessage sdp_message(original_sdp_contents);
  sdp_message.SetPreferredVideoFormat(webrtc::SdpVideoFormat::VP8());
  EXPECT_EQ(sdp_message.ToString(), original_sdp_contents);
}

TEST(SdpMessages, SetPreferredVideoFormat_PayloadExists_VP9_Profile0) {
  SdpMessage sdp_message(
      base::StringPrintf(kPreferredCodecFormatString, kOriginalVideoLine));
  sdp_message.SetPreferredVideoFormat(webrtc::SdpVideoFormat::VP9Profile0());
  EXPECT_EQ(
      sdp_message.ToString(),
      base::StringPrintf(
          kPreferredCodecFormatString,
          "m=video 9 UDP/TLS/RTP/SAVPF 98 96 97 99 35 36 45 46 47 48 119 120"));
}

TEST(SdpMessages, SetPreferredVideoFormat_PayloadExists_VP9_Profile1) {
  SdpMessage sdp_message(
      base::StringPrintf(kPreferredCodecFormatString, kOriginalVideoLine));
  sdp_message.SetPreferredVideoFormat(webrtc::SdpVideoFormat::VP9Profile1());
  EXPECT_EQ(
      sdp_message.ToString(),
      base::StringPrintf(
          kPreferredCodecFormatString,
          "m=video 9 UDP/TLS/RTP/SAVPF 35 96 97 98 99 36 45 46 47 48 119 120"));
}

TEST(SdpMessages, SetPreferredVideoFormat_PayloadDoesNotExist_VP9_Profile2) {
  const std::string original_sdp_contents =
      base::StringPrintf(kPreferredCodecFormatString, kOriginalVideoLine);
  SdpMessage sdp_message(original_sdp_contents);
  sdp_message.SetPreferredVideoFormat(webrtc::SdpVideoFormat::VP9Profile2());
  EXPECT_EQ(sdp_message.ToString(), original_sdp_contents);
}

TEST(SdpMessages, SetPreferredVideoFormat_PayloadExists_AV1_Profile0) {
  SdpMessage sdp_message(
      base::StringPrintf(kPreferredCodecFormatString, kOriginalVideoLine));
  sdp_message.SetPreferredVideoFormat(webrtc::SdpVideoFormat::AV1Profile0());
  EXPECT_EQ(sdp_message.ToString(),
            base::StringPrintf(kPreferredCodecFormatString,
                               "m=video 9 UDP/TLS/RTP/SAVPF 45 96 97 98 99 35 "
                               "36 46 47 48 119 120"));
}

TEST(SdpMessages, SetPreferredVideoFormat_PayloadExists_AV1_Profile1) {
  SdpMessage sdp_message(
      base::StringPrintf(kPreferredCodecFormatString, kOriginalVideoLine));
  sdp_message.SetPreferredVideoFormat(webrtc::SdpVideoFormat::AV1Profile1());
  EXPECT_EQ(sdp_message.ToString(),
            base::StringPrintf(kPreferredCodecFormatString,
                               "m=video 9 UDP/TLS/RTP/SAVPF 47 96 97 98 99 35 "
                               "36 45 46 48 119 120"));
}

TEST(SdpMessages, SetPreferredVideoFormat_PayloadDoesNotExist_H264) {
  const std::string original_sdp_contents =
      base::StringPrintf(kPreferredCodecFormatString, kOriginalVideoLine);
  SdpMessage sdp_message(original_sdp_contents);
  sdp_message.SetPreferredVideoFormat(webrtc::SdpVideoFormat::H264());
  EXPECT_EQ(sdp_message.ToString(), original_sdp_contents);
}

}  // namespace remoting::protocol
