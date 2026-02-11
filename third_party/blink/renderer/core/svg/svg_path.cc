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
#include <optional>
#include <utility>

#include "third_party/blink/renderer/core/svg/animation/smil_animation_effect_parameters.h"
#include "third_party/blink/renderer/core/svg/svg_path_blender.h"
#include "third_party/blink/renderer/core/svg/svg_path_byte_stream.h"
#include "third_party/blink/renderer/core/svg/svg_path_byte_stream_builder.h"
#include "third_party/blink/renderer/core/svg/svg_path_byte_stream_source.h"
#include "third_party/blink/renderer/core/svg/svg_path_utilities.h"
#include "third_party/blink/renderer/platform/geometry/path.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string_hash.h"

namespace blink {

using cssvalue::CSSPathValue;

namespace {

std::optional<SVGPathByteStream> BlendPathByteStreams(
    const SVGPathByteStream& from_stream,
    const SVGPathByteStream& to_stream,
    float progress) {
  SVGPathByteStreamBuilder builder;
  SVGPathByteStreamSource from_source(from_stream);
  SVGPathByteStreamSource to_source(to_stream);
  SVGPathBlender blender(&from_source, &to_source, &builder);
  if (!blender.BlendAnimatedPath(progress)) {
    return std::nullopt;
  }
  return builder.CopyByteStream();
}

std::optional<SVGPathByteStream> AddPathByteStreams(
    const SVGPathByteStream& from_stream,
    const SVGPathByteStream& by_stream,
    unsigned repeat_count = 1) {
  SVGPathByteStreamBuilder builder;
  SVGPathByteStreamSource from_source(from_stream);
  SVGPathByteStreamSource by_source(by_stream);
  SVGPathBlender blender(&from_source, &by_source, &builder);
  if (!blender.AddAnimatedPath(repeat_count)) {
    return std::nullopt;
  }
  return builder.CopyByteStream();
}

// Entries in this cache are kept with a WeakMember. They'll be removed from
// the map if nothing is referencing them, as such this doesn't grow unbounded
// in size.
struct CSSPathCache final : public GarbageCollected<CSSPathCache> {
  HeapHashMap<String, WeakMember<const CSSPathValue>> map;
  void Trace(Visitor* visitor) const { visitor->Trace(map); }
};

CSSPathCache& GetPathCache() {
  DEFINE_STATIC_LOCAL(Persistent<CSSPathCache>, cache,
                      (MakeGarbageCollected<CSSPathCache>()));
  return *cache;
}

const CSSPathValue* GetFromCache(const String& string) {
  CSSPathCache& cache = GetPathCache();
  auto it = cache.map.find(string);
  return it != cache.map.end() ? it->value.Get() : nullptr;
}

void AddToCache(const String& string, const CSSPathValue* value) {
  GetPathCache().map.insert(string, value);
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
  if (string.empty()) {
    path_value_ = CSSPathValue::EmptyPathValue();
    return SVGParseStatus::kNoError;
  }

  if (const CSSPathValue* cached = GetFromCache(string)) {
    path_value_ = cached;
    return SVGParseStatus::kNoError;
  }

  SVGPathByteStreamBuilder builder;
  SVGParsingError parse_status = BuildByteStreamFromString(string, builder);
  path_value_ = MakeGarbageCollected<CSSPathValue>(builder.CopyByteStream());

  if (parse_status == SVGParseStatus::kNoError) {
    AddToCache(string, path_value_.Get());
  }
  return parse_status;
}

void SVGPath::Add(const SVGPropertyBase* other, const SVGElement*) {
  const auto& other_path_byte_stream = To<SVGPath>(other)->ByteStream();

  if (ByteStream().IsEmpty() || other_path_byte_stream.IsEmpty()) {
    return;
  }

  if (ByteStream().size() != other_path_byte_stream.size()) {
    // Mismatched sizes - signal invalid animation.
    path_value_ = CSSPathValue::EmptyPathValue();
    return;
  }

  auto result = AddPathByteStreams(ByteStream(), other_path_byte_stream);
  if (result) {
    path_value_ = MakeGarbageCollected<CSSPathValue>(std::move(*result));
  } else {
    // Addition failed (e.g., mismatched commands) - signal invalid animation.
    path_value_ = CSSPathValue::EmptyPathValue();
  }
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

  std::optional<SVGPathByteStream> new_stream =
      BlendPathByteStreams(from_stream, to_stream, percentage);

  // If blending failed (e.g., mismatched commands), use discrete animation.
  if (!new_stream) {
    path_value_ = percentage < 0.5 ? from.PathValue() : to.PathValue();
    return;
  }

  // Handle accumulate='sum'.
  if (repeat_count && parameters.is_cumulative) {
    auto accumulated = AddPathByteStreams(
        *new_stream, To<SVGPath>(to_at_end_of_duration_value)->ByteStream(),
        repeat_count);
    if (!accumulated) {
      // If accumulation fails, leave the animated value unchanged.
      return;
    }
    new_stream = std::move(*accumulated);
  }

  // Handle additive='sum'.
  if (parameters.is_additive) {
    auto additive_result = AddPathByteStreams(*new_stream, ByteStream());
    if (!additive_result) {
      // If addition fails, leave the animated value unchanged.
      return;
    }
    new_stream = std::move(*additive_result);
  }

  path_value_ = MakeGarbageCollected<CSSPathValue>(std::move(*new_stream));
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
