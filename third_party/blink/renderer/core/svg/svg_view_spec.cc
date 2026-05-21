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
#include "third_party/blink/renderer/core/html/media/media_fragment_uri_parser.h"
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

const SVGViewSpec* SVGViewSpec::CreateFromSpatialFragment(
    const String& fragment,
    const gfx::SizeF natural_size) {
  // Use MediaFragmentURIParser for spatial dimension parsing.
  MediaFragmentURIParser media_parser(fragment);

  // Check for spatial media fragment (xywh=...).
  // https://www.w3.org/TR/media-frags/#naming-space
  const SpatialClip spatial_clip = media_parser.SpatialFragment();
  if (!spatial_clip.IsValid()) {
    return nullptr;
  }

  gfx::RectF clip_rect(spatial_clip.rect);

  if (spatial_clip.unit == SpatialClip::Unit::kPercent) {
    // Percent units resolve against the SVG's concrete object size. If that
    // size is empty (the SVG has no natural width/height that can be resolved
    // against a 0x0 default object size) there is no basis to resolve
    // percentages, so reject the fragment.
    //
    // TODO(dmangal): The Media Fragments spec does not define how percent
    // units should behave when the resource has no natural size; revisit
    // once that has been clarified.
    if (natural_size.IsEmpty()) {
      return nullptr;
    }
    clip_rect.Scale(natural_size.width() / 100.0f,
                    natural_size.height() / 100.0f);
  }

  // Clamp the clip rect to the SVG's natural bounds per the Media
  // Fragments spec, 6.1.2 [1] and 6.3.3 [2]. This prevents the
  // viewBox from mapping an oversized coordinate region into the
  // rendering box, which would distort the aspect ratio.
  //
  // Skip the clamp when the resource has no resolvable natural size
  // (pixel-based fragment on an SVG without a concrete object size).
  //
  // [1] https://www.w3.org/TR/media-frags/#valid-uri-spatial
  // [2] https://www.w3.org/TR/media-frags/#error-media-spatial
  if (!natural_size.IsEmpty()) {
    clip_rect.Intersect(gfx::RectF(natural_size));
  }

  if (clip_rect.IsEmpty()) {
    return nullptr;
  }

  // Per SVG2 §16.3.2, a spatial media fragment overrides the viewBox.
  // https://svgwg.org/svg2-draft/linking.html#SVGFragmentIdentifiersDefinitions
  SVGViewSpec* view_spec = MakeGarbageCollected<SVGViewSpec>();
  view_spec->view_box_ = MakeGarbageCollected<SVGRect>(
      clip_rect.x(), clip_rect.y(), clip_rect.width(), clip_rect.height());
  return view_spec;
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
