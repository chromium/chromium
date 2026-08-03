// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/session_options.h"

#include <sstream>

#include "base/values.h"
#include "build/build_config.h"
#include "remoting/base/session_options_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

TEST(SessionOptionsTest, ParseAllSupportedFields) {
  base::DictValue dict;
  dict.Set(kSessionOptionDetectUpdatedRegion, "true");
  dict.Set(kSessionOptionCaptureVideoOnDedicatedThread, "0");
  dict.Set(kSessionOptionDisableUdp, "");
  dict.Set(kSessionOptionAv1ActiveMap, "FALSE");
  dict.Set(kSessionOptionVp9EncoderSpeed, "4");
  dict.Set(kSessionOptionAv1EncoderSpeed, "-200");
#if BUILDFLAG(IS_MAC)
  dict.Set(kSessionOptionEnableSckCapturer, "1");
#endif  // BUILDFLAG(IS_MAC)
#if BUILDFLAG(IS_WIN)
  dict.Set(kSessionOptionAllowDxgiCapturer, "TRUE");
#endif  // BUILDFLAG(IS_WIN)

  SessionOptions options = SessionOptions::Parse(dict);
  EXPECT_EQ(options.detect_updated_region, true);
  EXPECT_EQ(options.capture_video_on_dedicated_thread, false);
  EXPECT_EQ(options.disable_udp, true);
  EXPECT_EQ(options.av1_active_map, false);
  EXPECT_EQ(options.vp9_encoder_speed, 4);
  EXPECT_EQ(options.av1_encoder_speed, -200);
#if BUILDFLAG(IS_MAC)
  EXPECT_EQ(options.enable_sck_capturer, true);
#endif  // BUILDFLAG(IS_MAC)
#if BUILDFLAG(IS_WIN)
  EXPECT_EQ(options.allow_dxgi_capturer, true);
#endif  // BUILDFLAG(IS_WIN)
}

TEST(SessionOptionsTest, ParseIgnoresUnsupportedField) {
  base::DictValue dict;
  dict.Set(kSessionOptionDetectUpdatedRegion, "true");
  dict.Set("Unsupported-Key", "foo");

  SessionOptions options = SessionOptions::Parse(dict);
  EXPECT_EQ(options.detect_updated_region, true);
}

TEST(SessionOptionsTest, ParseIgnoresInvalidBool) {
  base::DictValue dict;
  dict.Set(kSessionOptionDetectUpdatedRegion, "not_a_bool");

  EXPECT_EQ(SessionOptions::Parse(dict), SessionOptions());
}

TEST(SessionOptionsTest, ParseIgnoresInvalidInt) {
  base::DictValue dict;
  dict.Set(kSessionOptionVp9EncoderSpeed, "not_an_int");

  EXPECT_EQ(SessionOptions::Parse(dict), SessionOptions());
}

TEST(SessionOptionsTest, IgnoreNonApplicableOsKeys) {
  base::DictValue dict;
  dict.Set(kSessionOptionDetectUpdatedRegion, "true");
#if !BUILDFLAG(IS_MAC)
  dict.Set("Enable-Sck-Capturer", "true");
#endif  // !BUILDFLAG(IS_MAC)
#if !BUILDFLAG(IS_WIN)
  dict.Set("Allow-Dxgi-Capturer", "true");
#endif  // !BUILDFLAG(IS_WIN)

  SessionOptions options = SessionOptions::Parse(dict);
  EXPECT_EQ(options.detect_updated_region, true);
}

TEST(SessionOptionsTest, Equality) {
  SessionOptions options1;
  options1.detect_updated_region = true;
  options1.vp9_encoder_speed = 3;

  SessionOptions options2 = options1;
  EXPECT_EQ(options1, options2);

  options2.vp9_encoder_speed = 4;
  EXPECT_NE(options1, options2);
}

TEST(SessionOptionsTest, StreamOutput) {
  SessionOptions options;
  options.detect_updated_region = true;

  std::ostringstream ss;
  ss << options;
  EXPECT_NE(ss.str().find("Detect-Updated-Region: 1"), std::string::npos);
  EXPECT_NE(ss.str().find("Vp9-Encoder-Speed: <unspecified>"),
            std::string::npos);
}

}  // namespace remoting
