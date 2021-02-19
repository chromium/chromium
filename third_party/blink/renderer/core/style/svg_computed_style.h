/*
    Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>
    Copyright (C) 2005, 2006 Apple Computer, Inc.
    Copyright (C) Research In Motion Limited 2010. All rights reserved.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_SVG_COMPUTED_STYLE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_SVG_COMPUTED_STYLE_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/style_color.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/core/style/data_ref.h"
#include "third_party/blink/renderer/core/style/svg_computed_style_defs.h"
#include "third_party/blink/renderer/platform/geometry/length.h"
#include "third_party/blink/renderer/platform/graphics/graphics_types.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"

namespace blink {

class StyleDifference;

// TODO(sashab): Move this into a private class on ComputedStyle, and remove
// all methods on it, merging them into copy/creation methods on ComputedStyle
// instead. Keep the allocation logic, only allocating a new object if needed.
class SVGComputedStyle : public RefCounted<SVGComputedStyle> {
  USING_FAST_MALLOC(SVGComputedStyle);

 public:
  static scoped_refptr<SVGComputedStyle> Create() {
    return base::AdoptRef(new SVGComputedStyle);
  }
  scoped_refptr<SVGComputedStyle> Copy() const {
    return base::AdoptRef(new SVGComputedStyle(*this));
  }
  CORE_EXPORT ~SVGComputedStyle();

  bool InheritedEqual(const SVGComputedStyle&) const;
  void InheritFrom(const SVGComputedStyle&);

  CORE_EXPORT StyleDifference Diff(const SVGComputedStyle&) const;

  bool operator==(const SVGComputedStyle&) const;
  bool operator!=(const SVGComputedStyle& o) const { return !(*this == o); }

  // Initial values for all the properties
  static StyleSVGResource* InitialMarkerStartResource() { return nullptr; }
  static StyleSVGResource* InitialMarkerMidResource() { return nullptr; }
  static StyleSVGResource* InitialMarkerEndResource() { return nullptr; }

  // Setters for inherited resources
  void SetMarkerStartResource(scoped_refptr<StyleSVGResource> resource);

  void SetMarkerMidResource(scoped_refptr<StyleSVGResource> resource);

  void SetMarkerEndResource(scoped_refptr<StyleSVGResource> resource);

  // Read accessors for all the properties
  StyleSVGResource* MarkerStartResource() const {
    return inherited_resources->marker_start.get();
  }
  StyleSVGResource* MarkerMidResource() const {
    return inherited_resources->marker_mid.get();
  }
  StyleSVGResource* MarkerEndResource() const {
    return inherited_resources->marker_end.get();
  }

 protected:
  // inherited attributes
  DataRef<StyleInheritedResourceData> inherited_resources;

 private:
  enum CreateInitialType { kCreateInitial };

  CORE_EXPORT SVGComputedStyle();
  SVGComputedStyle(const SVGComputedStyle&);
  SVGComputedStyle(
      CreateInitialType);  // Used to create the initial style singleton.

  bool DiffNeedsLayoutAndPaintInvalidation(const SVGComputedStyle& other) const;
  bool DiffNeedsPaintInvalidation(const SVGComputedStyle& other) const;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_SVG_COMPUTED_STYLE_H_
