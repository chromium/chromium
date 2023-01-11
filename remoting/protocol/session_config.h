// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_SESSION_CONFIG_H_
#define REMOTING_PROTOCOL_SESSION_CONFIG_H_

#include <list>
#include <memory>
#include <string>

namespace remoting::protocol {

extern const int kDefaultStreamVersion;

// Struct for configuration parameters of a single channel.
// Some channels (like video) may have multiple underlying sockets that need
// to be configured simultaneously.
struct ChannelConfig {
  enum TransportType {
    TRANSPORT_STREAM,
    TRANSPORT_MUX_STREAM,
    TRANSPORT_DATAGRAM,
    TRANSPORT_NONE,
  };

  enum Codec {
    CODEC_UNDEFINED,  // Used for event and control channels.
    CODEC_VERBATIM,
    CODEC_ZIP,
    CODEC_VP8,
    CODEC_VP9,
    CODEC_H264,
    CODEC_OPUS,
    CODEC_SPEEX,
    CODEC_AV1,
  };

  // Creates a config with transport field set to TRANSPORT_NONE which indicates
  // that corresponding channel is disabled.
  static ChannelConfig None();

  // Default constructor. Equivalent to None().
  ChannelConfig() = default;

  // Creates a channel config with the specified parameters.
  ChannelConfig(TransportType transport, int version, Codec codec);

  // operator== is overloaded so that std::find() works with
  // std::list<ChannelConfig>.
  bool operator==(const ChannelConfig& b) const;

  TransportType transport = TRANSPORT_NONE;
  int version = 0;
  Codec codec = CODEC_UNDEFINED;
};

class CandidateSessionConfig;

// SessionConfig is used to represent negotiated session configuration. Note
// that it's useful mainly for the legacy protocol. When using the new
// WebRTC-based protocol the using_webrtc() flag is set to true and all other
// fields should be ignored.
class SessionConfig {
 public:
  enum class Protocol {
    // ICE-based protocol (aka Chromotocol).
    ICE,

    // WebRTC-based protocol.
    WEBRTC,
  };

  // Selects session configuration that is supported by both participants.
  // nullptr is returned if such configuration doesn't exist. When selecting
  // channel configuration priority is given to the configs listed first
  // in |client_config|.
  static std::unique_ptr<SessionConfig> SelectCommon(
      const CandidateSessionConfig* client_config,
      const CandidateSessionConfig* host_config);

  // Extracts final protocol configuration. Must be used for the description
  // received in the session-accept stanza. If the selection is ambiguous
  // (e.g. there is more than one configuration for one of the channels)
  // or undefined (e.g. no configurations for a channel) then nullptr is
  // returned.
  static std::unique_ptr<SessionConfig> GetFinalConfig(
      const CandidateSessionConfig* candidate_config);

  // Returns a suitable session configuration for use in tests.
  static std::unique_ptr<SessionConfig> ForTest();
  static std::unique_ptr<SessionConfig> ForTestWithAudio();
  static std::unique_ptr<SessionConfig> ForTestWithVerbatimVideo();
  static std::unique_ptr<SessionConfig> ForTestWithWebrtc();

  Protocol protocol() const { return protocol_; }

  // All fields below should be ignored when protocol() is set to WEBRTC.
  const ChannelConfig& control_config() const;
  const ChannelConfig& event_config() const;
  const ChannelConfig& video_config() const;
  const ChannelConfig& audio_config() const;

  bool is_audio_enabled() const {
    return audio_config_.transport != ChannelConfig::TRANSPORT_NONE;
  }

 private:
  SessionConfig(Protocol protocol);

  const Protocol protocol_;

  ChannelConfig control_config_;
  ChannelConfig event_config_;
  ChannelConfig video_config_;
  ChannelConfig audio_config_;
};

// Defines session description that is sent from client to the host in the
// session-initiate message. It is different from the regular Config
// because it allows one to specify multiple configurations for each channel.
class CandidateSessionConfig {
 public:
  static std::unique_ptr<CandidateSessionConfig> CreateEmpty();
  static std::unique_ptr<CandidateSessionConfig> CreateFrom(
      const SessionConfig& config);
  static std::unique_ptr<CandidateSessionConfig> CreateDefault();

  ~CandidateSessionConfig();

  bool webrtc_supported() const { return webrtc_supported_; }
  void set_webrtc_supported(bool webrtc_supported) {
    webrtc_supported_ = webrtc_supported;
  }

  bool ice_supported() const { return ice_supported_; }
  void set_ice_supported(bool ice_supported) { ice_supported_ = ice_supported; }

  const std::list<ChannelConfig>& control_configs() const {
    return control_configs_;
  }

  std::list<ChannelConfig>* mutable_control_configs() {
    return &control_configs_;
  }

  const std::list<ChannelConfig>& event_configs() const {
    return event_configs_;
  }

  std::list<ChannelConfig>* mutable_event_configs() { return &event_configs_; }

  const std::list<ChannelConfig>& video_configs() const {
    return video_configs_;
  }

  std::list<ChannelConfig>* mutable_video_configs() { return &video_configs_; }

  const std::list<ChannelConfig>& audio_configs() const {
    return audio_configs_;
  }

  std::list<ChannelConfig>* mutable_audio_configs() { return &audio_configs_; }

  // Returns true if |config| is supported.
  bool IsSupported(const SessionConfig& config) const;

  std::unique_ptr<CandidateSessionConfig> Clone() const;

  // Helpers for enabling/disabling specific features.
  void DisableAudioChannel();
  void PreferTransport(ChannelConfig::TransportType transport);

 private:
  CandidateSessionConfig();
  explicit CandidateSessionConfig(const CandidateSessionConfig& config);
  CandidateSessionConfig& operator=(const CandidateSessionConfig& b);

  bool webrtc_supported_ = false;
  bool ice_supported_ = false;

  std::list<ChannelConfig> control_configs_;
  std::list<ChannelConfig> event_configs_;
  std::list<ChannelConfig> video_configs_;
  std::list<ChannelConfig> audio_configs_;
};

}  // namespace remoting::protocol

#endif  // REMOTING_PROTOCOL_SESSION_CONFIG_H_
