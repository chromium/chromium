// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_URL_PATTERN_MOJOM_TRAITS_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_URL_PATTERN_MOJOM_TRAITS_H_

#include "mojo/public/cpp/bindings/struct_traits.h"
#include "third_party/blink/public/common/common_export.h"
#include "third_party/blink/public/common/url_pattern.h"
#include "third_party/blink/public/mojom/url_pattern.mojom.h"
#include "third_party/liburlpattern/pattern.h"

namespace mojo {
namespace internal {

inline base::StringPiece TruncateString(const std::string& string) {
  return base::StringPiece(string).substr(0, 4 * 1024);
}

}  // namespace internal

template <>
struct BLINK_COMMON_EXPORT
    EnumTraits<blink::mojom::Modifier, ::liburlpattern::Modifier> {
  static blink::mojom::Modifier ToMojom(
      const ::liburlpattern::Modifier& modifier) {
    switch (modifier) {
      case liburlpattern::Modifier::kZeroOrMore:
        return blink::mojom::Modifier::kZeroOrMore;
      case liburlpattern::Modifier::kOptional:
        return blink::mojom::Modifier::kOptional;
      case liburlpattern::Modifier::kOneOrMore:
        return blink::mojom::Modifier::kOneOrMore;
      case liburlpattern::Modifier::kNone:
        return blink::mojom::Modifier::kNone;
    }
  }

  static bool FromMojom(blink::mojom::Modifier data,
                        ::liburlpattern::Modifier* out);
};

template <>
struct BLINK_COMMON_EXPORT
    StructTraits<blink::mojom::FixedPatternDataView, ::liburlpattern::Part> {
  static base::StringPiece value(const ::liburlpattern::Part& part) {
    return internal::TruncateString(part.value);
  }

  static bool Read(blink::mojom::FixedPatternDataView data,
                   ::liburlpattern::Part* out);
};

template <>
struct BLINK_COMMON_EXPORT
    StructTraits<blink::mojom::WildcardPatternDataView, ::liburlpattern::Part> {
  static base::StringPiece name(const ::liburlpattern::Part& part) {
    return internal::TruncateString(part.name);
  }
  static base::StringPiece prefix(const ::liburlpattern::Part& part) {
    return internal::TruncateString(part.prefix);
  }
  static base::StringPiece value(const ::liburlpattern::Part& part) {
    return internal::TruncateString(part.value);
  }
  static base::StringPiece suffix(const ::liburlpattern::Part& part) {
    return internal::TruncateString(part.suffix);
  }

  static bool Read(blink::mojom::WildcardPatternDataView data,
                   ::liburlpattern::Part* out);
};

template <>
struct BLINK_COMMON_EXPORT
    UnionTraits<blink::mojom::PatternTemplateDataView, ::liburlpattern::Part> {
  static blink::mojom::PatternTemplateDataView::Tag GetTag(
      const ::liburlpattern::Part& value);

  static const ::liburlpattern::Part& fixed(
      const ::liburlpattern::Part& value) {
    return value;
  }

  static const ::liburlpattern::Part& full_wildcard(
      const ::liburlpattern::Part& value) {
    return value;
  }

  static const ::liburlpattern::Part& segment_wildcard(
      const ::liburlpattern::Part& value) {
    return value;
  }

  static bool Read(blink::mojom::PatternTemplateDataView data,
                   ::liburlpattern::Part* out);
};

template <>
struct BLINK_COMMON_EXPORT
    StructTraits<blink::mojom::UrlPatternPartDataView, ::liburlpattern::Part> {
  static const liburlpattern::Part& pattern(
      const ::liburlpattern::Part& pattern) {
    return pattern;
  }

  static liburlpattern::Modifier modifier(
      const ::liburlpattern::Part& pattern) {
    return pattern.modifier;
  }

  static bool Read(blink::mojom::UrlPatternPartDataView data,
                   ::liburlpattern::Part* out);
};

template <>
struct BLINK_COMMON_EXPORT
    StructTraits<blink::mojom::UrlPatternDataView, ::blink::UrlPattern> {
  static const std::vector<liburlpattern::Part>& pathname(
      const ::blink::UrlPattern& pattern) {
    return pattern.pathname;
  }

  static bool Read(blink::mojom::UrlPatternDataView data,
                   ::blink::UrlPattern* out);
};

}  // namespace mojo

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_URL_PATTERN_MOJOM_TRAITS_H_
