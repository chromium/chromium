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

#include "base/check_op.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/re2/src/re2/re2.h"

namespace blink {

namespace {

constexpr std::string_view kNptIdentifier = "npt:";

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
  StringView fragment_string = url_.FragmentIdentifier();
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
    String name = DecodeUrlEscapeSequences(
        fragment_string.substr(parameter_start, equal_offset - parameter_start),
        DecodeUrlMode::kUtf8OrIsomorphic);
    String value;
    if (equal_offset != parameter_end) {
      value = DecodeUrlEscapeSequences(
          fragment_string.substr(equal_offset + 1,
                                 parameter_end - equal_offset - 1),
          DecodeUrlMode::kUtf8OrIsomorphic);
    }

    //  b. Convert name and value to Unicode strings by interpreting them as
    //     UTF-8. If either name or value are not valid UTF-8 strings, then
    //     remove the name-value pair from the list.
    bool valid_utf8 = true;
    std::string utf8_name;
    if (!name.empty()) {
      utf8_name = name.Utf8(Utf8ConversionMode::kStrict);
      valid_utf8 = !utf8_name.empty();
    }
    std::string utf8_value;
    if (valid_utf8 && !value.empty()) {
      utf8_value = value.Utf8(Utf8ConversionMode::kStrict);
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

    // The fragment value has already been URL-decoded.
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
    // The spec allows Normal Play Time (npt), SMPTE timecodes, and real-world
    // clock time (RFC 2326), with npt: as the default. This implementation
    // only supports NPT.

    double start = std::numeric_limits<double>::quiet_NaN();
    double end = std::numeric_limits<double>::quiet_NaN();
    if (ParseNPTFragment(fragment.second, start, end)) {
      start_time_ = start;
      end_time_ = end;

      // Although we have a valid fragment, don't return yet because when a
      // fragment dimension occurs multiple times, only the last occurrence of
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
  std::string_view s = time_string;
  if (s.starts_with(kNptIdentifier)) {
    s.remove_prefix(kNptIdentifier.size());
  }
  if (s.empty()) {
    return false;
  }

  // http://www.w3.org/2008/WebVideo/Fragments/WD-media-fragments-spec/#naming-time
  // If a single number only is given, this corresponds to the begin time except
  // if it is preceded by a comma that would in this case indicate the end time.
  size_t offset = 0;
  if (s[0] == ',') {
    start_time = 0;
  } else if (!ParseNPTTime(s, offset, start_time)) {
    return false;
  }

  if (offset == s.size()) {
    return true;
  }

  // Invariant: s[offset] == ',' — ParseNPTTime stops at ',' or end (end
  // returned above); the s[0]==',' path also leaves offset=0.
  DCHECK_EQ(s[offset], ',');
  if (++offset == s.size()) {
    return false;
  }

  return ParseNPTTime(s, offset, end_time) && offset == s.size() &&
         start_time < end_time;
}

bool MediaFragmentURIParser::ParseNPTTime(std::string_view time_string,
                                          size_t& offset,
                                          double& time) {
  // http://www.w3.org/2008/WebVideo/Fragments/WD-media-fragments-spec/#npttimedef
  // NPT (RFC 2326): plain seconds with optional fraction, or colon-separated
  // HH:MM:SS / MM:SS with optional fraction. MM and SS are exactly two digits;
  // HH can be any number of digits.
  //
  // ; defined in RFC 2326
  // npt-sec       = 1*DIGIT [ "." *DIGIT ]
  // npt-hhmmss    = npt-hh ":" npt-mm ":" npt-ss [ "." *DIGIT]
  // npt-mmss      = npt-mm ":" npt-ss [ "." *DIGIT]
  // npt-hh        =   1*DIGIT     ; any non-negative integer
  // npt-mm        =   2DIGIT      ; 0-59
  // npt-ss        =   2DIGIT      ; 0-59
  //
  // The regex alternation tries HH:MM:SS first, then MM:SS, then
  // seconds-only. The fractional seconds group (7) is factored out and applies
  // to all formats:
  //   1-3: HH, MM, SS  (HH:MM:SS format)
  //   4-5: MM, SS      (MM:SS format)
  //   6:   SS          (seconds-only format)
  //   7:   frac        (optional fractional seconds)
  static const base::NoDestructor<re2::RE2> kNPTTimeRegex(
      R"((?:(\d+):(\d{2}):(\d{2})|(\d{2}):(\d{2})|(\d+))(\.\d*)?)");

  re2::StringPiece sp = time_string.substr(offset);
  re2::StringPiece hh, hh_mm, hh_ss;
  re2::StringPiece mm, mm_ss;
  re2::StringPiece sec, frac;
  if (!re2::RE2::Consume(&sp, *kNPTTimeRegex, &hh, &hh_mm, &hh_ss, &mm, &mm_ss,
                         &sec, &frac)) {
    return false;
  }

  // The time must be followed by ',' (separating start from end) or end of
  // string.
  if (!sp.empty() && sp[0] != ',') {
    return false;
  }

  offset = time_string.size() - sp.size();

  double frac_val = 0.0;
  if (!frac.empty()) {
    // For the \.\d* regex match, StringToDouble only fails for a bare '.',
    // which strtod maps to 0.0 — the initial value — so no reset is needed.
    base::StringToDouble(frac, &frac_val);
  }

  // \d{2} guarantees two ASCII digits → value in [0, 99], no overflow.
  auto TwoDigitToInt = [](re2::StringPiece s) {
    return (s[0] - '0') * 10 + (s[1] - '0');
  };

  if (!hh.empty()) {
    // HH:MM:SS[.frac]: HH (\d+) uses StringToInt to reject out-of-range values;
    // double arithmetic avoids signed int overflow UB. MM/SS (\d{2}): safe.
    int hh_val;
    if (!base::StringToInt(hh, &hh_val)) {
      return false;
    }
    int mm_val = TwoDigitToInt(hh_mm);
    int ss_val = TwoDigitToInt(hh_ss);
    if (mm_val > 59 || ss_val > 59) {
      return false;
    }
    time = static_cast<double>(hh_val) * 3600.0 +
           static_cast<double>(mm_val) * 60.0 + static_cast<double>(ss_val) +
           frac_val;
  } else if (!mm.empty()) {
    // MM:SS[.frac] format. Both use \d{2} so conversion is always safe.
    int mm_val = TwoDigitToInt(mm);
    int ss_val = TwoDigitToInt(mm_ss);
    if (mm_val > 59 || ss_val > 59) {
      return false;
    }
    time = mm_val * 60 + ss_val + frac_val;
  } else {
    // Seconds-only format. Values outside int range are rejected, instead of
    // being silently coerced.
    int sec_val;
    if (!base::StringToInt(sec, &sec_val)) {
      return false;
    }
    time = sec_val + frac_val;
  }

  return true;
}

}  // namespace blink
