// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_STYLE_PATH_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_STYLE_PATH_H_

#include <memory>
#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/core/style/basic_shapes.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

class CSSValue;
class Path;
class SVGPathByteStream;

class StylePath final : public BasicShape {
 public:
  static scoped_refptr<StylePath> Create(std::unique_ptr<SVGPathByteStream>);
  ~StylePath() override;

  static const StylePath* EmptyPath();

  const Path& GetPath() const;
  float length() const;
  bool IsClosed() const;

  const SVGPathByteStream& ByteStream() const { return *byte_stream_; }

  CSSValue* ComputedCSSValue() const;

  void GetPath(Path&, const FloatRect&) override;
  bool operator==(const BasicShape&) const override;

  ShapeType GetType() const override { return kStylePathType; }

 private:
  explicit StylePath(std::unique_ptr<SVGPathByteStream>);

  std::unique_ptr<SVGPathByteStream> byte_stream_;
  mutable std::unique_ptr<Path> path_;
  mutable float path_length_;
};

template <>
struct DowncastTraits<StylePath> {
  static bool AllowFrom(const BasicShape& value) {
    return value.GetType() == BasicShape::kStylePathType;
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_STYLE_PATH_H_
