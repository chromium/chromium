/*
 * Copyright (C) 2011, 2012 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/html/media/media_fragment_uri_parser.h"

#include <string_view>

#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

namespace {

constexpr std::string_view kNptIdentifier = "npt:";

static String CollectDigits(std::string_view input, size_t& position) {
  StringBuilder digits;

  // http://www.ietf.org/rfc/rfc2326.txt
  // DIGIT ; any positive number
  while (position < input.size() && IsASCIIDigit(input[position])) {
    digits.Append(input[position++]);
  }
  return digits.ToString();
}

static String CollectFraction(std::string_view input, size_t& position) {
  StringBuilder digits;

  // http://www.ietf.org/rfc/rfc2326.txt
  // [ "." *DIGIT ]
  if (input[position] != '.')
    return String();

  digits.Append(input[position++]);
  while (position < input.size() && IsASCIIDigit(input[position])) {
    digits.Append(input[position++]);
  }
  return digits.ToString();
}

}  // namespace

MediaFragmentURIParser::MediaFragmentURIParser(const KURL& url)
    : url_(url),
      start_time_(std::numeric_limits<double>::quiet_NaN()),
      end_time_(std::numeric_limits<double>::quiet_NaN()) {}

double MediaFragmentURIParser::StartTime() {
  if (!url_.IsValid()) {
    return std::numeric_limits<double>::quiet_NaN();
  }
  if (!has_parsed_time_) {
    ParseTimeFragment();
  }
  return start_time_;
}

double MediaFragmentURIParser::EndTime() {
  if (!url_.IsValid()) {
    return std::numeric_limits<double>::quiet_NaN();
  }
  if (!has_parsed_time_) {
    ParseTimeFragment();
  }
  return end_time_;
}

Vector<String> MediaFragmentURIParser::DefaultTracks() {
  if (!url_.IsValid()) {
    return {};
  }
  if (!has_parsed_track_) {
    ParseTrackFragment();
  }
  return default_tracks_;
}

void MediaFragmentURIParser::ParseFragments() {
  has_parsed_fragments_ = true;
  if (!url_.HasFragmentIdentifier()) {
    return;
  }
  String fragment_string = url_.FragmentIdentifier().ToString();
  if (fragment_string.empty())
    return;

  wtf_size_t offset = 0;
  wtf_size_t end = fragment_string.length();
  while (offset < end) {
    // http://www.w3.org/2008/WebVideo/Fragments/WD-media-fragments-spec/#processing-name-value-components
    // 1. Parse the octet string according to the namevalues syntax, yielding a
    //    list of name-value pairs, where name and value are both octet string.
    //    In accordance with RFC 3986, the name and value components must be
    //    parsed and separated before percent-encoded octets are decoded.
    wtf_size_t parameter_start = offset;
    wtf_size_t parameter_end = fragment_string.find('&', offset);
    if (parameter_end == kNotFound)
      parameter_end = end;

    wtf_size_t equal_offset = fragment_string.find('=', offset);
    if (equal_offset == kNotFound || equal_offset > parameter_end) {
      offset = parameter_end + 1;
      continue;
    }

    // 2. For each name-value pair:
    //  a. Decode percent-encoded octets in name and value as defined by RFC
    //     3986. If either name or value are not valid percent-encoded strings,
    //     then remove the name-value pair from the list.
    String name = DecodeURLEscapeSequences(
        fragment_string.Substring(parameter_start,
                                  equal_offset - parameter_start),
        DecodeURLMode::kUTF8OrIsomorphic);
    String value;
    if (equal_offset != parameter_end) {
      value = DecodeURLEscapeSequences(
          fragment_string.Substring(equal_offset + 1,
                                    parameter_end - equal_offset - 1),
          DecodeURLMode::kUTF8OrIsomorphic);
    }

    //  b. Convert name and value to Unicode strings by interpreting them as
    //     UTF-8. If either name or value are not valid UTF-8 strings, then
    //     remove the name-value pair from the list.
    bool valid_utf8 = true;
    std::string utf8_name;
    if (!name.empty()) {
      utf8_name = name.Utf8(kStrictUTF8Conversion);
      valid_utf8 = !utf8_name.empty();
    }
    std::string utf8_value;
    if (valid_utf8 && !value.empty()) {
      utf8_value = value.Utf8(kStrictUTF8Conversion);
      valid_utf8 = !utf8_value.empty();
    }

    if (valid_utf8)
      fragments_.emplace_back(std::move(utf8_name), std::move(utf8_value));

    offset = parameter_end + 1;
  }
}

void MediaFragmentURIParser::ParseTrackFragment() {
  has_parsed_track_ = true;
  if (!has_parsed_fragments_) {
    ParseFragments();
  }

  for (const auto& fragment : fragments_) {
    // https://www.w3.org/2008/WebVideo/Fragments/WD-media-fragments-spec/#naming-track
    // Track selection is denoted by the name 'track'. Allowed track names are
    // determined by the original source media, this information has to be known
    // before construction of the media fragment. There is no support for
    // generic media type names.
    if (fragment.first != "track") {
      continue;
    }

    // The fragment value has already been escaped.
    default_tracks_.emplace_back(String::FromUTF8(fragment.second));
  }
}

void MediaFragmentURIParser::ParseTimeFragment() {
  has_parsed_time_ = true;
  if (!has_parsed_fragments_) {
    ParseFragments();
  }

  for (const auto& fragment : fragments_) {
    // http://www.w3.org/2008/WebVideo/Fragments/WD-media-fragments-spec/#naming-time
    // Temporal clipping is denoted by the name t, and specified as an interval
    // with a begin time and an end time
    if (fragment.first != "t")
      continue;

    // http://www.w3.org/2008/WebVideo/Fragments/WD-media-fragments-spec/#npt-time
    // Temporal clipping can be specified either as Normal Play Time (npt) RFC
    // 2326, as SMPTE timecodes, SMPTE, or as real-world clock time (clock) RFC
    // 2326. Begin and end times are always specified in the same format. The
    // format is specified by name, followed by a colon (:), with npt: being the
    // default.

    double start = std::numeric_limits<double>::quiet_NaN();
    double end = std::numeric_limits<double>::quiet_NaN();
    if (ParseNPTFragment(fragment.second, start, end)) {
      start_time_ = start;
      end_time_ = end;

      // Although we have a valid fragment, don't return yet because when a
      // fragment dimensions occurs multiple times, only the last occurrence of
      // that dimension is used:
      // http://www.w3.org/2008/WebVideo/Fragments/WD-media-fragments-spec/#error-uri-general
      // Multiple occurrences of the same dimension: only the last valid
      // occurrence of a dimension (e.g., t=10 in #t=2&t=10) is interpreted, all
      // previous occurrences (valid or invalid) SHOULD be ignored by the UA.
    }
  }
}

bool MediaFragmentURIParser::ParseNPTFragment(std::string_view time_string,
                                              double& start_time,
                                              double& end_time) {
  size_t offset = 0;
  if (time_string.starts_with(kNptIdentifier)) {
    offset += kNptIdentifier.size();
  }

  if (offset == time_string.size()) {
    return false;
  }

  // http://www.w3.org/2008/WebVideo/Fragments/WD-media-fragments-spec/#naming-time
  // If a single number only is given, this corresponds to the begin time except
  // if it is preceded by a comma that would in this case indicate the end time.
  if (time_string[offset] == ',') {
    start_time = 0;
  } else {
    if (!ParseNPTTime(time_string, offset, start_time)) {
      return false;
    }
  }

  if (offset == time_string.size()) {
    return true;
  }

  if (time_string[offset] != ',')
    return false;
  if (++offset == time_string.size()) {
    return false;
  }

  if (!ParseNPTTime(time_string, offset, end_time)) {
    return false;
  }

  if (offset != time_string.size()) {
    return false;
  }

  if (start_time >= end_time)
    return false;

  return true;
}

bool MediaFragmentURIParser::ParseNPTTime(std::string_view time_string,
                                          size_t& offset,
                                          double& time) {
  enum Mode { kMinutes, kHours };
  Mode mode = kMinutes;

  if (offset >= time_string.size() || !IsASCIIDigit(time_string[offset])) {
    return false;
  }

  // http://www.w3.org/2008/WebVideo/Fragments/WD-media-fragments-spec/#npttimedef
  // Normal Play Time can either be specified as seconds, with an optional
  // fractional part to indicate miliseconds, or as colon-separated hours,
  // minutes and seconds (again with an optional fraction). Minutes and
  // seconds must be specified as exactly two digits, hours and fractional
  // seconds can be any number of digits. The hours, minutes and seconds
  // specification for NPT is a convenience only, it does not signal frame
  // accuracy. The specification of the "npt:" identifier is optional since
  // NPT is the default time scheme. This specification builds on the RTSP
  // specification of NPT RFC 2326.
  //
  // ; defined in RFC 2326
  // npt-sec       = 1*DIGIT [ "." *DIGIT ]
  // npt-hhmmss    = npt-hh ":" npt-mm ":" npt-ss [ "." *DIGIT]
  // npt-mmss      = npt-mm ":" npt-ss [ "." *DIGIT]
  // npt-hh        =   1*DIGIT     ; any positive number
  // npt-mm        =   2DIGIT      ; 0-59
  // npt-ss        =   2DIGIT      ; 0-59

  String digits1 = CollectDigits(time_string, offset);
  int value1 = digits1.ToInt();
  if (offset >= time_string.size() || time_string[offset] == ',') {
    time = value1;
    return true;
  }

  double fraction = 0;
  if (time_string[offset] == '.') {
    if (offset == time_string.size()) {
      return true;
    }
    String digits = CollectFraction(time_string, offset);
    fraction = digits.ToDouble();
    time = value1 + fraction;
    return true;
  }

  if (digits1.length() < 1) {
    return false;
  }

  // Collect the next sequence of 0-9 after ':'
  if (offset >= time_string.size() || time_string[offset++] != ':') {
    return false;
  }
  if (offset >= time_string.size() || !IsASCIIDigit(time_string[(offset)])) {
    return false;
  }
  String digits2 = CollectDigits(time_string, offset);
  int value2 = digits2.ToInt();
  if (digits2.length() != 2)
    return false;

  // Detect whether this timestamp includes hours.
  if (offset < time_string.size() && time_string[offset] == ':') {
    mode = kHours;
  }
  if (mode == kMinutes) {
    if (digits1.length() != 2) {
      return false;
    }
    if (value1 > 59 || value2 > 59) {
      return false;
    }
  }

  int value3;
  if (mode == kHours ||
      (offset < time_string.size() && time_string[offset] == ':')) {
    if (offset >= time_string.size() || time_string[offset++] != ':') {
      return false;
    }
    if (offset >= time_string.size() || !IsASCIIDigit(time_string[offset])) {
      return false;
    }
    String digits3 = CollectDigits(time_string, offset);
    if (digits3.length() != 2)
      return false;
    value3 = digits3.ToInt();
    if (value2 > 59 || value3 > 59) {
      return false;
    }
  } else {
    value3 = value2;
    value2 = value1;
    value1 = 0;
  }

  if (offset < time_string.size() && time_string[offset] == '.') {
    fraction = CollectFraction(time_string, offset).ToDouble();
  }

  const int kSecondsPerHour = 3600;
  const int kSecondsPerMinute = 60;
  time = (value1 * kSecondsPerHour) + (value2 * kSecondsPerMinute) + value3 +
         fraction;
  return true;
}

}  // namespace blink
