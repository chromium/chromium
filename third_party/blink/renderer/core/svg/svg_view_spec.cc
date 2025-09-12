/*
 * Copyright (C) 2007, 2010 Rob Buis <buis@kde.org>
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

#include "third_party/blink/renderer/core/svg/svg_view_spec.h"

#include "base/containers/span.h"
#include "third_party/blink/renderer/core/svg/svg_animated_preserve_aspect_ratio.h"
#include "third_party/blink/renderer/core/svg/svg_animated_rect.h"
#include "third_party/blink/renderer/core/svg/svg_parser_utilities.h"
#include "third_party/blink/renderer/core/svg/svg_preserve_aspect_ratio.h"
#include "third_party/blink/renderer/core/svg/svg_rect.h"
#include "third_party/blink/renderer/core/svg/svg_transform_list.h"
#include "third_party/blink/renderer/core/svg/svg_view_element.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/text/character_visitor.h"
#include "third_party/blink/renderer/platform/wtf/text/parsing_utilities.h"

namespace blink {

void SVGViewSpec::Trace(Visitor* visitor) const {
  visitor->Trace(view_box_);
  visitor->Trace(preserve_aspect_ratio_);
  visitor->Trace(transform_);
}

const SVGViewSpec* SVGViewSpec::CreateFromFragment(const String& fragment) {
  SVGViewSpec* view_spec = MakeGarbageCollected<SVGViewSpec>();
  if (!view_spec->ParseViewSpec(fragment))
    return nullptr;
  return view_spec;
}

const SVGViewSpec* SVGViewSpec::CreateForViewElement(
    const SVGViewElement& view) {
  SVGViewSpec* view_spec = MakeGarbageCollected<SVGViewSpec>();
  if (view.HasValidViewBox())
    view_spec->view_box_ = view.viewBox()->CurrentValue()->Clone();
  if (view.preserveAspectRatio()->IsSpecified()) {
    view_spec->preserve_aspect_ratio_ =
        view.preserveAspectRatio()->CurrentValue()->Clone();
  }
  if (view.hasAttribute(svg_names::kZoomAndPanAttr))
    view_spec->zoom_and_pan_ = view.zoomAndPan();
  return view_spec;
}

const SVGViewSpec* SVGViewSpec::CreateFromAspectRatio(
    const SVGPreserveAspectRatio* preserve_aspect_ratio) {
  if (!preserve_aspect_ratio) {
    return nullptr;
  }
  SVGViewSpec* view_spec = MakeGarbageCollected<SVGViewSpec>();
  view_spec->preserve_aspect_ratio_ = preserve_aspect_ratio;
  return view_spec;
}

bool SVGViewSpec::ParseViewSpec(const String& spec) {
  if (spec.empty())
    return false;
  return VisitCharacters(
      spec, [&](auto chars) { return ParseViewSpecInternal(chars); });
}

namespace {

enum ViewSpecFunctionType {
  kUnknown,
  kPreserveAspectRatio,
  kTransform,
  kViewBox,
  kViewTarget,
  kZoomAndPan,
};

template <typename CharType>
static ViewSpecFunctionType ScanViewSpecFunction(
    const base::span<const CharType> chars,
    size_t& position) {
  switch (chars[position]) {
    case 'v':
      if (SkipToken(chars, "viewBox", position)) {
        return kViewBox;
      }
      if (SkipToken(chars, "viewTarget", position)) {
        return kViewTarget;
      }
      break;
    case 'z':
      if (SkipToken(chars, "zoomAndPan", position)) {
        return kZoomAndPan;
      }
      break;
    case 'p':
      if (SkipToken(chars, "preserveAspectRatio", position)) {
        return kPreserveAspectRatio;
      }
      break;
    case 't':
      if (SkipToken(chars, "transform", position)) {
        return kTransform;
      }
      break;
  }
  return kUnknown;
}

}  // namespace

template <typename CharType>
bool SVGViewSpec::ParseViewSpecInternal(base::span<const CharType> chars) {
  if (!SkipToken(chars, "svgView")) {
    return false;
  }

  size_t position = 0;
  if (!SkipExactly<CharType>(chars, '(', position)) {
    return false;
  }

  while (position < chars.size() && chars[position] != ')') {
    ViewSpecFunctionType function_type = ScanViewSpecFunction(chars, position);
    if (function_type == kUnknown)
      return false;

    if (!SkipExactly<CharType>(chars, '(', position)) {
      return false;
    }

    switch (function_type) {
      case kViewBox: {
        float x = 0.0f;
        float y = 0.0f;
        float width = 0.0f;
        float height = 0.0f;
        auto span = chars.subspan(position);
        if (!(ParseNumber(span, x) && ParseNumber(span, y) &&
              ParseNumber(span, width) &&
              ParseNumber(span, height, kDisallowWhitespace))) {
          return false;
        }
        if (width < 0 || height < 0)
          return false;
        view_box_ = MakeGarbageCollected<SVGRect>(x, y, width, height);
        position = span.data() - chars.data();
        break;
      }
      case kViewTarget: {
        // Ignore arguments.
        position = SkipUntil<CharType>(chars, position, ')');
        break;
      }
      case kZoomAndPan:
        zoom_and_pan_ = SVGZoomAndPan::Parse(chars, position);
        if (zoom_and_pan_ == kSVGZoomAndPanUnknown)
          return false;
        break;
      case kPreserveAspectRatio: {
        auto* preserve_aspect_ratio =
            MakeGarbageCollected<SVGPreserveAspectRatio>();
        auto span = chars.subspan(position);
        if (!preserve_aspect_ratio->Parse(span, false)) {
          return false;
        }
        position = span.data() - chars.data();
        preserve_aspect_ratio_ = preserve_aspect_ratio;
        break;
      }
      case kTransform: {
        auto* transform = MakeGarbageCollected<SVGTransformList>();
        transform->Parse(chars, position);
        transform_ = transform;
        break;
      }
      default:
        NOTREACHED();
    }

    if (!SkipExactly<CharType>(chars, ')', position)) {
      return false;
    }

    SkipExactly<CharType>(chars, ';', position);
  }
  return SkipExactly<CharType>(chars, ')', position);
}

}  // namespace blink
