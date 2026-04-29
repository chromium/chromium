// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/signaling/session_config.h"

#include <algorithm>
#include <vector>

#include "base/check.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"

namespace remoting {

// static
std::unique_ptr<SessionConfig> SessionConfig::SelectCommon(
    const CandidateSessionConfig* client_config,
    const CandidateSessionConfig* host_config) {
  // Use WebRTC if both host and client support it.
  if (client_config->webrtc_supported() && host_config->webrtc_supported()) {
    return base::WrapUnique(new SessionConfig());
  }

  return nullptr;
}

// static
std::unique_ptr<SessionConfig> SessionConfig::GetFinalConfig(
    const CandidateSessionConfig* candidate_config) {
  if (candidate_config->webrtc_supported()) {
    return base::WrapUnique(new SessionConfig());
  }

  return nullptr;
}

// static
std::unique_ptr<SessionConfig> SessionConfig::ForTest() {
  return base::WrapUnique(new SessionConfig());
}

SessionConfig::SessionConfig() = default;

CandidateSessionConfig::CandidateSessionConfig() = default;
CandidateSessionConfig::CandidateSessionConfig(
    const CandidateSessionConfig& config) = default;
CandidateSessionConfig::~CandidateSessionConfig() = default;

std::unique_ptr<CandidateSessionConfig> CandidateSessionConfig::Clone() const {
  return base::WrapUnique(new CandidateSessionConfig(*this));
}

// static
std::unique_ptr<CandidateSessionConfig> CandidateSessionConfig::CreateEmpty() {
  return base::WrapUnique(new CandidateSessionConfig());
}

// static
std::unique_ptr<CandidateSessionConfig>
CandidateSessionConfig::CreateDefault() {
  std::unique_ptr<CandidateSessionConfig> result = CreateEmpty();
  result->set_webrtc_supported(true);
  return result;
}

}  // namespace remoting
