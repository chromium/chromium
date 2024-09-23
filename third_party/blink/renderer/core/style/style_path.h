// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_STYLE_PATH_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_STYLE_PATH_H_

#include <optional>

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/core/style/basic_shapes.h"
#include "third_party/blink/renderer/core/svg/svg_path_byte_stream.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

class CSSValue;
class Path;
class SVGPathByteStream;

class StylePath final : public BasicShape {
 public:
  static scoped_refptr<StylePath> Create(SVGPathByteStream,
                                         WindRule wind_rule = RULE_NONZERO);
  ~StylePath() override;

  static const StylePath* EmptyPath();

  const Path& GetPath() const;
  float length() const;
  bool IsClosed() const;

  const SVGPathByteStream& ByteStream() const { return byte_stream_; }

  CSSValue* ComputedCSSValue() const;

  void GetPath(Path&, const gfx::RectF&, float zoom) const override;
  WindRule GetWindRule() const { return wind_rule_; }

  ShapeType GetType() const override { return kStylePathType; }

 protected:
  bool IsEqualAssumingSameType(const BasicShape&) const override;

 private:
  explicit StylePath(SVGPathByteStream, WindRule wind_rule);

  SVGPathByteStream byte_stream_;
  mutable std::optional<Path> path_;
  mutable float path_length_;
  WindRule wind_rule_;
};

template <>
struct DowncastTraits<StylePath> {
  static bool AllowFrom(const BasicShape& value) {
    return value.GetType() == BasicShape::kStylePathType;
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_STYLE_PATH_H_
