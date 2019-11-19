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

#include "third_party/blink/renderer/core/svg/svg_animate_element.h"
#include "third_party/blink/renderer/core/svg/svg_path_blender.h"
#include "third_party/blink/renderer/core/svg/svg_path_byte_stream.h"
#include "third_party/blink/renderer/core/svg/svg_path_byte_stream_builder.h"
#include "third_party/blink/renderer/core/svg/svg_path_byte_stream_source.h"
#include "third_party/blink/renderer/core/svg/svg_path_utilities.h"
#include "third_party/blink/renderer/platform/graphics/path.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {

using cssvalue::CSSPathValue;

namespace {

std::unique_ptr<SVGPathByteStream> BlendPathByteStreams(
    const SVGPathByteStream& from_stream,
    const SVGPathByteStream& to_stream,
    float progress) {
  std::unique_ptr<SVGPathByteStream> result_stream =
      std::make_unique<SVGPathByteStream>();
  SVGPathByteStreamBuilder builder(*result_stream);
  SVGPathByteStreamSource from_source(from_stream);
  SVGPathByteStreamSource to_source(to_stream);
  SVGPathBlender blender(&from_source, &to_source, &builder);
  blender.BlendAnimatedPath(progress);
  return result_stream;
}

std::unique_ptr<SVGPathByteStream> AddPathByteStreams(
    const SVGPathByteStream& from_stream,
    const SVGPathByteStream& by_stream,
    unsigned repeat_count = 1) {
  std::unique_ptr<SVGPathByteStream> result_stream =
      std::make_unique<SVGPathByteStream>();
  SVGPathByteStreamBuilder builder(*result_stream);
  SVGPathByteStreamSource from_source(from_stream);
  SVGPathByteStreamSource by_source(by_stream);
  SVGPathBlender blender(&from_source, &by_source, &builder);
  blender.AddAnimatedPath(repeat_count);
  return result_stream;
}

std::unique_ptr<SVGPathByteStream> ConditionallyAddPathByteStreams(
    std::unique_ptr<SVGPathByteStream> from_stream,
    const SVGPathByteStream& by_stream,
    unsigned repeat_count = 1) {
  if (from_stream->IsEmpty() || by_stream.IsEmpty())
    return from_stream;
  return AddPathByteStreams(*from_stream, by_stream, repeat_count);
}

}  // namespace

SVGPath::SVGPath() : path_value_(CSSPathValue::EmptyPathValue()) {}

SVGPath::SVGPath(CSSPathValue* path_value) : path_value_(path_value) {
  DCHECK(path_value_);
}

SVGPath::~SVGPath() = default;

String SVGPath::ValueAsString() const {
  return BuildStringFromByteStream(ByteStream(), kNoTransformation);
}

SVGPath* SVGPath::Clone() const {
  return MakeGarbageCollected<SVGPath>(path_value_);
}

SVGParsingError SVGPath::SetValueAsString(const String& string) {
  std::unique_ptr<SVGPathByteStream> byte_stream =
      std::make_unique<SVGPathByteStream>();
  SVGParsingError parse_status =
      BuildByteStreamFromString(string, *byte_stream);
  path_value_ = MakeGarbageCollected<CSSPathValue>(std::move(byte_stream));
  return parse_status;
}

SVGPropertyBase* SVGPath::CloneForAnimation(const String& value) const {
  std::unique_ptr<SVGPathByteStream> byte_stream =
      std::make_unique<SVGPathByteStream>();
  BuildByteStreamFromString(value, *byte_stream);
  return MakeGarbageCollected<SVGPath>(
      MakeGarbageCollected<CSSPathValue>(std::move(byte_stream)));
}

void SVGPath::Add(SVGPropertyBase* other, SVGElement*) {
  const SVGPathByteStream& other_path_byte_stream =
      ToSVGPath(other)->ByteStream();
  if (ByteStream().size() != other_path_byte_stream.size() ||
      ByteStream().IsEmpty() || other_path_byte_stream.IsEmpty())
    return;

  path_value_ = MakeGarbageCollected<CSSPathValue>(
      AddPathByteStreams(ByteStream(), other_path_byte_stream));
}

void SVGPath::CalculateAnimatedValue(
    const SVGAnimateElement& animation_element,
    float percentage,
    unsigned repeat_count,
    SVGPropertyBase* from_value,
    SVGPropertyBase* to_value,
    SVGPropertyBase* to_at_end_of_duration_value,
    SVGElement*) {
  bool is_to_animation = animation_element.GetAnimationMode() == kToAnimation;

  const SVGPath& to = ToSVGPath(*to_value);
  const SVGPathByteStream& to_stream = to.ByteStream();

  // If no 'to' value is given, nothing to animate.
  if (!to_stream.size())
    return;

  const SVGPath& from = ToSVGPath(*from_value);
  const SVGPathByteStream* from_stream = &from.ByteStream();

  std::unique_ptr<SVGPathByteStream> copy;
  if (is_to_animation) {
    copy = ByteStream().Clone();
    from_stream = copy.get();
  }

  // If the 'from' value is given and it's length doesn't match the 'to' value
  // list length, fallback to a discrete animation.
  if (from_stream->size() != to_stream.size() && from_stream->size()) {
    if (percentage < 0.5) {
      if (!is_to_animation) {
        path_value_ = from.PathValue();
        return;
      }
    } else {
      path_value_ = to.PathValue();
      return;
    }
  }

  std::unique_ptr<SVGPathByteStream> new_stream =
      BlendPathByteStreams(*from_stream, to_stream, percentage);

  if (!is_to_animation) {
    // Handle additive='sum'.
    if (animation_element.IsAdditive()) {
      new_stream =
          ConditionallyAddPathByteStreams(std::move(new_stream), ByteStream());
    }

    // Handle accumulate='sum'.
    if (repeat_count && animation_element.IsAccumulated()) {
      new_stream = ConditionallyAddPathByteStreams(
          std::move(new_stream),
          ToSVGPath(to_at_end_of_duration_value)->ByteStream(), repeat_count);
    }
  }
  path_value_ = MakeGarbageCollected<CSSPathValue>(std::move(new_stream));
}

float SVGPath::CalculateDistance(SVGPropertyBase* to, SVGElement*) {
  // FIXME: Support paced animations.
  return -1;
}

void SVGPath::Trace(blink::Visitor* visitor) {
  visitor->Trace(path_value_);
  SVGPropertyBase::Trace(visitor);
}

}  // namespace blink
