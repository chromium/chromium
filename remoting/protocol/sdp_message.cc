// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/sdp_message.h"

#include <algorithm>
#include <string>
#include <string_view>
#include <utility>

#include "base/logging.h"
#include "base/notreached.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "third_party/webrtc/api/video_codecs/sdp_video_format.h"
#include "third_party/webrtc/media/base/media_constants.h"

namespace remoting::protocol {

namespace {

using webrtc::kAv1CodecName;
using webrtc::kVp9CodecName;

// The fmtp constants are in media_constants.h but they are not exported.
// TODO: joedow - Switch over to the webrtc constants if they are exported.
constexpr char kAv1FmtpProfile[] = "profile";
constexpr char kVP9ProfileId[] = "profile-id";

constexpr std::string_view kAudioLinePrefix = "m=audio";
constexpr std::string_view kVideoLinePrefix = "m=video";
constexpr std::string_view kFmtpLinePrefix = "a=fmtp:";
constexpr std::string_view kRtpMapPrefix = "a=rtpmap:";

}  // namespace

SdpMessage::SdpMessage(const std::string& sdp) {
  sdp_lines_ = base::SplitString(sdp, "\n", base::TRIM_WHITESPACE,
                                 base::SPLIT_WANT_NONEMPTY);
  for (const auto& line : sdp_lines_) {
    if (base::StartsWith(line, kAudioLinePrefix,
                         base::CompareCase::SENSITIVE)) {
      has_audio_ = true;
    }
    if (base::StartsWith(line, kVideoLinePrefix,
                         base::CompareCase::SENSITIVE)) {
      has_video_ = true;
    }
  }
}

SdpMessage::~SdpMessage() = default;

std::string SdpMessage::ToString() const {
  return base::JoinString(sdp_lines_, "\r\n") + "\r\n";
}

std::string SdpMessage::NormalizedForSignature() const {
  return base::JoinString(sdp_lines_, "\n") + "\n";
}

bool SdpMessage::AddCodecParameter(const std::string& codec,
                                   const std::string& parameters_to_add) {
  auto payloads = FindCodecPayloads(codec);
  if (payloads.empty()) {
    return false;
  }

  for (size_t i = 0; i < payloads.size(); i++) {
    sdp_lines_.insert(sdp_lines_.begin() + payloads[i].index + i + 1,
                      base::StringPrintf("%s%s %s", kFmtpLinePrefix,
                                         payloads[i].type, parameters_to_add));
  }
  return true;
}

void SdpMessage::SetPreferredVideoFormat(const webrtc::SdpVideoFormat& format) {
  // In order to find a matching codec, we need to also look at the fmtp line
  // to match the profile if the codec is VP9 or AV1. If a profile is not
  // explicitly set, then profile 0 should be used.
  auto payloads =
      FindCodecPayloads(format.name, GetFmtpFragmentForSdpVideoFormat(format));

  // There should only be one matching payload so if the codec + profile does
  // not exist or there are duplicates, then we'll just use the default since
  // that is safe (the default is VP8 which is a required codec for WebRTC).
  if (payloads.size() != 1) {
    LOG(WARNING) << "SDP does not contain a payload for: " << format.ToString();
    return;
  }
  // |payload_type| is a number like '98' or '45'.
  auto payload_type = payloads.begin()->type;

  // Reorder the payloads within the video line to set the preferred codec.
  for (auto& line : sdp_lines_) {
    if (!line.starts_with(kVideoLinePrefix)) {
      continue;
    }

    auto video_line_parts = base::SplitString(line, " ", base::TRIM_WHITESPACE,
                                              base::SPLIT_WANT_NONEMPTY);
    // The video line looks similar to this:
    // m=video 9 UDP/TLS/RTP/SAVPF 96 97 98 99 35 36 45 46 47 48 119 120 121
    //
    // The list of numeric values are the payloads so we can ignore the first
    // three indices.
    const size_t kPayloadStartIndex = 3;
    for (size_t i = kPayloadStartIndex; i < video_line_parts.size(); i++) {
      if (video_line_parts[i] == payload_type) {
        // Found the payload, so shift the existing values over and then copy
        // the preferred value into the first payload index.
        for (size_t j = i; j > kPayloadStartIndex; j--) {
          video_line_parts[j] = video_line_parts[j - 1];
        }
        video_line_parts[kPayloadStartIndex] = payload_type;
        break;
      }
    }
    line = base::JoinString(video_line_parts, " ");
    break;
  }
}

SdpMessage::Payloads SdpMessage::FindCodecPayloads(
    const std::string& codec) const {
  Payloads results;
  for (size_t i = 0; i < sdp_lines_.size(); ++i) {
    const auto& line = sdp_lines_[i];
    if (!base::StartsWith(line, kRtpMapPrefix, base::CompareCase::SENSITIVE)) {
      continue;
    }
    size_t space_pos = line.find(' ');
    if (space_pos == std::string::npos) {
      continue;
    }
    if (line.substr(space_pos + 1, codec.size()) == codec &&
        line[space_pos + 1 + codec.size()] == '/') {
      std::string payload_type =
          line.substr(kRtpMapPrefix.size(), space_pos - kRtpMapPrefix.size());
      results.push_back({i, std::move(payload_type)});
    }
  }
  return results;
}

SdpMessage::Payloads SdpMessage::FindCodecPayloads(
    const std::string& codec,
    const std::string& fmtp_param) const {
  auto payloads = FindCodecPayloads(codec);
  // Return the map if there are no entries or if |fmtp_param| is empty since we
  // don't need to do any additional filtering.
  if (payloads.empty() || fmtp_param.empty()) {
    return payloads;
  }

  for (const auto& line : sdp_lines_) {
    if (!base::StartsWith(line, kFmtpLinePrefix,
                          base::CompareCase::SENSITIVE)) {
      continue;
    }

    // If we find an fmtp line with a matching profile value, then check to see
    // if the payload matches any of the values in the |payloads|.
    if (line.find(fmtp_param) != std::string::npos) {
      for (const auto& [index, payload_type] : payloads) {
        auto fmtp_with_payload =
            base::StringPrintf("%s%s ", kFmtpLinePrefix, payload_type);
        if (base::StartsWith(line, fmtp_with_payload,
                             base::CompareCase::SENSITIVE)) {
          return {{.index = index, .type = payload_type}};
        }
      }
    }
  }

  // Return the unfiltered set of payloads if no fmtp matches were found.
  return payloads;
}

std::string SdpMessage::GetFmtpFragmentForSdpVideoFormat(
    const webrtc::SdpVideoFormat& format) const {
  const char* fmtp_profile_key = nullptr;
  if (format.name == kVp9CodecName) {
    fmtp_profile_key = kVP9ProfileId;
  } else if (format.name == kAv1CodecName) {
    fmtp_profile_key = kAv1FmtpProfile;
  }

  std::string fmtp_fragment;
  if (fmtp_profile_key) {
    const auto fmtp_entry = format.parameters.find(fmtp_profile_key);
    if (fmtp_entry != format.parameters.end()) {
      const auto& [key, value] = *fmtp_entry;
      fmtp_fragment = base::StringPrintf("%s=%s", key, value);
    }
  }

  return fmtp_fragment;
}

}  // namespace remoting::protocol
