/*
 * Copyright (C) 2007 Rob Buis <buis@kde.org>
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SVG_SVG_VIEW_SPEC_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SVG_SVG_VIEW_SPEC_H_

#include "third_party/blink/renderer/core/svg/svg_zoom_and_pan.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class SVGPreserveAspectRatio;
class SVGRect;
class SVGTransformList;
class SVGViewElement;

class SVGViewSpec final : public GarbageCollected<SVGViewSpec> {
 public:
  static SVGViewSpec* CreateFromFragment(const String&);
  static SVGViewSpec* CreateForViewElement(const SVGViewElement&);

  SVGViewSpec();

  const SVGRect* ViewBox() const { return view_box_; }
  const SVGPreserveAspectRatio* PreserveAspectRatio() const {
    return preserve_aspect_ratio_;
  }
  const SVGTransformList* Transform() const { return transform_; }
  SVGZoomAndPanType ZoomAndPan() const { return zoom_and_pan_; }

  void Trace(Visitor*);

 private:
  bool ParseViewSpec(const String&);
  template <typename CharType>
  bool ParseViewSpecInternal(const CharType* ptr, const CharType* end);

  Member<SVGRect> view_box_;
  Member<SVGPreserveAspectRatio> preserve_aspect_ratio_;
  Member<SVGTransformList> transform_;
  SVGZoomAndPanType zoom_and_pan_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SVG_SVG_VIEW_SPEC_H_
