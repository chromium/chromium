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

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/core/svg/svg_preserve_aspect_ratio.h"

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
SVGParsingError SVGPreserveAspectRatio::ParseInternal(const CharType*& ptr,
                                                      const CharType* end,
                                                      bool validate) {
  SVGPreserveAspectRatioType align = kSvgPreserveaspectratioXmidymid;
  SVGMeetOrSliceType meet_or_slice = kSvgMeetorsliceMeet;

  SetAlign(align);
  SetMeetOrSlice(meet_or_slice);

  const CharType* start = ptr;
  if (!SkipOptionalSVGSpaces(ptr, end))
    return SVGParsingError(SVGParseStatus::kExpectedEnumeration, ptr - start);

  if (*ptr == 'n') {
    if (!SkipToken(ptr, end, "none"))
      return SVGParsingError(SVGParseStatus::kExpectedEnumeration, ptr - start);
    align = kSvgPreserveaspectratioNone;
    SkipOptionalSVGSpaces(ptr, end);
  } else if (*ptr == 'x') {
    if ((end - ptr) < 8)
      return SVGParsingError(SVGParseStatus::kExpectedEnumeration, ptr - start);
    if (ptr[1] != 'M' || ptr[4] != 'Y' || ptr[5] != 'M')
      return SVGParsingError(SVGParseStatus::kExpectedEnumeration, ptr - start);
    if (ptr[2] == 'i') {
      if (ptr[3] == 'n') {
        if (ptr[6] == 'i') {
          if (ptr[7] == 'n')
            align = kSvgPreserveaspectratioXminymin;
          else if (ptr[7] == 'd')
            align = kSvgPreserveaspectratioXminymid;
          else
            return SVGParsingError(SVGParseStatus::kExpectedEnumeration,
                                   ptr - start);
        } else if (ptr[6] == 'a' && ptr[7] == 'x') {
          align = kSvgPreserveaspectratioXminymax;
        } else {
          return SVGParsingError(SVGParseStatus::kExpectedEnumeration,
                                 ptr - start);
        }
      } else if (ptr[3] == 'd') {
        if (ptr[6] == 'i') {
          if (ptr[7] == 'n')
            align = kSvgPreserveaspectratioXmidymin;
          else if (ptr[7] == 'd')
            align = kSvgPreserveaspectratioXmidymid;
          else
            return SVGParsingError(SVGParseStatus::kExpectedEnumeration,
                                   ptr - start);
        } else if (ptr[6] == 'a' && ptr[7] == 'x') {
          align = kSvgPreserveaspectratioXmidymax;
        } else {
          return SVGParsingError(SVGParseStatus::kExpectedEnumeration,
                                 ptr - start);
        }
      } else {
        return SVGParsingError(SVGParseStatus::kExpectedEnumeration,
                               ptr - start);
      }
    } else if (ptr[2] == 'a' && ptr[3] == 'x') {
      if (ptr[6] == 'i') {
        if (ptr[7] == 'n')
          align = kSvgPreserveaspectratioXmaxymin;
        else if (ptr[7] == 'd')
          align = kSvgPreserveaspectratioXmaxymid;
        else
          return SVGParsingError(SVGParseStatus::kExpectedEnumeration,
                                 ptr - start);
      } else if (ptr[6] == 'a' && ptr[7] == 'x') {
        align = kSvgPreserveaspectratioXmaxymax;
      } else {
        return SVGParsingError(SVGParseStatus::kExpectedEnumeration,
                               ptr - start);
      }
    } else {
      return SVGParsingError(SVGParseStatus::kExpectedEnumeration, ptr - start);
    }
    ptr += 8;
    SkipOptionalSVGSpaces(ptr, end);
  } else {
    return SVGParsingError(SVGParseStatus::kExpectedEnumeration, ptr - start);
  }

  if (ptr < end) {
    if (*ptr == 'm') {
      if (!SkipToken(ptr, end, "meet"))
        return SVGParsingError(SVGParseStatus::kExpectedEnumeration,
                               ptr - start);
      SkipOptionalSVGSpaces(ptr, end);
    } else if (*ptr == 's') {
      if (!SkipToken(ptr, end, "slice"))
        return SVGParsingError(SVGParseStatus::kExpectedEnumeration,
                               ptr - start);
      SkipOptionalSVGSpaces(ptr, end);
      if (align != kSvgPreserveaspectratioNone)
        meet_or_slice = kSvgMeetorsliceSlice;
    }
  }

  if (end != ptr && validate)
    return SVGParsingError(SVGParseStatus::kTrailingGarbage, ptr - start);

  SetAlign(align);
  SetMeetOrSlice(meet_or_slice);

  return SVGParseStatus::kNoError;
}

SVGParsingError SVGPreserveAspectRatio::SetValueAsString(const String& string) {
  SetDefault();

  if (string.empty())
    return SVGParseStatus::kNoError;

  return WTF::VisitCharacters(string, [&](auto chars) {
    const auto* start = chars.data();
    return ParseInternal(start, start + chars.size(), true);
  });
}

bool SVGPreserveAspectRatio::Parse(const LChar*& ptr,
                                   const LChar* end,
                                   bool validate) {
  return ParseInternal(ptr, end, validate) == SVGParseStatus::kNoError;
}

bool SVGPreserveAspectRatio::Parse(const UChar*& ptr,
                                   const UChar* end,
                                   bool validate) {
  return ParseInternal(ptr, end, validate) == SVGParseStatus::kNoError;
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
  NOTREACHED_IN_MIGRATION();
}

void SVGPreserveAspectRatio::CalculateAnimatedValue(
    const SMILAnimationEffectParameters&,
    float percentage,
    unsigned repeat_count,
    const SVGPropertyBase* from_value,
    const SVGPropertyBase* to_value,
    const SVGPropertyBase*,
    const SVGElement*) {
  NOTREACHED_IN_MIGRATION();
}

float SVGPreserveAspectRatio::CalculateDistance(
    const SVGPropertyBase* to_value,
    const SVGElement* context_element) const {
  // No paced animations for SVGPreserveAspectRatio.
  return -1;
}

}  // namespace blink
