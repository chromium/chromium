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

#include "third_party/blink/renderer/core/svg/svg_parser_utilities.h"
#include "third_party/blink/renderer/core/svg/svg_preserve_aspect_ratio.h"
#include "third_party/blink/renderer/core/svg/svg_rect.h"
#include "third_party/blink/renderer/core/svg/svg_transform_list.h"
#include "third_party/blink/renderer/core/svg/svg_view_element.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/wtf/text/parsing_utilities.h"

namespace blink {

SVGViewSpec::SVGViewSpec() : zoom_and_pan_(kSVGZoomAndPanUnknown) {}

void SVGViewSpec::Trace(Visitor* visitor) {
  visitor->Trace(view_box_);
  visitor->Trace(preserve_aspect_ratio_);
  visitor->Trace(transform_);
}

SVGViewSpec* SVGViewSpec::CreateFromFragment(const String& fragment) {
  SVGViewSpec* view_spec = MakeGarbageCollected<SVGViewSpec>();
  if (!view_spec->ParseViewSpec(fragment))
    return nullptr;
  return view_spec;
}

SVGViewSpec* SVGViewSpec::CreateForViewElement(const SVGViewElement& view) {
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

bool SVGViewSpec::ParseViewSpec(const String& spec) {
  if (spec.IsEmpty())
    return false;
  if (spec.Is8Bit()) {
    const LChar* ptr = spec.Characters8();
    const LChar* end = ptr + spec.length();
    return ParseViewSpecInternal(ptr, end);
  }
  const UChar* ptr = spec.Characters16();
  const UChar* end = ptr + spec.length();
  return ParseViewSpecInternal(ptr, end);
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
static ViewSpecFunctionType ScanViewSpecFunction(const CharType*& ptr,
                                                 const CharType* end) {
  DCHECK_LT(ptr, end);
  switch (*ptr) {
    case 'v':
      if (SkipToken(ptr, end, "viewBox"))
        return kViewBox;
      if (SkipToken(ptr, end, "viewTarget"))
        return kViewTarget;
      break;
    case 'z':
      if (SkipToken(ptr, end, "zoomAndPan"))
        return kZoomAndPan;
      break;
    case 'p':
      if (SkipToken(ptr, end, "preserveAspectRatio"))
        return kPreserveAspectRatio;
      break;
    case 't':
      if (SkipToken(ptr, end, "transform"))
        return kTransform;
      break;
  }
  return kUnknown;
}

}  // namespace

template <typename CharType>
bool SVGViewSpec::ParseViewSpecInternal(const CharType* ptr,
                                        const CharType* end) {
  if (!SkipToken(ptr, end, "svgView"))
    return false;

  if (!SkipExactly<CharType>(ptr, end, '('))
    return false;

  while (ptr < end && *ptr != ')') {
    ViewSpecFunctionType function_type = ScanViewSpecFunction(ptr, end);
    if (function_type == kUnknown)
      return false;

    if (!SkipExactly<CharType>(ptr, end, '('))
      return false;

    switch (function_type) {
      case kViewBox: {
        float x = 0.0f;
        float y = 0.0f;
        float width = 0.0f;
        float height = 0.0f;
        if (!(ParseNumber(ptr, end, x) && ParseNumber(ptr, end, y) &&
              ParseNumber(ptr, end, width) &&
              ParseNumber(ptr, end, height, kDisallowWhitespace)))
          return false;
        view_box_ =
            MakeGarbageCollected<SVGRect>(FloatRect(x, y, width, height));
        break;
      }
      case kViewTarget: {
        // Ignore arguments.
        SkipUntil<CharType>(ptr, end, ')');
        break;
      }
      case kZoomAndPan:
        zoom_and_pan_ = SVGZoomAndPan::Parse(ptr, end);
        if (zoom_and_pan_ == kSVGZoomAndPanUnknown)
          return false;
        break;
      case kPreserveAspectRatio:
        preserve_aspect_ratio_ = MakeGarbageCollected<SVGPreserveAspectRatio>();
        if (!preserve_aspect_ratio_->Parse(ptr, end, false))
          return false;
        break;
      case kTransform:
        transform_ = MakeGarbageCollected<SVGTransformList>();
        transform_->Parse(ptr, end);
        break;
      default:
        NOTREACHED();
        break;
    }

    if (!SkipExactly<CharType>(ptr, end, ')'))
      return false;

    SkipExactly<CharType>(ptr, end, ';');
  }
  return SkipExactly<CharType>(ptr, end, ')');
}

}  // namespace blink
