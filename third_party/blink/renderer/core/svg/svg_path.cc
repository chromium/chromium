/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Nikolas Zimmermann
 * <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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

#include "third_party/blink/renderer/core/svg/svg_path.h"

#include <memory>
#include <utility>

#include "third_party/blink/renderer/core/svg/animation/smil_animation_effect_parameters.h"
#include "third_party/blink/renderer/core/svg/svg_path_blender.h"
#include "third_party/blink/renderer/core/svg/svg_path_byte_stream.h"
#include "third_party/blink/renderer/core/svg/svg_path_byte_stream_builder.h"
#include "third_party/blink/renderer/core/svg/svg_path_byte_stream_source.h"
#include "third_party/blink/renderer/core/svg/svg_path_utilities.h"
#include "third_party/blink/renderer/platform/graphics/path.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

using cssvalue::CSSPathValue;

namespace {

SVGPathByteStream BlendPathByteStreams(const SVGPathByteStream& from_stream,
                                       const SVGPathByteStream& to_stream,
                                       float progress) {
  SVGPathByteStreamBuilder builder;
  SVGPathByteStreamSource from_source(from_stream);
  SVGPathByteStreamSource to_source(to_stream);
  SVGPathBlender blender(&from_source, &to_source, &builder);
  blender.BlendAnimatedPath(progress);
  return builder.CopyByteStream();
}

SVGPathByteStream AddPathByteStreams(const SVGPathByteStream& from_stream,
                                     const SVGPathByteStream& by_stream,
                                     unsigned repeat_count = 1) {
  SVGPathByteStreamBuilder builder;
  SVGPathByteStreamSource from_source(from_stream);
  SVGPathByteStreamSource by_source(by_stream);
  SVGPathBlender blender(&from_source, &by_source, &builder);
  blender.AddAnimatedPath(repeat_count);
  return builder.CopyByteStream();
}

SVGPathByteStream ConditionallyAddPathByteStreams(
    SVGPathByteStream from_stream,
    const SVGPathByteStream& by_stream,
    unsigned repeat_count = 1) {
  if (from_stream.IsEmpty() || by_stream.IsEmpty()) {
    return from_stream;
  }
  return AddPathByteStreams(from_stream, by_stream, repeat_count);
}

}  // namespace

SVGPath::SVGPath() : path_value_(CSSPathValue::EmptyPathValue()) {}

SVGPath::SVGPath(const CSSPathValue& path_value) : path_value_(path_value) {}

SVGPath::~SVGPath() = default;

String SVGPath::ValueAsString() const {
  return BuildStringFromByteStream(ByteStream(), kNoTransformation);
}

SVGPath* SVGPath::Clone() const {
  return MakeGarbageCollected<SVGPath>(*path_value_);
}

SVGParsingError SVGPath::SetValueAsString(const String& string) {
  SVGPathByteStreamBuilder builder;
  SVGParsingError parse_status = BuildByteStreamFromString(string, builder);
  path_value_ = MakeGarbageCollected<CSSPathValue>(builder.CopyByteStream());
  return parse_status;
}

SVGPropertyBase* SVGPath::CloneForAnimation(const String& value) const {
  SVGPathByteStreamBuilder builder;
  BuildByteStreamFromString(value, builder);
  return MakeGarbageCollected<SVGPath>(
      *MakeGarbageCollected<CSSPathValue>(builder.CopyByteStream()));
}

void SVGPath::Add(const SVGPropertyBase* other, const SVGElement*) {
  const auto& other_path_byte_stream = To<SVGPath>(other)->ByteStream();
  if (ByteStream().size() != other_path_byte_stream.size() ||
      ByteStream().IsEmpty() || other_path_byte_stream.IsEmpty())
    return;

  path_value_ = MakeGarbageCollected<CSSPathValue>(
      AddPathByteStreams(ByteStream(), other_path_byte_stream));
}

void SVGPath::CalculateAnimatedValue(
    const SMILAnimationEffectParameters& parameters,
    float percentage,
    unsigned repeat_count,
    const SVGPropertyBase* from_value,
    const SVGPropertyBase* to_value,
    const SVGPropertyBase* to_at_end_of_duration_value,
    const SVGElement*) {
  const auto& to = To<SVGPath>(*to_value);
  const SVGPathByteStream& to_stream = to.ByteStream();

  // If no 'to' value is given, nothing to animate.
  if (!to_stream.size())
    return;

  const auto& from = To<SVGPath>(*from_value);
  const SVGPathByteStream& from_stream = from.ByteStream();

  // If the 'from' value is given and it's length doesn't match the 'to' value
  // list length, fallback to a discrete animation.
  if (from_stream.size() != to_stream.size() && from_stream.size()) {
    // If this is a 'to' animation, the "from" value will be the same
    // object as this object, so this will be a no-op but shouldn't
    // clobber the object.
    path_value_ = percentage < 0.5 ? from.PathValue() : to.PathValue();
    return;
  }

  // If this is a 'to' animation, the "from" value will be the same
  // object as this object, so make sure to update the state of this
  // object as the last thing to avoid clobbering the result. As long
  // as all intermediate results are computed into |new_stream| that
  // should be unproblematic.
  SVGPathByteStream new_stream =
      BlendPathByteStreams(from_stream, to_stream, percentage);

  // Handle accumulate='sum'.
  if (repeat_count && parameters.is_cumulative) {
    new_stream = ConditionallyAddPathByteStreams(
        std::move(new_stream),
        To<SVGPath>(to_at_end_of_duration_value)->ByteStream(), repeat_count);
  }

  // Handle additive='sum'.
  if (parameters.is_additive) {
    new_stream =
        ConditionallyAddPathByteStreams(std::move(new_stream), ByteStream());
  }

  path_value_ = MakeGarbageCollected<CSSPathValue>(std::move(new_stream));
}

float SVGPath::CalculateDistance(const SVGPropertyBase* to,
                                 const SVGElement*) const {
  // FIXME: Support paced animations.
  return -1;
}

void SVGPath::Trace(Visitor* visitor) const {
  visitor->Trace(path_value_);
  SVGPropertyBase::Trace(visitor);
}

}  // namespace blink
