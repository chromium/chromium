// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/signaling/session_config.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

TEST(SessionConfig, SelectCommon) {
  std::unique_ptr<CandidateSessionConfig> client_config =
      CandidateSessionConfig::CreateDefault();
  std::unique_ptr<CandidateSessionConfig> host_config =
      CandidateSessionConfig::CreateDefault();

  std::unique_ptr<SessionConfig> selected =
      SessionConfig::SelectCommon(client_config.get(), host_config.get());
  ASSERT_TRUE(selected);

  // WebRTC is not supported by either peer.
  client_config->set_webrtc_supported(false);
  selected =
      SessionConfig::SelectCommon(client_config.get(), host_config.get());
  EXPECT_FALSE(selected);
}

TEST(SessionConfig, GetFinalConfig) {
  std::unique_ptr<CandidateSessionConfig> candidate_config =
      CandidateSessionConfig::CreateDefault();

  std::unique_ptr<SessionConfig> config =
      SessionConfig::GetFinalConfig(candidate_config.get());
  ASSERT_TRUE(config);

  candidate_config->set_webrtc_supported(false);
  config = SessionConfig::GetFinalConfig(candidate_config.get());
  EXPECT_FALSE(config);
}

}  // namespace remoting
