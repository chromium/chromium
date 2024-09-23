// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/style/style_path.h"

#include <limits>
#include <memory>
#include <utility>

#include "third_party/blink/renderer/core/css/css_path_value.h"
#include "third_party/blink/renderer/core/svg/svg_path_utilities.h"
#include "third_party/blink/renderer/platform/graphics/path.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/transforms/affine_transform.h"

namespace blink {

StylePath::StylePath(SVGPathByteStream path_byte_stream, WindRule wind_rule)
    : byte_stream_(std::move(path_byte_stream)),
      path_length_(std::numeric_limits<float>::quiet_NaN()),
      wind_rule_(wind_rule) {}

StylePath::~StylePath() = default;

scoped_refptr<StylePath> StylePath::Create(SVGPathByteStream path_byte_stream,
                                           WindRule wind_rule) {
  return base::AdoptRef(new StylePath(std::move(path_byte_stream), wind_rule));
}

const StylePath* StylePath::EmptyPath() {
  DEFINE_STATIC_REF(StylePath, empty_path,
                    StylePath::Create(SVGPathByteStream()));
  return empty_path;
}

const Path& StylePath::GetPath() const {
  if (!path_) {
    path_.emplace();
    BuildPathFromByteStream(byte_stream_, *path_);
    path_->SetWindRule(wind_rule_);
  }
  return *path_;
}

float StylePath::length() const {
  if (std::isnan(path_length_)) {
    path_length_ = GetPath().length();
  }
  return path_length_;
}

bool StylePath::IsClosed() const {
  return GetPath().IsClosed();
}

CSSValue* StylePath::ComputedCSSValue() const {
  return MakeGarbageCollected<cssvalue::CSSPathValue>(
      const_cast<StylePath*>(this), kTransformToAbsolute);
}

bool StylePath::IsEqualAssumingSameType(const BasicShape& o) const {
  const StylePath& other = To<StylePath>(o);
  return wind_rule_ == other.wind_rule_ && byte_stream_ == other.byte_stream_;
}

void StylePath::GetPath(Path& path,
                        const gfx::RectF& offset_rect,
                        float zoom) const {
  path = GetPath();
  path.Transform(AffineTransform::Translation(offset_rect.x(), offset_rect.y())
                     .Scale(zoom));
}

}  // namespace blink
