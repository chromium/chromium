// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/sdp_message.h"

#include <algorithm>
#include <string_view>
#include <utility>

#include "base/notreached.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"

namespace remoting::protocol {

SdpMessage::SdpMessage(const std::string& sdp) {
  sdp_lines_ = base::SplitString(sdp, "\n", base::TRIM_WHITESPACE,
                                 base::SPLIT_WANT_NONEMPTY);
  for (const auto& line : sdp_lines_) {
    if (base::StartsWith(line, "m=audio", base::CompareCase::SENSITIVE)) {
      has_audio_ = true;
    }
    if (base::StartsWith(line, "m=video", base::CompareCase::SENSITIVE)) {
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
  std::vector<std::pair<int, std::string>> payload_types = FindCodec(codec);
  if (payload_types.empty()) {
    return false;
  }

  for (size_t i = 0; i < payload_types.size(); i++) {
    sdp_lines_.insert(
        sdp_lines_.begin() + payload_types[i].first + i + 1,
        "a=fmtp:" + payload_types[i].second + ' ' + parameters_to_add);
  }
  return true;
}

bool SdpMessage::PreferVideoCodec(const std::string& codec) {
  if (!has_video_) {
    return false;
  }
  std::vector<std::pair<int, std::string>> payload_types = FindCodec(codec);
  if (payload_types.empty()) {
    return false;
  }

  for (auto& sdp_line : sdp_lines_) {
    if (!base::StartsWith(sdp_line, "m=video", base::CompareCase::SENSITIVE)) {
      continue;
    }

    // A valid SDP contains only one "m=video" line. So instead of continue, if
    // this line is invalid, we should return false immediately.
    std::vector<std::string_view> fields = base::SplitStringPiece(
        sdp_line, " ", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    // The first three fields are "m=video", port and proto.
    static constexpr int kSkipFields = 3;
    if (fields.size() <= kSkipFields) {
      return false;
    }

    const auto first_codec_pos = fields.begin() + kSkipFields;

    for (const auto& payload : payload_types) {
      auto pos = std::find(first_codec_pos, fields.end(),
                           std::string_view(payload.second));
      // The codec has not been found in codec list.
      if (pos == fields.end()) {
        continue;
      }

      std::rotate(first_codec_pos, pos, pos + 1);
    }

    sdp_line = base::JoinString(fields, " ");
    return true;
  }

  // If has_video_ is true (tested at the very beginning of the function), we
  // should always return within the for-loop above.
  NOTREACHED();
}

std::vector<std::pair<int, std::string>> SdpMessage::FindCodec(
    const std::string& codec) const {
  const std::string kRtpMapPrefix = "a=rtpmap:";
  std::vector<std::pair<int, std::string>> results;
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
      results.push_back(std::make_pair(i, std::move(payload_type)));
    }
  }
  return results;
}

}  // namespace remoting::protocol
