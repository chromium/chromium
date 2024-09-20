// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/session_config.h"

#include <vector>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"

namespace remoting::protocol {

namespace {

bool SelectCommonChannelConfig(const std::list<ChannelConfig>& host_configs,
                               const std::list<ChannelConfig>& client_configs,
                               ChannelConfig* config) {
  // Usually each of these lists will contain just a few elements, so iterating
  // over all of them is not a problem.
  std::list<ChannelConfig>::const_iterator it;
  for (it = client_configs.begin(); it != client_configs.end(); ++it) {
    if (base::Contains(host_configs, *it)) {
      *config = *it;
      return true;
    }
  }
  return false;
}

void UpdateConfigListToPreferTransport(std::list<ChannelConfig>* configs,
                                       ChannelConfig::TransportType transport) {
  std::vector<ChannelConfig> sorted(configs->begin(), configs->end());
  std::stable_sort(sorted.begin(), sorted.end(),
                   [transport](const ChannelConfig& a, const ChannelConfig& b) {
                     // |a| must precede |b| if |a| uses preferred transport and
                     // |b| doesn't.
                     return a.transport == transport &&
                            b.transport != transport;
                   });
  configs->assign(sorted.begin(), sorted.end());
}

}  // namespace

const int kDefaultStreamVersion = 2;
const int kControlStreamVersion = 3;

ChannelConfig ChannelConfig::None() {
  return ChannelConfig();
}

ChannelConfig::ChannelConfig(TransportType transport, int version, Codec codec)
    : transport(transport), version(version), codec(codec) {}

bool ChannelConfig::operator==(const ChannelConfig& b) const {
  // If the transport field is set to NONE then all other fields are irrelevant.
  if (transport == ChannelConfig::TRANSPORT_NONE) {
    return transport == b.transport;
  }
  return transport == b.transport && version == b.version && codec == b.codec;
}

// static
std::unique_ptr<SessionConfig> SessionConfig::SelectCommon(
    const CandidateSessionConfig* client_config,
    const CandidateSessionConfig* host_config) {
  // Use WebRTC if both host and client support it.
  if (client_config->webrtc_supported() && host_config->webrtc_supported()) {
    return base::WrapUnique(new SessionConfig(Protocol::WEBRTC));
  }

  // Reject connection if ICE is not supported by either of the peers.
  if (!host_config->ice_supported() || !client_config->ice_supported()) {
    return nullptr;
  }

  std::unique_ptr<SessionConfig> result(new SessionConfig(Protocol::ICE));

  std::list<ChannelConfig> host_video_configs = host_config->video_configs();
  host_video_configs.remove_if([](const ChannelConfig& config) {
    // Older ICE-based clients do not support VP9, H.264, or AV1 so remove them.
    return config.codec == ChannelConfig::CODEC_H264 ||
           config.codec == ChannelConfig::CODEC_VP9 ||
           config.codec == ChannelConfig::CODEC_AV1;
  });

  if (!SelectCommonChannelConfig(host_config->control_configs(),
                                 client_config->control_configs(),
                                 &result->control_config_) ||
      !SelectCommonChannelConfig(host_config->event_configs(),
                                 client_config->event_configs(),
                                 &result->event_config_) ||
      !SelectCommonChannelConfig(host_video_configs,
                                 client_config->video_configs(),
                                 &result->video_config_) ||
      !SelectCommonChannelConfig(host_config->audio_configs(),
                                 client_config->audio_configs(),
                                 &result->audio_config_)) {
    return nullptr;
  }

  return result;
}

// static
std::unique_ptr<SessionConfig> SessionConfig::GetFinalConfig(
    const CandidateSessionConfig* candidate_config) {
  if (candidate_config->webrtc_supported()) {
    if (candidate_config->ice_supported()) {
      LOG(ERROR) << "Received candidate config is ambiguous.";
      return nullptr;
    }
    return base::WrapUnique(new SessionConfig(Protocol::WEBRTC));
  }

  if (!candidate_config->ice_supported()) {
    return nullptr;
  }

  if (candidate_config->control_configs().size() != 1 ||
      candidate_config->event_configs().size() != 1 ||
      candidate_config->video_configs().size() != 1 ||
      candidate_config->audio_configs().size() != 1) {
    return nullptr;
  }

  std::unique_ptr<SessionConfig> result(new SessionConfig(Protocol::ICE));
  result->control_config_ = candidate_config->control_configs().front();
  result->event_config_ = candidate_config->event_configs().front();
  result->video_config_ = candidate_config->video_configs().front();
  result->audio_config_ = candidate_config->audio_configs().front();

  return result;
}

// static
std::unique_ptr<SessionConfig> SessionConfig::ForTest() {
  std::unique_ptr<SessionConfig> result(new SessionConfig(Protocol::ICE));
  result->control_config_ =
      ChannelConfig(ChannelConfig::TRANSPORT_MUX_STREAM, kControlStreamVersion,
                    ChannelConfig::CODEC_UNDEFINED);
  result->event_config_ =
      ChannelConfig(ChannelConfig::TRANSPORT_MUX_STREAM, kDefaultStreamVersion,
                    ChannelConfig::CODEC_UNDEFINED);
  result->video_config_ =
      ChannelConfig(ChannelConfig::TRANSPORT_STREAM, kDefaultStreamVersion,
                    ChannelConfig::CODEC_VP8);
  result->audio_config_ =
      ChannelConfig(ChannelConfig::TRANSPORT_MUX_STREAM, kDefaultStreamVersion,
                    ChannelConfig::CODEC_OPUS);
  return result;
}

std::unique_ptr<SessionConfig> SessionConfig::ForTestWithAudio() {
  std::unique_ptr<SessionConfig> result(ForTest());
  result->audio_config_ =
      ChannelConfig(ChannelConfig::TRANSPORT_STREAM, kDefaultStreamVersion,
                    ChannelConfig::CODEC_OPUS);
  return result;
}

std::unique_ptr<SessionConfig> SessionConfig::ForTestWithVerbatimVideo() {
  std::unique_ptr<SessionConfig> result = ForTest();
  result->video_config_ =
      ChannelConfig(ChannelConfig::TRANSPORT_STREAM, kDefaultStreamVersion,
                    ChannelConfig::CODEC_VERBATIM);
  return result;
}

std::unique_ptr<SessionConfig> SessionConfig::ForTestWithWebrtc() {
  return base::WrapUnique(new SessionConfig(Protocol::WEBRTC));
}

const ChannelConfig& SessionConfig::control_config() const {
  DCHECK(protocol_ == Protocol::ICE);
  return control_config_;
}
const ChannelConfig& SessionConfig::event_config() const {
  DCHECK(protocol_ == Protocol::ICE);
  return event_config_;
}
const ChannelConfig& SessionConfig::video_config() const {
  DCHECK(protocol_ == Protocol::ICE);
  return video_config_;
}
const ChannelConfig& SessionConfig::audio_config() const {
  DCHECK(protocol_ == Protocol::ICE);
  return audio_config_;
}

SessionConfig::SessionConfig(Protocol protocol) : protocol_(protocol) {}

CandidateSessionConfig::CandidateSessionConfig() = default;
CandidateSessionConfig::CandidateSessionConfig(
    const CandidateSessionConfig& config) = default;
CandidateSessionConfig::~CandidateSessionConfig() = default;

bool CandidateSessionConfig::IsSupported(const SessionConfig& config) const {
  switch (config.protocol()) {
    case SessionConfig::Protocol::ICE:
      return ice_supported() &&
             base::Contains(control_configs_, config.control_config()) &&
             base::Contains(event_configs_, config.event_config()) &&
             base::Contains(video_configs_, config.video_config()) &&
             base::Contains(audio_configs_, config.audio_config());

    case SessionConfig::Protocol::WEBRTC:
      return webrtc_supported();
  }

  NOTREACHED();
}

std::unique_ptr<CandidateSessionConfig> CandidateSessionConfig::Clone() const {
  return base::WrapUnique(new CandidateSessionConfig(*this));
}

// static
std::unique_ptr<CandidateSessionConfig> CandidateSessionConfig::CreateEmpty() {
  return base::WrapUnique(new CandidateSessionConfig());
}

// static
std::unique_ptr<CandidateSessionConfig> CandidateSessionConfig::CreateFrom(
    const SessionConfig& config) {
  std::unique_ptr<CandidateSessionConfig> result = CreateEmpty();

  switch (config.protocol()) {
    case SessionConfig::Protocol::WEBRTC:
      result->set_webrtc_supported(true);
      result->set_ice_supported(false);
      break;

    case SessionConfig::Protocol::ICE:
      result->set_webrtc_supported(false);
      result->set_ice_supported(true);
      result->mutable_control_configs()->push_back(config.control_config());
      result->mutable_event_configs()->push_back(config.event_config());
      result->mutable_video_configs()->push_back(config.video_config());
      result->mutable_audio_configs()->push_back(config.audio_config());
      break;
  }

  return result;
}

// static
std::unique_ptr<CandidateSessionConfig>
CandidateSessionConfig::CreateDefault() {
  std::unique_ptr<CandidateSessionConfig> result = CreateEmpty();

  result->set_ice_supported(true);

  // Control channel.
  result->mutable_control_configs()->push_back(
      ChannelConfig(ChannelConfig::TRANSPORT_MUX_STREAM, kControlStreamVersion,
                    ChannelConfig::CODEC_UNDEFINED));

  // Event channel.
  result->mutable_event_configs()->push_back(
      ChannelConfig(ChannelConfig::TRANSPORT_MUX_STREAM, kDefaultStreamVersion,
                    ChannelConfig::CODEC_UNDEFINED));

  // Video channel.
  result->mutable_video_configs()->push_back(
      ChannelConfig(ChannelConfig::TRANSPORT_STREAM, kDefaultStreamVersion,
                    ChannelConfig::CODEC_VP9));
  result->mutable_video_configs()->push_back(
      ChannelConfig(ChannelConfig::TRANSPORT_STREAM, kDefaultStreamVersion,
                    ChannelConfig::CODEC_VP8));

  // Audio channel.
  result->mutable_audio_configs()->push_back(
      ChannelConfig(ChannelConfig::TRANSPORT_MUX_STREAM, kDefaultStreamVersion,
                    ChannelConfig::CODEC_OPUS));
  result->mutable_audio_configs()->push_back(ChannelConfig::None());

  return result;
}

void CandidateSessionConfig::DisableAudioChannel() {
  mutable_audio_configs()->clear();
  mutable_audio_configs()->push_back(ChannelConfig());
}

void CandidateSessionConfig::PreferTransport(
    ChannelConfig::TransportType transport) {
  UpdateConfigListToPreferTransport(&control_configs_, transport);
  UpdateConfigListToPreferTransport(&event_configs_, transport);
  UpdateConfigListToPreferTransport(&video_configs_, transport);
  UpdateConfigListToPreferTransport(&audio_configs_, transport);
}

}  // namespace remoting::protocol
