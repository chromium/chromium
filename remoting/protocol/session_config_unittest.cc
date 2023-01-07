// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/session_config.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace remoting::protocol {

void TestGetFinalConfig(std::unique_ptr<SessionConfig> config) {
  std::unique_ptr<CandidateSessionConfig> candidate_config =
      CandidateSessionConfig::CreateFrom(*config);
  ASSERT_TRUE(candidate_config);
  std::unique_ptr<SessionConfig> config2 =
      SessionConfig::GetFinalConfig(candidate_config.get());
  ASSERT_TRUE(config2);
  EXPECT_EQ(config->protocol(), config2->protocol());

  if (config->protocol() == SessionConfig::Protocol::ICE) {
    EXPECT_EQ(config->control_config(), config2->control_config());
    EXPECT_EQ(config->event_config(), config2->event_config());
    EXPECT_EQ(config->video_config(), config2->video_config());
    EXPECT_EQ(config->audio_config(), config2->audio_config());
  }
}

TEST(SessionConfig, SelectCommon) {
  std::unique_ptr<CandidateSessionConfig> default_candidate_config =
      CandidateSessionConfig::CreateDefault();

  std::unique_ptr<CandidateSessionConfig> candidate_config_with_webrtc =
      CandidateSessionConfig::CreateEmpty();
  candidate_config_with_webrtc->set_webrtc_supported(true);

  std::unique_ptr<CandidateSessionConfig> hybrid_candidate_config =
      CandidateSessionConfig::CreateDefault();
  hybrid_candidate_config->set_webrtc_supported(true);

  std::unique_ptr<SessionConfig> selected;

  // ICE is selected by default.
  selected = SessionConfig::SelectCommon(default_candidate_config.get(),
                                         default_candidate_config.get());
  ASSERT_TRUE(selected);
  EXPECT_EQ(SessionConfig::Protocol::ICE, selected->protocol());

  // WebRTC protocol is not supported by default.
  selected = SessionConfig::SelectCommon(default_candidate_config.get(),
                                         candidate_config_with_webrtc.get());
  EXPECT_FALSE(selected);

  // ICE is selected when client supports both protocols
  selected = SessionConfig::SelectCommon(default_candidate_config.get(),
                                         hybrid_candidate_config.get());
  ASSERT_TRUE(selected);
  EXPECT_EQ(SessionConfig::Protocol::ICE, selected->protocol());

  // WebRTC is selected when both peers support it.
  selected = SessionConfig::SelectCommon(candidate_config_with_webrtc.get(),
                                         candidate_config_with_webrtc.get());
  ASSERT_TRUE(selected);
  EXPECT_EQ(SessionConfig::Protocol::WEBRTC, selected->protocol());

  // WebRTC is selected when both peers support it.
  selected = SessionConfig::SelectCommon(candidate_config_with_webrtc.get(),
                                         hybrid_candidate_config.get());
  ASSERT_TRUE(selected);
  EXPECT_EQ(SessionConfig::Protocol::WEBRTC, selected->protocol());

  // ICE is selected if both peers support both protocols.
  selected = SessionConfig::SelectCommon(hybrid_candidate_config.get(),
                                         hybrid_candidate_config.get());
  ASSERT_TRUE(selected);
  EXPECT_EQ(SessionConfig::Protocol::WEBRTC, selected->protocol());
}

TEST(SessionConfig, GetFinalConfig) {
  TestGetFinalConfig(SessionConfig::ForTest());
  TestGetFinalConfig(SessionConfig::ForTestWithWebrtc());
}

TEST(SessionConfig, IsSupported) {
  std::unique_ptr<CandidateSessionConfig> default_candidate_config =
      CandidateSessionConfig::CreateDefault();

  std::unique_ptr<CandidateSessionConfig> candidate_config_with_webrtc =
      CandidateSessionConfig::CreateEmpty();
  candidate_config_with_webrtc->set_webrtc_supported(true);

  std::unique_ptr<CandidateSessionConfig> hybrid_candidate_config =
      CandidateSessionConfig::CreateDefault();
  hybrid_candidate_config->set_webrtc_supported(true);

  std::unique_ptr<SessionConfig> ice_config = SessionConfig::ForTest();
  std::unique_ptr<SessionConfig> webrtc_config =
      SessionConfig::ForTestWithWebrtc();

  EXPECT_TRUE(default_candidate_config->IsSupported(*ice_config));
  EXPECT_FALSE(default_candidate_config->IsSupported(*webrtc_config));

  EXPECT_FALSE(candidate_config_with_webrtc->IsSupported(*ice_config));
  EXPECT_TRUE(candidate_config_with_webrtc->IsSupported(*webrtc_config));

  EXPECT_TRUE(hybrid_candidate_config->IsSupported(*ice_config));
  EXPECT_TRUE(hybrid_candidate_config->IsSupported(*webrtc_config));
}

}  // namespace remoting::protocol
