// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FENCED_FRAME_FENCED_FRAME_INNER_CONFIG_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FENCED_FRAME_FENCED_FRAME_INNER_CONFIG_H_

#include "base/notreached.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_opaqueproperty_unsignedlong.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_opaqueproperty_usvstring.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

// FencedFrameInnerConfig class implements the FencedFrameInnerConfig IDL. It
// specifies the fenced frame's inner properties. It can be returned by shared
// storage's selectURL() and FLEDGE's runAdAuction(), or directly constructed
// for navigation to non-opaque URLs. Please see the link for examples of
// installing FencedFrameInnerConfig in fenced frames.
// https://github.com/WICG/fenced-frame/issues/48#issuecomment-1245809738
class CORE_EXPORT FencedFrameInnerConfig final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  // Note this visibility has different semantics from
  // FencedFrameURLMapping::VisibilityToEmbedder and
  // FencedFrameURLMapping::VisibilityToContent. Here `AttributeVisibility`
  // specifies whether each attribute is transparent to the author, or is null.
  // Whereas the enums in FencedFrameURLMapping specify whether information
  // should be redacted when it is communicated to different entities
  // (renderers).
  enum class AttributeVisibility {
    kTransparent,
    kOpaque,
    kNull,
  };

  explicit FencedFrameInnerConfig(const String& src);
  FencedFrameInnerConfig(const FencedFrameInnerConfig&) = delete;
  FencedFrameInnerConfig& operator=(const FencedFrameInnerConfig&) = delete;

  V8UnionOpaquePropertyOrUSVString* url() const;
  V8UnionOpaquePropertyOrUnsignedLong* width() const;
  V8UnionOpaquePropertyOrUnsignedLong* height() const;

 private:
  enum class Attribute {
    kURL,
    kWidth,
    kHeight,
  };

  AttributeVisibility url_attribute_visibility_ = AttributeVisibility::kNull;
  AttributeVisibility size_attribute_visibility_ = AttributeVisibility::kNull;

  KURL url_;
  uint32_t width_;
  uint32_t height_;

  // Attribute's union type based on its value type.
  template <typename T>
  struct AttributeUnion;

  // Get attribute's visibility.
  template <Attribute attr>
  AttributeVisibility GetAttributeVisibility() const {
    switch (attr) {
      case Attribute::kURL:
        return url_attribute_visibility_;
      case Attribute::kWidth:
      case Attribute::kHeight:
        return size_attribute_visibility_;
    }
    NOTREACHED();
  }

  // Get attribute's value.
  template <Attribute attr>
  auto GetValue() const {
    if constexpr (attr == Attribute::kURL) {
      return url_.GetString();
    } else if constexpr (attr == Attribute::kWidth) {
      return width_;
    } else if constexpr (attr == Attribute::kHeight) {
      return height_;
    }
    NOTREACHED();
  }

  // Get the union based on attribute's `AttributeType`.
  template <Attribute attr>
  auto Get() const ->
      typename AttributeUnion<decltype(GetValue<attr>())>::Type* {
    auto attribute_visibility = GetAttributeVisibility<attr>();
    auto value = GetValue<attr>();

    using UnionType = typename AttributeUnion<decltype(value)>::Type;

    switch (attribute_visibility) {
      case AttributeVisibility::kTransparent:
        return MakeGarbageCollected<UnionType>(value);
      case AttributeVisibility::kOpaque:
        return MakeGarbageCollected<UnionType>(
            V8OpaqueProperty(V8OpaqueProperty::Enum::kOpaque));
      case AttributeVisibility::kNull:
        return nullptr;
    }
    NOTREACHED();
  }
};

template <>
struct FencedFrameInnerConfig::AttributeUnion<String> {
  using Type = V8UnionOpaquePropertyOrUSVString;
};
template <>
struct FencedFrameInnerConfig::AttributeUnion<uint32_t> {
  using Type = V8UnionOpaquePropertyOrUnsignedLong;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FENCED_FRAME_FENCED_FRAME_INNER_CONFIG_H_
