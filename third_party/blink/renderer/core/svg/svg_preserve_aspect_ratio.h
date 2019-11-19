/*
 * Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SVG_SVG_PRESERVE_ASPECT_RATIO_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SVG_SVG_PRESERVE_ASPECT_RATIO_H_

#include "third_party/blink/renderer/core/svg/properties/svg_property_helper.h"
#include "third_party/blink/renderer/core/svg/svg_parsing_error.h"

namespace blink {

class AffineTransform;
class FloatRect;
class SVGPreserveAspectRatioTearOff;

class SVGPreserveAspectRatio final
    : public SVGPropertyHelper<SVGPreserveAspectRatio> {
 public:
  enum SVGPreserveAspectRatioType {
    kSvgPreserveaspectratioUnknown = 0,
    kSvgPreserveaspectratioNone = 1,
    kSvgPreserveaspectratioXminymin = 2,
    kSvgPreserveaspectratioXmidymin = 3,
    kSvgPreserveaspectratioXmaxymin = 4,
    kSvgPreserveaspectratioXminymid = 5,
    kSvgPreserveaspectratioXmidymid = 6,
    kSvgPreserveaspectratioXmaxymid = 7,
    kSvgPreserveaspectratioXminymax = 8,
    kSvgPreserveaspectratioXmidymax = 9,
    kSvgPreserveaspectratioXmaxymax = 10
  };

  enum SVGMeetOrSliceType {
    kSvgMeetorsliceUnknown = 0,
    kSvgMeetorsliceMeet = 1,
    kSvgMeetorsliceSlice = 2
  };

  typedef SVGPreserveAspectRatioTearOff TearOffType;

  SVGPreserveAspectRatio();

  virtual SVGPreserveAspectRatio* Clone() const;

  bool operator==(const SVGPreserveAspectRatio&) const;
  bool operator!=(const SVGPreserveAspectRatio& other) const {
    return !operator==(other);
  }

  void SetAlign(SVGPreserveAspectRatioType align) { align_ = align; }
  SVGPreserveAspectRatioType Align() const { return align_; }

  void SetMeetOrSlice(SVGMeetOrSliceType meet_or_slice) {
    meet_or_slice_ = meet_or_slice;
  }
  SVGMeetOrSliceType MeetOrSlice() const { return meet_or_slice_; }

  void TransformRect(FloatRect& dest_rect, FloatRect& src_rect);

  AffineTransform ComputeTransform(float logical_x,
                                   float logical_y,
                                   float logical_width,
                                   float logical_height,
                                   float physical_width,
                                   float physical_height) const;

  String ValueAsString() const override;
  SVGParsingError SetValueAsString(const String&);
  bool Parse(const UChar*& ptr, const UChar* end, bool validate);
  bool Parse(const LChar*& ptr, const LChar* end, bool validate);

  void Add(SVGPropertyBase*, SVGElement*) override;
  void CalculateAnimatedValue(const SVGAnimateElement&,
                              float percentage,
                              unsigned repeat_count,
                              SVGPropertyBase* from,
                              SVGPropertyBase* to,
                              SVGPropertyBase* to_at_end_of_duration_value,
                              SVGElement* context_element) override;
  float CalculateDistance(SVGPropertyBase* to,
                          SVGElement* context_element) override;

  static AnimatedPropertyType ClassType() {
    return kAnimatedPreserveAspectRatio;
  }

  void SetDefault();

 private:
  template <typename CharType>
  SVGParsingError ParseInternal(const CharType*& ptr,
                                const CharType* end,
                                bool validate);

  SVGPreserveAspectRatioType align_;
  SVGMeetOrSliceType meet_or_slice_;
};

DEFINE_SVG_PROPERTY_TYPE_CASTS(SVGPreserveAspectRatio);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SVG_SVG_PRESERVE_ASPECT_RATIO_H_
