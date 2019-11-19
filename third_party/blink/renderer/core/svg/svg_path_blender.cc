/*
 * Copyright (C) Research In Motion Limited 2010, 2011. All rights reserved.
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

#include "third_party/blink/renderer/core/svg/svg_path_blender.h"

#include "third_party/blink/renderer/core/svg/svg_path_byte_stream_source.h"
#include "third_party/blink/renderer/core/svg/svg_path_consumer.h"
#include "third_party/blink/renderer/core/svg/svg_path_data.h"
#include "third_party/blink/renderer/platform/animation/animation_utilities.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

enum FloatBlendMode { kBlendHorizontal, kBlendVertical };

class SVGPathBlender::BlendState {
  STACK_ALLOCATED();

 public:
  BlendState(float progress, unsigned add_types_count = 0)
      : progress_(progress),
        add_types_count_(add_types_count),
        is_in_first_half_of_animation_(progress < 0.5f),
        types_are_equal_(false),
        from_is_absolute_(false) {}

  bool BlendSegments(const PathSegmentData& from_seg,
                     const PathSegmentData& to_seg,
                     PathSegmentData&);

 private:
  float BlendAnimatedDimensonalFloat(float, float, FloatBlendMode);
  FloatPoint BlendAnimatedFloatPointSameCoordinates(const FloatPoint& from,
                                                    const FloatPoint& to);
  FloatPoint BlendAnimatedFloatPoint(const FloatPoint& from,
                                     const FloatPoint& to);
  bool CanBlend(const PathSegmentData& from_seg, const PathSegmentData& to_seg);

  FloatPoint from_sub_path_point_;
  FloatPoint from_current_point_;
  FloatPoint to_sub_path_point_;
  FloatPoint to_current_point_;

  double progress_;
  float add_types_count_;
  bool is_in_first_half_of_animation_;
  // This is per-segment blend state corresponding to the 'from' and 'to'
  // segments currently being blended, and only used within blendSegments().
  bool types_are_equal_;
  bool from_is_absolute_;
};

float SVGPathBlender::BlendState::BlendAnimatedDimensonalFloat(
    float from,
    float to,
    FloatBlendMode blend_mode) {
  if (add_types_count_) {
    DCHECK(types_are_equal_);
    return from + to * add_types_count_;
  }

  if (types_are_equal_)
    return Blend(from, to, progress_);

  float from_value = blend_mode == kBlendHorizontal ? from_current_point_.X()
                                                    : from_current_point_.Y();
  float to_value = blend_mode == kBlendHorizontal ? to_current_point_.X()
                                                  : to_current_point_.Y();

  // Transform toY to the coordinate mode of fromY
  float anim_value =
      Blend(from, from_is_absolute_ ? to + to_value : to - to_value, progress_);

  // If we're in the first half of the animation, we should use the type of the
  // from segment.
  if (is_in_first_half_of_animation_)
    return anim_value;

  // Transform the animated point to the coordinate mode, needed for the current
  // progress.
  float current_value = Blend(from_value, to_value, progress_);
  return !from_is_absolute_ ? anim_value + current_value
                            : anim_value - current_value;
}

FloatPoint SVGPathBlender::BlendState::BlendAnimatedFloatPointSameCoordinates(
    const FloatPoint& from_point,
    const FloatPoint& to_point) {
  if (add_types_count_) {
    FloatPoint repeated_to_point = to_point;
    repeated_to_point.Scale(add_types_count_, add_types_count_);
    return from_point + repeated_to_point;
  }
  return Blend(from_point, to_point, progress_);
}

FloatPoint SVGPathBlender::BlendState::BlendAnimatedFloatPoint(
    const FloatPoint& from_point,
    const FloatPoint& to_point) {
  if (types_are_equal_)
    return BlendAnimatedFloatPointSameCoordinates(from_point, to_point);

  // Transform toPoint to the coordinate mode of fromPoint
  FloatPoint animated_point = to_point;
  if (from_is_absolute_)
    animated_point += to_current_point_;
  else
    animated_point.Move(-to_current_point_.X(), -to_current_point_.Y());

  animated_point = Blend(from_point, animated_point, progress_);

  // If we're in the first half of the animation, we should use the type of the
  // from segment.
  if (is_in_first_half_of_animation_)
    return animated_point;

  // Transform the animated point to the coordinate mode, needed for the current
  // progress.
  FloatPoint current_point =
      Blend(from_current_point_, to_current_point_, progress_);
  if (!from_is_absolute_)
    return animated_point + current_point;

  animated_point.Move(-current_point.X(), -current_point.Y());
  return animated_point;
}

bool SVGPathBlender::BlendState::CanBlend(const PathSegmentData& from_seg,
                                          const PathSegmentData& to_seg) {
  // Update state first because we'll need it if we return true below.
  types_are_equal_ = from_seg.command == to_seg.command;
  from_is_absolute_ = IsAbsolutePathSegType(from_seg.command);

  // If the types are equal, they'll blend regardless of parameters.
  if (types_are_equal_)
    return true;

  // Addition require segments with the same type.
  if (add_types_count_)
    return false;

  // Allow the segments to differ in "relativeness".
  return ToAbsolutePathSegType(from_seg.command) ==
         ToAbsolutePathSegType(to_seg.command);
}

static void UpdateCurrentPoint(FloatPoint& sub_path_point,
                               FloatPoint& current_point,
                               const PathSegmentData& segment) {
  switch (segment.command) {
    case kPathSegMoveToRel:
      current_point += segment.target_point;
      sub_path_point = current_point;
      break;
    case kPathSegLineToRel:
    case kPathSegCurveToCubicRel:
    case kPathSegCurveToQuadraticRel:
    case kPathSegArcRel:
    case kPathSegLineToHorizontalRel:
    case kPathSegLineToVerticalRel:
    case kPathSegCurveToCubicSmoothRel:
    case kPathSegCurveToQuadraticSmoothRel:
      current_point += segment.target_point;
      break;
    case kPathSegMoveToAbs:
      current_point = segment.target_point;
      sub_path_point = current_point;
      break;
    case kPathSegLineToAbs:
    case kPathSegCurveToCubicAbs:
    case kPathSegCurveToQuadraticAbs:
    case kPathSegArcAbs:
    case kPathSegCurveToCubicSmoothAbs:
    case kPathSegCurveToQuadraticSmoothAbs:
      current_point = segment.target_point;
      break;
    case kPathSegLineToHorizontalAbs:
      current_point.SetX(segment.target_point.X());
      break;
    case kPathSegLineToVerticalAbs:
      current_point.SetY(segment.target_point.Y());
      break;
    case kPathSegClosePath:
      current_point = sub_path_point;
      break;
    default:
      NOTREACHED();
  }
}

bool SVGPathBlender::BlendState::BlendSegments(
    const PathSegmentData& from_seg,
    const PathSegmentData& to_seg,
    PathSegmentData& blended_segment) {
  if (!CanBlend(from_seg, to_seg))
    return false;

  blended_segment.command =
      is_in_first_half_of_animation_ ? from_seg.command : to_seg.command;

  switch (to_seg.command) {
    case kPathSegCurveToCubicRel:
    case kPathSegCurveToCubicAbs:
      blended_segment.point1 =
          BlendAnimatedFloatPoint(from_seg.point1, to_seg.point1);
      FALLTHROUGH;
    case kPathSegCurveToCubicSmoothRel:
    case kPathSegCurveToCubicSmoothAbs:
      blended_segment.point2 =
          BlendAnimatedFloatPoint(from_seg.point2, to_seg.point2);
      FALLTHROUGH;
    case kPathSegMoveToRel:
    case kPathSegMoveToAbs:
    case kPathSegLineToRel:
    case kPathSegLineToAbs:
    case kPathSegCurveToQuadraticSmoothRel:
    case kPathSegCurveToQuadraticSmoothAbs:
      blended_segment.target_point =
          BlendAnimatedFloatPoint(from_seg.target_point, to_seg.target_point);
      break;
    case kPathSegLineToHorizontalRel:
    case kPathSegLineToHorizontalAbs:
      blended_segment.target_point.SetX(BlendAnimatedDimensonalFloat(
          from_seg.target_point.X(), to_seg.target_point.X(),
          kBlendHorizontal));
      break;
    case kPathSegLineToVerticalRel:
    case kPathSegLineToVerticalAbs:
      blended_segment.target_point.SetY(BlendAnimatedDimensonalFloat(
          from_seg.target_point.Y(), to_seg.target_point.Y(), kBlendVertical));
      break;
    case kPathSegClosePath:
      break;
    case kPathSegCurveToQuadraticRel:
    case kPathSegCurveToQuadraticAbs:
      blended_segment.target_point =
          BlendAnimatedFloatPoint(from_seg.target_point, to_seg.target_point);
      blended_segment.point1 =
          BlendAnimatedFloatPoint(from_seg.point1, to_seg.point1);
      break;
    case kPathSegArcRel:
    case kPathSegArcAbs:
      blended_segment.target_point =
          BlendAnimatedFloatPoint(from_seg.target_point, to_seg.target_point);
      blended_segment.point1 = BlendAnimatedFloatPointSameCoordinates(
          from_seg.ArcRadii(), to_seg.ArcRadii());
      blended_segment.point2 = BlendAnimatedFloatPointSameCoordinates(
          from_seg.point2, to_seg.point2);
      if (add_types_count_) {
        blended_segment.arc_large = from_seg.arc_large || to_seg.arc_large;
        blended_segment.arc_sweep = from_seg.arc_sweep || to_seg.arc_sweep;
      } else {
        blended_segment.arc_large = is_in_first_half_of_animation_
                                        ? from_seg.arc_large
                                        : to_seg.arc_large;
        blended_segment.arc_sweep = is_in_first_half_of_animation_
                                        ? from_seg.arc_sweep
                                        : to_seg.arc_sweep;
      }
      break;
    default:
      NOTREACHED();
  }

  UpdateCurrentPoint(from_sub_path_point_, from_current_point_, from_seg);
  UpdateCurrentPoint(to_sub_path_point_, to_current_point_, to_seg);

  return true;
}

SVGPathBlender::SVGPathBlender(SVGPathByteStreamSource* from_source,
                               SVGPathByteStreamSource* to_source,
                               SVGPathConsumer* consumer)
    : from_source_(from_source), to_source_(to_source), consumer_(consumer) {
  DCHECK(from_source_);
  DCHECK(to_source_);
  DCHECK(consumer_);
}

bool SVGPathBlender::AddAnimatedPath(unsigned repeat_count) {
  BlendState blend_state(0, repeat_count);
  return BlendAnimatedPath(blend_state);
}

bool SVGPathBlender::BlendAnimatedPath(float progress) {
  BlendState blend_state(progress);
  return BlendAnimatedPath(blend_state);
}

bool SVGPathBlender::BlendAnimatedPath(BlendState& blend_state) {
  bool from_source_is_empty = !from_source_->HasMoreData();
  while (to_source_->HasMoreData()) {
    PathSegmentData to_seg = to_source_->ParseSegment();
    if (to_seg.command == kPathSegUnknown)
      return false;

    PathSegmentData from_seg;
    from_seg.command = to_seg.command;

    if (from_source_->HasMoreData()) {
      from_seg = from_source_->ParseSegment();
      if (from_seg.command == kPathSegUnknown)
        return false;
    }

    PathSegmentData blended_seg;
    if (!blend_state.BlendSegments(from_seg, to_seg, blended_seg))
      return false;

    consumer_->EmitSegment(blended_seg);

    if (from_source_is_empty)
      continue;
    if (from_source_->HasMoreData() != to_source_->HasMoreData())
      return false;
  }
  return true;
}

}  // namespace blink
