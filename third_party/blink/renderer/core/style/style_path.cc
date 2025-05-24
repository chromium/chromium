// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/style/style_path.h"

#include <limits>
#include <memory>
#include <utility>

#include "third_party/blink/renderer/core/css/css_path_value.h"
#include "third_party/blink/renderer/core/svg/svg_path_utilities.h"
#include "third_party/blink/renderer/platform/geometry/path_builder.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/transforms/affine_transform.h"

namespace blink {

const StylePath* StylePath::EmptyPath() {
  DEFINE_STATIC_LOCAL(Persistent<StylePath>, empty_path,
                      (MakeGarbageCollected<StylePath>(SVGPathByteStream())));
  return empty_path.Get();
}

const Path& StylePath::GetPath() const {
  if (!path_) {
    path_ = BuildPathFromByteStream(byte_stream_, wind_rule_);
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

Path StylePath::GetPath(const gfx::RectF& offset_rect,
                        float zoom,
                        float path_scale) const {
  const Path& path = GetPath();
  const AffineTransform transform =
      AffineTransform::Translation(offset_rect.x(), offset_rect.y())
          .Scale(zoom * path_scale);

  return transform.IsIdentity()
             ? path
             : PathBuilder(path).Transform(transform).Finalize();
}

}  // namespace blink
