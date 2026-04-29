// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_SIGNALING_SESSION_CONFIG_H_
#define REMOTING_SIGNALING_SESSION_CONFIG_H_

#include <memory>

namespace remoting {

class CandidateSessionConfig;

// SessionConfig is used to represent negotiated session configuration.
class SessionConfig {
 public:
  // Selects session configuration that is supported by both participants.
  // nullptr is returned if such configuration doesn't exist.
  static std::unique_ptr<SessionConfig> SelectCommon(
      const CandidateSessionConfig* client_config,
      const CandidateSessionConfig* host_config);

  // Extracts final protocol configuration. Must be used for the description
  // received in the session-accept stanza.
  static std::unique_ptr<SessionConfig> GetFinalConfig(
      const CandidateSessionConfig* candidate_config);

  // Returns a suitable session configuration for use in tests.
  static std::unique_ptr<SessionConfig> ForTest();

 private:
  SessionConfig();
};

// Defines session description that is sent from client to the host in the
// session-initiate message.
class CandidateSessionConfig {
 public:
  static std::unique_ptr<CandidateSessionConfig> CreateEmpty();
  static std::unique_ptr<CandidateSessionConfig> CreateDefault();

  ~CandidateSessionConfig();

  bool webrtc_supported() const { return webrtc_supported_; }
  void set_webrtc_supported(bool webrtc_supported) {
    webrtc_supported_ = webrtc_supported;
  }

  std::unique_ptr<CandidateSessionConfig> Clone() const;

 private:
  CandidateSessionConfig();
  explicit CandidateSessionConfig(const CandidateSessionConfig& config);

  bool webrtc_supported_ = false;
};

}  // namespace remoting

#endif  // REMOTING_SIGNALING_SESSION_CONFIG_H_
