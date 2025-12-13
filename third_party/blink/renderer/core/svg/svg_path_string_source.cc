/*
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 * Copyright (C) 2013 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "third_party/blink/renderer/core/svg/svg_path_string_source.h"

#include "base/compiler_specific.h"
#include "base/notreached.h"
#include "third_party/blink/renderer/core/svg/svg_parser_utilities.h"
#include "ui/gfx/geometry/point_f.h"

namespace blink {

namespace {

// only used to parse largeArcFlag and sweepFlag which must be a "0" or "1"
// and might not have any whitespace/comma after it
template <typename CharType>
std::optional<bool> ParseArcFlag(base::span<const CharType>& span) {
  if (span.empty()) {
    return std::nullopt;
  }
  bool flag;
  const CharType flag_char = span[0];
  if (flag_char == '0') {
    flag = false;
  } else if (flag_char == '1') {
    flag = true;
  } else {
    return std::nullopt;
  }

  span.take_first_elem();
  SkipOptionalSVGSpacesOrDelimiter(span);

  return flag;
}

SVGPathSegType MapLetterToSegmentType(unsigned lookahead) {
  switch (lookahead) {
    case 'Z':
    case 'z':
      return kPathSegClosePath;
    case 'M':
      return kPathSegMoveToAbs;
    case 'm':
      return kPathSegMoveToRel;
    case 'L':
      return kPathSegLineToAbs;
    case 'l':
      return kPathSegLineToRel;
    case 'C':
      return kPathSegCurveToCubicAbs;
    case 'c':
      return kPathSegCurveToCubicRel;
    case 'Q':
      return kPathSegCurveToQuadraticAbs;
    case 'q':
      return kPathSegCurveToQuadraticRel;
    case 'A':
      return kPathSegArcAbs;
    case 'a':
      return kPathSegArcRel;
    case 'H':
      return kPathSegLineToHorizontalAbs;
    case 'h':
      return kPathSegLineToHorizontalRel;
    case 'V':
      return kPathSegLineToVerticalAbs;
    case 'v':
      return kPathSegLineToVerticalRel;
    case 'S':
      return kPathSegCurveToCubicSmoothAbs;
    case 's':
      return kPathSegCurveToCubicSmoothRel;
    case 'T':
      return kPathSegCurveToQuadraticSmoothAbs;
    case 't':
      return kPathSegCurveToQuadraticSmoothRel;
    default:
      return kPathSegUnknown;
  }
}

bool IsNumberStart(unsigned lookahead) {
  return (lookahead >= '0' && lookahead <= '9') || lookahead == '+' ||
         lookahead == '-' || lookahead == '.';
}

bool MaybeImplicitCommand(unsigned lookahead,
                          SVGPathSegType previous_command,
                          SVGPathSegType& next_command) {
  // Check if the current lookahead may start a number - in which case it
  // could be the start of an implicit command. The 'close' command does not
  // have any parameters though and hence can't have an implicit
  // 'continuation'.
  if (!IsNumberStart(lookahead) || previous_command == kPathSegClosePath)
    return false;
  // Implicit continuations of moveto command translate to linetos.
  if (previous_command == kPathSegMoveToAbs) {
    next_command = kPathSegLineToAbs;
    return true;
  }
  if (previous_command == kPathSegMoveToRel) {
    next_command = kPathSegLineToRel;
    return true;
  }
  next_command = previous_command;
  return true;
}

}  // namespace

SVGPathStringSource::SVGPathStringSource(StringView source)
    : is_8bit_source_(source.Is8Bit()),
      previous_command_(kPathSegUnknown),
      source_(source) {
  DCHECK(!source.IsNull());

  if (is_8bit_source_) {
    remaining_.span8_ = source_.Span8();
  } else {
    remaining_.span16_ = source_.Span16();
  }
  EatWhitespace();
}

void SVGPathStringSource::EatWhitespace() {
  if (is_8bit_source_) {
    SkipOptionalSVGSpaces(remaining_.span8_);
  } else {
    SkipOptionalSVGSpaces(remaining_.span16_);
  }
}

void SVGPathStringSource::SetErrorMark(SVGParseStatus status) {
  if (error_.Status() != SVGParseStatus::kNoError)
    return;
  size_t locus =
      source_.length() -
      (is_8bit_source_ ? remaining_.span8_.size() : remaining_.span16_.size());
  error_ = SVGParsingError(status, locus);
}

float SVGPathStringSource::ParseNumberWithError() {
  float number_value = 0;
  bool error = is_8bit_source_ ? !ParseNumber(remaining_.span8_, number_value)
                               : !ParseNumber(remaining_.span16_, number_value);
  if (error) [[unlikely]] {
    SetErrorMark(SVGParseStatus::kExpectedNumber);
  }
  return number_value;
}

bool SVGPathStringSource::ParseArcFlagWithError() {
  std::optional<bool> flag_value = is_8bit_source_
                                       ? ParseArcFlag(remaining_.span8_)
                                       : ParseArcFlag(remaining_.span16_);
  if (!flag_value) [[unlikely]] {
    SetErrorMark(SVGParseStatus::kExpectedArcFlag);
    return false;
  }
  return *flag_value;
}

PathSegmentData SVGPathStringSource::ParseSegment() {
  DCHECK(HasMoreData());
  PathSegmentData segment;
  unsigned lookahead =
      is_8bit_source_ ? remaining_.span8_[0] : remaining_.span16_[0];
  SVGPathSegType command = MapLetterToSegmentType(lookahead);
  if (previous_command_ == kPathSegUnknown) [[unlikely]] {
    // First command has to be a moveto.
    if (command != kPathSegMoveToRel && command != kPathSegMoveToAbs) {
      SetErrorMark(SVGParseStatus::kExpectedMoveToCommand);
      return segment;
    }
    // Consume command letter.
    if (is_8bit_source_) {
      remaining_.span8_.take_first_elem();
    } else {
      remaining_.span16_.take_first_elem();
    }
  } else if (command == kPathSegUnknown) {
    // Possibly an implicit command.
    DCHECK_NE(previous_command_, kPathSegUnknown);
    if (!MaybeImplicitCommand(lookahead, previous_command_, command)) {
      SetErrorMark(SVGParseStatus::kExpectedPathCommand);
      return segment;
    }
  } else {
    // Valid explicit command.
    if (is_8bit_source_) {
      remaining_.span8_.take_first_elem();
    } else {
      remaining_.span16_.take_first_elem();
    }
  }

  segment.command = previous_command_ = command;

  DCHECK_EQ(error_.Status(), SVGParseStatus::kNoError);

  switch (segment.command) {
    case kPathSegCurveToCubicRel:
    case kPathSegCurveToCubicAbs:
      segment.point1.set_x(ParseNumberWithError());
      segment.point1.set_y(ParseNumberWithError());
      [[fallthrough]];
    case kPathSegCurveToCubicSmoothRel:
    case kPathSegCurveToCubicSmoothAbs:
      segment.point2.set_x(ParseNumberWithError());
      segment.point2.set_y(ParseNumberWithError());
      [[fallthrough]];
    case kPathSegMoveToRel:
    case kPathSegMoveToAbs:
    case kPathSegLineToRel:
    case kPathSegLineToAbs:
    case kPathSegCurveToQuadraticSmoothRel:
    case kPathSegCurveToQuadraticSmoothAbs:
      segment.target_point.set_x(ParseNumberWithError());
      segment.target_point.set_y(ParseNumberWithError());
      break;
    case kPathSegLineToHorizontalRel:
    case kPathSegLineToHorizontalAbs:
      segment.target_point.set_x(ParseNumberWithError());
      break;
    case kPathSegLineToVerticalRel:
    case kPathSegLineToVerticalAbs:
      segment.target_point.set_y(ParseNumberWithError());
      break;
    case kPathSegClosePath:
      EatWhitespace();
      break;
    case kPathSegCurveToQuadraticRel:
    case kPathSegCurveToQuadraticAbs:
      segment.point1.set_x(ParseNumberWithError());
      segment.point1.set_y(ParseNumberWithError());
      segment.target_point.set_x(ParseNumberWithError());
      segment.target_point.set_y(ParseNumberWithError());
      break;
    case kPathSegArcRel:
    case kPathSegArcAbs:
      segment.SetArcRadiusX(ParseNumberWithError());
      segment.SetArcRadiusY(ParseNumberWithError());
      segment.SetArcAngle(ParseNumberWithError());
      segment.arc_large = ParseArcFlagWithError();
      segment.arc_sweep = ParseArcFlagWithError();
      segment.target_point.set_x(ParseNumberWithError());
      segment.target_point.set_y(ParseNumberWithError());
      break;
    case kPathSegUnknown:
      NOTREACHED();
  }

  if (error_.Status() != SVGParseStatus::kNoError) [[unlikely]] {
    segment.command = kPathSegUnknown;
  }
  return segment;
}

}  // namespace blink
