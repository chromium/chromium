// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/formats/webm/webm_info_parser.h"

#include "base/logging.h"
#include "media/formats/webm/webm_constants.h"

namespace media {

// Default timecode scale, in nanoseconds, if the TimecodeScale element is not
// specified in the INFO element.
static const int kWebMDefaultTimecodeScale = 1000000;

WebMInfoParser::WebMInfoParser() : timecode_scale_ns_(-1), duration_(-1) {}

WebMInfoParser::~WebMInfoParser() = default;

int WebMInfoParser::Parse(const uint8_t* buf, int size) {
  timecode_scale_ns_ = -1;
  duration_ = -1;

  WebMListParser parser(kWebMIdInfo, this);
  int result = parser.Parse(buf, size);

  if (result <= 0)
    return result;

  // For now we do all or nothing parsing.
  return parser.IsParsingComplete() ? result : 0;
}

WebMParserClient* WebMInfoParser::OnListStart(int id) { return this; }

bool WebMInfoParser::OnListEnd(int id) {
  if (id == kWebMIdInfo && timecode_scale_ns_ == -1) {
    // Set timecode scale to default value if it isn't present in
    // the Info element.
    timecode_scale_ns_ = kWebMDefaultTimecodeScale;
  }
  return true;
}

bool WebMInfoParser::OnUInt(int id, int64_t val) {
  if (id != kWebMIdTimecodeScale)
    return true;

  if (val <= 0) {
    DVLOG(1) << "TimeCodeScale of " << val << " is invalid. Must be > 0.";
    return false;
  }

  if (timecode_scale_ns_ != -1) {
    DVLOG(1) << "Multiple values for id " << std::hex << id << " specified";
    return false;
  }

  timecode_scale_ns_ = val;
  return true;
}

bool WebMInfoParser::OnFloat(int id, double val) {
  if (id != kWebMIdDuration) {
    DVLOG(1) << "Unexpected float for id" << std::hex << id;
    return false;
  }

  if (duration_ != -1) {
    DVLOG(1) << "Multiple values for duration.";
    return false;
  }

  duration_ = val;
  return true;
}

bool WebMInfoParser::OnBinary(int id, const uint8_t* data, int size) {
  if (id == kWebMIdDateUTC) {
    if (size != 8)
      return false;

    int64_t date_in_nanoseconds = 0;
    for (int i = 0; i < size; ++i)
      date_in_nanoseconds = (date_in_nanoseconds << 8) | data[i];

    static constexpr base::Time::Exploded kExplodedEpoch = {
        .year = 2001, .month = 1, .day_of_week = 1, .day_of_month = 1};
    base::Time out_time;
    if (!base::Time::FromUTCExploded(kExplodedEpoch, &out_time)) {
      return false;
    }
    date_utc_ = out_time + base::Microseconds(date_in_nanoseconds / 1000);
  }
  return true;
}

bool WebMInfoParser::OnString(int id, const std::string& str) {
  return true;
}

}  // namespace media
