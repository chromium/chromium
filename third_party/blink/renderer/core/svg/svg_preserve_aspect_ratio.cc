/*
 * Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2010 Dirk Schulze <krit@webkit.org>
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

#include "third_party/blink/renderer/core/svg/svg_preserve_aspect_ratio.h"

#include "base/containers/span.h"
#include "base/notreached.h"
#include "third_party/blink/renderer/core/svg/svg_parser_utilities.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/transforms/affine_transform.h"
#include "third_party/blink/renderer/platform/wtf/text/character_visitor.h"
#include "third_party/blink/renderer/platform/wtf/text/parsing_utilities.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "ui/gfx/geometry/rect_f.h"

namespace blink {

SVGPreserveAspectRatio::SVGPreserveAspectRatio() {
  SetDefault();
}

void SVGPreserveAspectRatio::SetDefault() {
  align_ = kSvgPreserveaspectratioXmidymid;
  meet_or_slice_ = kSvgMeetorsliceMeet;
}

SVGPreserveAspectRatio* SVGPreserveAspectRatio::Clone() const {
  auto* preserve_aspect_ratio = MakeGarbageCollected<SVGPreserveAspectRatio>();

  preserve_aspect_ratio->align_ = align_;
  preserve_aspect_ratio->meet_or_slice_ = meet_or_slice_;

  return preserve_aspect_ratio;
}

template <typename CharType>
SVGParsingError SVGPreserveAspectRatio::ParseInternal(
    base::span<CharType>& span_inout,
    bool validate) {
  SVGPreserveAspectRatioType align = kSvgPreserveaspectratioXmidymid;
  SVGMeetOrSliceType meet_or_slice = kSvgMeetorsliceMeet;

  SetAlign(align);
  SetMeetOrSlice(meet_or_slice);

  auto* start = span_inout.data();

  if (!SkipOptionalSVGSpaces(span_inout)) {
    return SVGParsingError(SVGParseStatus::kExpectedEnumeration,
                           span_inout.data() - start);
  }

  if (span_inout[0] == 'n') {
    if (!SkipToken(span_inout, "none")) {
      return SVGParsingError(SVGParseStatus::kExpectedEnumeration,
                             span_inout.data() - start);
    }
    align = kSvgPreserveaspectratioNone;
    SkipOptionalSVGSpaces(span_inout);
  } else if (span_inout[0] == 'x') {
    if (span_inout.size() < 8) {
      return SVGParsingError(SVGParseStatus::kExpectedEnumeration,
                             span_inout.data() - start);
    }
    if (span_inout[1] != 'M' || span_inout[4] != 'Y' || span_inout[5] != 'M') {
      return SVGParsingError(SVGParseStatus::kExpectedEnumeration,
                             span_inout.data() - start);
    }
    if (span_inout[2] == 'i') {
      if (span_inout[3] == 'n') {
        if (span_inout[6] == 'i') {
          if (span_inout[7] == 'n') {
            align = kSvgPreserveaspectratioXminymin;
          } else if (span_inout[7] == 'd') {
            align = kSvgPreserveaspectratioXminymid;
          } else {
            return SVGParsingError(SVGParseStatus::kExpectedEnumeration,
                                   span_inout.data() - start);
          }
        } else if (span_inout[6] == 'a' && span_inout[7] == 'x') {
          align = kSvgPreserveaspectratioXminymax;
        } else {
          return SVGParsingError(SVGParseStatus::kExpectedEnumeration,
                                 span_inout.data() - start);
        }
      } else if (span_inout[3] == 'd') {
        if (span_inout[6] == 'i') {
          if (span_inout[7] == 'n') {
            align = kSvgPreserveaspectratioXmidymin;
          } else if (span_inout[7] == 'd') {
            align = kSvgPreserveaspectratioXmidymid;
          } else {
            return SVGParsingError(SVGParseStatus::kExpectedEnumeration,
                                   span_inout.data() - start);
          }
        } else if (span_inout[6] == 'a' && span_inout[7] == 'x') {
          align = kSvgPreserveaspectratioXmidymax;
        } else {
          return SVGParsingError(SVGParseStatus::kExpectedEnumeration,
                                 span_inout.data() - start);
        }
      } else {
        return SVGParsingError(SVGParseStatus::kExpectedEnumeration,
                               span_inout.data() - start);
      }
    } else if (span_inout[2] == 'a' && span_inout[3] == 'x') {
      if (span_inout[6] == 'i') {
        if (span_inout[7] == 'n') {
          align = kSvgPreserveaspectratioXmaxymin;
        } else if (span_inout[7] == 'd') {
          align = kSvgPreserveaspectratioXmaxymid;
        } else {
          return SVGParsingError(SVGParseStatus::kExpectedEnumeration,
                                 span_inout.data() - start);
        }
      } else if (span_inout[6] == 'a' && span_inout[7] == 'x') {
        align = kSvgPreserveaspectratioXmaxymax;
      } else {
        return SVGParsingError(SVGParseStatus::kExpectedEnumeration,
                               span_inout.data() - start);
      }
    } else {
      return SVGParsingError(SVGParseStatus::kExpectedEnumeration,
                             span_inout.data() - start);
    }
    span_inout = span_inout.subspan(8ul);
    SkipOptionalSVGSpaces(span_inout);
  } else {
    return SVGParsingError(SVGParseStatus::kExpectedEnumeration,
                           span_inout.data() - start);
  }

  if (!span_inout.empty()) {
    if (span_inout[0] == 'm') {
      if (!SkipToken(span_inout, "meet")) {
        return SVGParsingError(SVGParseStatus::kExpectedEnumeration,
                               span_inout.data() - start);
      }
      SkipOptionalSVGSpaces(span_inout);
    } else if (span_inout[0] == 's') {
      if (!SkipToken(span_inout, "slice")) {
        return SVGParsingError(SVGParseStatus::kExpectedEnumeration,
                               span_inout.data() - start);
      }
      SkipOptionalSVGSpaces(span_inout);
      if (align != kSvgPreserveaspectratioNone)
        meet_or_slice = kSvgMeetorsliceSlice;
    }
  }

  if (!span_inout.empty() && validate) {
    return SVGParsingError(SVGParseStatus::kTrailingGarbage,
                           span_inout.data() - start);
  }

  SetAlign(align);
  SetMeetOrSlice(meet_or_slice);

  return SVGParseStatus::kNoError;
}

SVGParsingError SVGPreserveAspectRatio::SetValueAsString(const String& string) {
  SetDefault();

  if (string.empty())
    return SVGParseStatus::kNoError;

  return VisitCharacters(
      string, [&](auto chars) { return ParseInternal(chars, true); });
}

bool SVGPreserveAspectRatio::Parse(base::span<const UChar>& span,
                                   bool validate) {
  return ParseInternal(span, validate) == SVGParseStatus::kNoError;
}

bool SVGPreserveAspectRatio::Parse(base::span<const LChar>& span,
                                   bool validate) {
  return ParseInternal(span, validate) == SVGParseStatus::kNoError;
}

void SVGPreserveAspectRatio::TransformRect(gfx::RectF& dest_rect,
                                           gfx::RectF& src_rect) const {
  if (align_ == kSvgPreserveaspectratioNone)
    return;

  gfx::SizeF image_size = src_rect.size();
  float orig_dest_width = dest_rect.width();
  float orig_dest_height = dest_rect.height();
  switch (meet_or_slice_) {
    case SVGPreserveAspectRatio::kSvgMeetorsliceUnknown:
      break;
    case SVGPreserveAspectRatio::kSvgMeetorsliceMeet: {
      float width_to_height_multiplier = src_rect.height() / src_rect.width();
      if (orig_dest_height > orig_dest_width * width_to_height_multiplier) {
        dest_rect.set_height(orig_dest_width * width_to_height_multiplier);
        switch (align_) {
          case SVGPreserveAspectRatio::kSvgPreserveaspectratioXminymid:
          case SVGPreserveAspectRatio::kSvgPreserveaspectratioXmidymid:
          case SVGPreserveAspectRatio::kSvgPreserveaspectratioXmaxymid:
            dest_rect.set_y(dest_rect.y() + orig_dest_height / 2 -
                            dest_rect.height() / 2);
            break;
          case SVGPreserveAspectRatio::kSvgPreserveaspectratioXminymax:
          case SVGPreserveAspectRatio::kSvgPreserveaspectratioXmidymax:
          case SVGPreserveAspectRatio::kSvgPreserveaspectratioXmaxymax:
            dest_rect.set_y(dest_rect.y() + orig_dest_height -
                            dest_rect.height());
            break;
          default:
            break;
        }
      }
      if (orig_dest_width > orig_dest_height / width_to_height_multiplier) {
        dest_rect.set_width(orig_dest_height / width_to_height_multiplier);
        switch (align_) {
          case SVGPreserveAspectRatio::kSvgPreserveaspectratioXmidymin:
          case SVGPreserveAspectRatio::kSvgPreserveaspectratioXmidymid:
          case SVGPreserveAspectRatio::kSvgPreserveaspectratioXmidymax:
            dest_rect.set_x(dest_rect.x() + orig_dest_width / 2 -
                            dest_rect.width() / 2);
            break;
          case SVGPreserveAspectRatio::kSvgPreserveaspectratioXmaxymin:
          case SVGPreserveAspectRatio::kSvgPreserveaspectratioXmaxymid:
          case SVGPreserveAspectRatio::kSvgPreserveaspectratioXmaxymax:
            dest_rect.set_x(dest_rect.x() + orig_dest_width -
                            dest_rect.width());
            break;
          default:
            break;
        }
      }
      break;
    }
    case SVGPreserveAspectRatio::kSvgMeetorsliceSlice: {
      float width_to_height_multiplier = src_rect.height() / src_rect.width();
      // If the destination height is less than the height of the image we'll be
      // drawing.
      if (orig_dest_height < orig_dest_width * width_to_height_multiplier) {
        float dest_to_src_multiplier = src_rect.width() / dest_rect.width();
        src_rect.set_height(dest_rect.height() * dest_to_src_multiplier);
        switch (align_) {
          case SVGPreserveAspectRatio::kSvgPreserveaspectratioXminymid:
          case SVGPreserveAspectRatio::kSvgPreserveaspectratioXmidymid:
          case SVGPreserveAspectRatio::kSvgPreserveaspectratioXmaxymid:
            src_rect.set_y(src_rect.y() + image_size.height() / 2 -
                           src_rect.height() / 2);
            break;
          case SVGPreserveAspectRatio::kSvgPreserveaspectratioXminymax:
          case SVGPreserveAspectRatio::kSvgPreserveaspectratioXmidymax:
          case SVGPreserveAspectRatio::kSvgPreserveaspectratioXmaxymax:
            src_rect.set_y(src_rect.y() + image_size.height() -
                           src_rect.height());
            break;
          default:
            break;
        }
      }
      // If the destination width is less than the width of the image we'll be
      // drawing.
      if (orig_dest_width < orig_dest_height / width_to_height_multiplier) {
        float dest_to_src_multiplier = src_rect.height() / dest_rect.height();
        src_rect.set_width(dest_rect.width() * dest_to_src_multiplier);
        switch (align_) {
          case SVGPreserveAspectRatio::kSvgPreserveaspectratioXmidymin:
          case SVGPreserveAspectRatio::kSvgPreserveaspectratioXmidymid:
          case SVGPreserveAspectRatio::kSvgPreserveaspectratioXmidymax:
            src_rect.set_x(src_rect.x() + image_size.width() / 2 -
                           src_rect.width() / 2);
            break;
          case SVGPreserveAspectRatio::kSvgPreserveaspectratioXmaxymin:
          case SVGPreserveAspectRatio::kSvgPreserveaspectratioXmaxymid:
          case SVGPreserveAspectRatio::kSvgPreserveaspectratioXmaxymax:
            src_rect.set_x(src_rect.x() + image_size.width() -
                           src_rect.width());
            break;
          default:
            break;
        }
      }
      break;
    }
  }
}

AffineTransform SVGPreserveAspectRatio::ComputeTransform(
    const gfx::RectF& view_box,
    const gfx::SizeF& viewport_size) const {
  DCHECK(!view_box.IsEmpty());
  DCHECK(!viewport_size.IsEmpty());
  DCHECK_NE(align_, kSvgPreserveaspectratioUnknown);

  double extended_logical_x = view_box.x();
  double extended_logical_y = view_box.y();
  double extended_logical_width = view_box.width();
  double extended_logical_height = view_box.height();
  double extended_physical_width = viewport_size.width();
  double extended_physical_height = viewport_size.height();

  AffineTransform transform;
  if (align_ == kSvgPreserveaspectratioNone) {
    transform.ScaleNonUniform(
        extended_physical_width / extended_logical_width,
        extended_physical_height / extended_logical_height);
    transform.Translate(-extended_logical_x, -extended_logical_y);
    return transform;
  }

  double logical_ratio = extended_logical_width / extended_logical_height;
  double physical_ratio = extended_physical_width / extended_physical_height;
  if ((logical_ratio < physical_ratio &&
       (meet_or_slice_ == kSvgMeetorsliceMeet)) ||
      (logical_ratio >= physical_ratio &&
       (meet_or_slice_ == kSvgMeetorsliceSlice))) {
    transform.ScaleNonUniform(
        extended_physical_height / extended_logical_height,
        extended_physical_height / extended_logical_height);

    if (align_ == kSvgPreserveaspectratioXminymin ||
        align_ == kSvgPreserveaspectratioXminymid ||
        align_ == kSvgPreserveaspectratioXminymax)
      transform.Translate(-extended_logical_x, -extended_logical_y);
    else if (align_ == kSvgPreserveaspectratioXmidymin ||
             align_ == kSvgPreserveaspectratioXmidymid ||
             align_ == kSvgPreserveaspectratioXmidymax)
      transform.Translate(-extended_logical_x - (extended_logical_width -
                                                 extended_physical_width *
                                                     extended_logical_height /
                                                     extended_physical_height) /
                                                    2,
                          -extended_logical_y);
    else
      transform.Translate(-extended_logical_x - (extended_logical_width -
                                                 extended_physical_width *
                                                     extended_logical_height /
                                                     extended_physical_height),
                          -extended_logical_y);

    return transform;
  }

  transform.ScaleNonUniform(extended_physical_width / extended_logical_width,
                            extended_physical_width / extended_logical_width);

  if (align_ == kSvgPreserveaspectratioXminymin ||
      align_ == kSvgPreserveaspectratioXmidymin ||
      align_ == kSvgPreserveaspectratioXmaxymin)
    transform.Translate(-extended_logical_x, -extended_logical_y);
  else if (align_ == kSvgPreserveaspectratioXminymid ||
           align_ == kSvgPreserveaspectratioXmidymid ||
           align_ == kSvgPreserveaspectratioXmaxymid)
    transform.Translate(-extended_logical_x,
                        -extended_logical_y -
                            (extended_logical_height -
                             extended_physical_height * extended_logical_width /
                                 extended_physical_width) /
                                2);
  else
    transform.Translate(-extended_logical_x,
                        -extended_logical_y -
                            (extended_logical_height -
                             extended_physical_height * extended_logical_width /
                                 extended_physical_width));

  return transform;
}

String SVGPreserveAspectRatio::ValueAsString() const {
  StringBuilder builder;

  const char* align_string = "";
  switch (align_) {
    case kSvgPreserveaspectratioNone:
      align_string = "none";
      break;
    case kSvgPreserveaspectratioXminymin:
      align_string = "xMinYMin";
      break;
    case kSvgPreserveaspectratioXmidymin:
      align_string = "xMidYMin";
      break;
    case kSvgPreserveaspectratioXmaxymin:
      align_string = "xMaxYMin";
      break;
    case kSvgPreserveaspectratioXminymid:
      align_string = "xMinYMid";
      break;
    case kSvgPreserveaspectratioXmidymid:
      align_string = "xMidYMid";
      break;
    case kSvgPreserveaspectratioXmaxymid:
      align_string = "xMaxYMid";
      break;
    case kSvgPreserveaspectratioXminymax:
      align_string = "xMinYMax";
      break;
    case kSvgPreserveaspectratioXmidymax:
      align_string = "xMidYMax";
      break;
    case kSvgPreserveaspectratioXmaxymax:
      align_string = "xMaxYMax";
      break;
    case kSvgPreserveaspectratioUnknown:
      align_string = "unknown";
      break;
  }
  builder.Append(align_string);

  const char* meet_or_slice_string = "";
  switch (meet_or_slice_) {
    default:
    case kSvgMeetorsliceUnknown:
      break;
    case kSvgMeetorsliceMeet:
      meet_or_slice_string = " meet";
      break;
    case kSvgMeetorsliceSlice:
      meet_or_slice_string = " slice";
      break;
  }
  builder.Append(meet_or_slice_string);
  return builder.ToString();
}

void SVGPreserveAspectRatio::Add(const SVGPropertyBase* other,
                                 const SVGElement*) {
  NOTREACHED();
}

void SVGPreserveAspectRatio::CalculateAnimatedValue(
    const SMILAnimationEffectParameters&,
    float percentage,
    unsigned repeat_count,
    const SVGPropertyBase* from_value,
    const SVGPropertyBase* to_value,
    const SVGPropertyBase*,
    const SVGElement*) {
  NOTREACHED();
}

float SVGPreserveAspectRatio::CalculateDistance(
    const SVGPropertyBase* to_value,
    const SVGElement* context_element) const {
  // No paced animations for SVGPreserveAspectRatio.
  return -1;
}

}  // namespace blink
