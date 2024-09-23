// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/safe_url_pattern_mojom_traits.h"

namespace mojo {

bool EnumTraits<blink::mojom::Modifier, ::liburlpattern::Modifier>::FromMojom(
    blink::mojom::Modifier data,
    ::liburlpattern::Modifier* out) {
  switch (data) {
    case blink::mojom::Modifier::kZeroOrMore:
      *out = liburlpattern::Modifier::kZeroOrMore;
      return true;
    case blink::mojom::Modifier::kOptional:
      *out = liburlpattern::Modifier::kOptional;
      return true;
    case blink::mojom::Modifier::kOneOrMore:
      *out = liburlpattern::Modifier::kOneOrMore;
      return true;
    case blink::mojom::Modifier::kNone:
      *out = liburlpattern::Modifier::kNone;
      return true;
  }
  NOTREACHED_IN_MIGRATION();
  return false;
}

bool StructTraits<blink::mojom::FixedPatternDataView, ::liburlpattern::Part>::
    Read(blink::mojom::FixedPatternDataView data, ::liburlpattern::Part* out) {
  if (!data.ReadValue(&out->value)) {
    return false;
  }

  return true;
}

bool StructTraits<
    blink::mojom::WildcardPatternDataView,
    ::liburlpattern::Part>::Read(blink::mojom::WildcardPatternDataView data,
                                 ::liburlpattern::Part* out) {
  if (!data.ReadName(&out->name)) {
    return false;
  }
  if (!data.ReadPrefix(&out->prefix)) {
    return false;
  }
  if (!data.ReadValue(&out->value)) {
    return false;
  }
  if (!data.ReadSuffix(&out->suffix)) {
    return false;
  }

  return true;
}

blink::mojom::PatternTemplateDataView::Tag
UnionTraits<blink::mojom::PatternTemplateDataView,
            ::liburlpattern::Part>::GetTag(const ::liburlpattern::Part& value) {
  switch (value.type) {
    case liburlpattern::PartType::kFixed:
      return blink::mojom::PatternTemplate::Tag::kFixed;
    case liburlpattern::PartType::kFullWildcard:
      return blink::mojom::PatternTemplate::Tag::kFullWildcard;
    case liburlpattern::PartType::kSegmentWildcard:
      return blink::mojom::PatternTemplate::Tag::kSegmentWildcard;
    case liburlpattern::PartType::kRegex:
      NOTREACHED_IN_MIGRATION();
      return blink::mojom::PatternTemplate::Tag::kFixed;
  }
}

bool UnionTraits<blink::mojom::PatternTemplateDataView, ::liburlpattern::Part>::
    Read(blink::mojom::PatternTemplateDataView data, liburlpattern::Part* out) {
  ::liburlpattern::Part part;
  switch (data.tag()) {
    case blink::mojom::PatternTemplateDataView::Tag::kFixed:
      if (!data.ReadFixed(&part)) {
        return false;
      }
      part.type = liburlpattern::PartType::kFixed;
      *out = part;
      return true;
    case blink::mojom::PatternTemplateDataView::Tag::kFullWildcard:
      if (!data.ReadFullWildcard(&part)) {
        return false;
      }
      part.type = liburlpattern::PartType::kFullWildcard;
      *out = part;
      return true;
    case blink::mojom::PatternTemplateDataView::Tag::kSegmentWildcard:
      if (!data.ReadSegmentWildcard(&part)) {
        return false;
      }
      part.type = liburlpattern::PartType::kSegmentWildcard;
      *out = part;
      return true;
  }
  return false;
}

bool StructTraits<
    blink::mojom::SafeUrlPatternPartDataView,
    ::liburlpattern::Part>::Read(blink::mojom::SafeUrlPatternPartDataView data,
                                 ::liburlpattern::Part* out) {
  liburlpattern::Part part;
  if (!data.ReadPattern(&part)) {
    return false;
  }
  out->name = part.name;
  out->prefix = part.prefix;
  out->value = part.value;
  out->suffix = part.suffix;
  out->type = part.type;

  if (!data.ReadModifier(&out->modifier)) {
    return false;
  }

  return true;
}

bool StructTraits<
    blink::mojom::SafeUrlPatternDataView,
    ::blink::SafeUrlPattern>::Read(blink::mojom::SafeUrlPatternDataView data,
                                   ::blink::SafeUrlPattern* out) {
  if (!data.ReadProtocol(&out->protocol)) {
    return false;
  }

  if (!data.ReadUsername(&out->username)) {
    return false;
  }

  if (!data.ReadPassword(&out->password)) {
    return false;
  }

  if (!data.ReadHostname(&out->hostname)) {
    return false;
  }

  if (!data.ReadPort(&out->port)) {
    return false;
  }

  if (!data.ReadPathname(&out->pathname)) {
    return false;
  }

  if (!data.ReadSearch(&out->search)) {
    return false;
  }

  if (!data.ReadHash(&out->hash)) {
    return false;
  }

  if (!data.ReadOptions(&out->options)) {
    return false;
  }

  return true;
}

bool StructTraits<blink::mojom::SafeUrlPatternOptionsDataView,
                  ::blink::SafeUrlPatternOptions>::
    Read(blink::mojom::SafeUrlPatternOptionsDataView data,
         ::blink::SafeUrlPatternOptions* out) {
  out->ignore_case = data.ignore_case();
  return true;
}

}  // namespace mojo
