/*
    Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>
    Copyright (C) Research In Motion Limited 2010. All rights reserved.

    Based on khtml code by:
    Copyright (C) 2000-2003 Lars Knoll (knoll@kde.org)
              (C) 2000 Antti Koivisto (koivisto@kde.org)
              (C) 2000-2003 Dirk Mueller (mueller@kde.org)
              (C) 2002-2003 Apple Computer, Inc.

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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_SVG_COMPUTED_STYLE_DEFS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_SVG_COMPUTED_STYLE_DEFS_H_

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/style_color.h"
#include "third_party/blink/renderer/core/style/style_path.h"
#include "third_party/blink/renderer/platform/geometry/length.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class StyleSVGResource;

typedef base::RefCountedData<WTF::Vector<Length>> SVGDashArray;

enum SVGPaintType {
  SVG_PAINTTYPE_RGBCOLOR,
  SVG_PAINTTYPE_NONE,
  SVG_PAINTTYPE_CURRENTCOLOR,
  SVG_PAINTTYPE_URI_NONE,
  SVG_PAINTTYPE_URI_CURRENTCOLOR,
  SVG_PAINTTYPE_URI_RGBCOLOR,
  SVG_PAINTTYPE_URI
};

enum EBaselineShift { BS_LENGTH, BS_SUB, BS_SUPER };

enum ETextAnchor { TA_START, TA_MIDDLE, TA_END };

enum EColorInterpolation { CI_AUTO, CI_SRGB, CI_LINEARRGB };

enum EColorRendering { CR_AUTO, CR_OPTIMIZESPEED, CR_OPTIMIZEQUALITY };
enum EShapeRendering {
  SR_AUTO,
  SR_OPTIMIZESPEED,
  SR_CRISPEDGES,
  SR_GEOMETRICPRECISION
};

enum EAlignmentBaseline {
  AB_AUTO,
  AB_BASELINE,
  AB_BEFORE_EDGE,
  AB_TEXT_BEFORE_EDGE,
  AB_MIDDLE,
  AB_CENTRAL,
  AB_AFTER_EDGE,
  AB_TEXT_AFTER_EDGE,
  AB_IDEOGRAPHIC,
  AB_ALPHABETIC,
  AB_HANGING,
  AB_MATHEMATICAL
};

enum EDominantBaseline {
  DB_AUTO,
  DB_USE_SCRIPT,
  DB_NO_CHANGE,
  DB_RESET_SIZE,
  DB_IDEOGRAPHIC,
  DB_ALPHABETIC,
  DB_HANGING,
  DB_MATHEMATICAL,
  DB_CENTRAL,
  DB_MIDDLE,
  DB_TEXT_AFTER_EDGE,
  DB_TEXT_BEFORE_EDGE
};

enum EVectorEffect { VE_NONE, VE_NON_SCALING_STROKE };

enum EBufferedRendering { BR_AUTO, BR_DYNAMIC, BR_STATIC };

enum EMaskType { MT_LUMINANCE, MT_ALPHA };

enum EPaintOrderType {
  PT_NONE = 0,
  PT_FILL = 1,
  PT_STROKE = 2,
  PT_MARKERS = 3
};

enum EPaintOrder {
  kPaintOrderNormal = 0,
  kPaintOrderFillStrokeMarkers = 1,
  kPaintOrderFillMarkersStroke = 2,
  kPaintOrderStrokeFillMarkers = 3,
  kPaintOrderStrokeMarkersFill = 4,
  kPaintOrderMarkersFillStroke = 5,
  kPaintOrderMarkersStrokeFill = 6
};

struct SVGPaint {
  CORE_EXPORT SVGPaint();
  SVGPaint(Color color);
  SVGPaint(const SVGPaint& paint);
  CORE_EXPORT ~SVGPaint();
  CORE_EXPORT SVGPaint& operator=(const SVGPaint& paint);

  CORE_EXPORT bool operator==(const SVGPaint&) const;
  bool operator!=(const SVGPaint& other) const { return !(*this == other); }

  bool IsNone() const { return type == SVG_PAINTTYPE_NONE; }
  bool IsColor() const {
    return type == SVG_PAINTTYPE_RGBCOLOR || type == SVG_PAINTTYPE_CURRENTCOLOR;
  }
  // Used by CSSPropertyEquality::PropertiesEqual.
  bool EqualTypeOrColor(const SVGPaint& other) const {
    return type == other.type &&
           (type != SVG_PAINTTYPE_RGBCOLOR || color == other.color);
  }
  bool HasFallbackColor() const {
    return type == SVG_PAINTTYPE_URI_CURRENTCOLOR ||
           type == SVG_PAINTTYPE_URI_RGBCOLOR;
  }
  bool HasColor() const { return IsColor() || HasFallbackColor(); }
  bool HasUrl() const { return type >= SVG_PAINTTYPE_URI_NONE; }
  bool HasCurrentColor() const {
    return type == SVG_PAINTTYPE_CURRENTCOLOR ||
           type == SVG_PAINTTYPE_URI_CURRENTCOLOR;
  }
  StyleSVGResource* Resource() const { return resource.get(); }

  const Color& GetColor() const { return color; }
  const AtomicString& GetUrl() const;

  scoped_refptr<StyleSVGResource> resource;
  Color color;
  SVGPaintType type;
};

// Inherited/Non-Inherited Style Datastructures
class StyleFillData : public RefCounted<StyleFillData> {
  USING_FAST_MALLOC(StyleFillData);

 public:
  static scoped_refptr<StyleFillData> Create() {
    return base::AdoptRef(new StyleFillData);
  }
  scoped_refptr<StyleFillData> Copy() const {
    return base::AdoptRef(new StyleFillData(*this));
  }

  bool operator==(const StyleFillData&) const;
  bool operator!=(const StyleFillData& other) const {
    return !(*this == other);
  }

  float opacity;
  SVGPaint paint;
  SVGPaint visited_link_paint;

 private:
  StyleFillData();
  StyleFillData(const StyleFillData&);
};

class UnzoomedLength {
  DISALLOW_NEW();

 public:
  explicit UnzoomedLength(const Length& length) : length_(length) {}

  bool IsZero() const { return length_.IsZero(); }

  bool operator==(const UnzoomedLength& other) const {
    return length_ == other.length_;
  }
  bool operator!=(const UnzoomedLength& other) const {
    return !operator==(other);
  }

  const Length& length() const { return length_; }

 private:
  Length length_;
};

class CORE_EXPORT StyleStrokeData : public RefCounted<StyleStrokeData> {
  USING_FAST_MALLOC(StyleStrokeData);

 public:
  static scoped_refptr<StyleStrokeData> Create() {
    return base::AdoptRef(new StyleStrokeData);
  }

  scoped_refptr<StyleStrokeData> Copy() const {
    return base::AdoptRef(new StyleStrokeData(*this));
  }

  bool operator==(const StyleStrokeData&) const;
  bool operator!=(const StyleStrokeData& other) const {
    return !(*this == other);
  }

  float opacity;
  float miter_limit;

  UnzoomedLength width;
  Length dash_offset;
  scoped_refptr<SVGDashArray> dash_array;

  SVGPaint paint;
  SVGPaint visited_link_paint;

 private:
  StyleStrokeData();
  StyleStrokeData(const StyleStrokeData&);
};

class StyleStopData : public RefCounted<StyleStopData> {
  USING_FAST_MALLOC(StyleStopData);

 public:
  static scoped_refptr<StyleStopData> Create() {
    return base::AdoptRef(new StyleStopData);
  }
  scoped_refptr<StyleStopData> Copy() const {
    return base::AdoptRef(new StyleStopData(*this));
  }

  bool operator==(const StyleStopData&) const;
  bool operator!=(const StyleStopData& other) const {
    return !(*this == other);
  }

  StyleColor color;
  float opacity;

 private:
  StyleStopData();
  StyleStopData(const StyleStopData&);
};

// Note: the rule for this class is, *no inheritance* of these props
class CORE_EXPORT StyleMiscData : public RefCounted<StyleMiscData> {
  USING_FAST_MALLOC(StyleMiscData);

 public:
  static scoped_refptr<StyleMiscData> Create() {
    return base::AdoptRef(new StyleMiscData);
  }
  scoped_refptr<StyleMiscData> Copy() const {
    return base::AdoptRef(new StyleMiscData(*this));
  }

  bool operator==(const StyleMiscData&) const;
  bool operator!=(const StyleMiscData& other) const {
    return !(*this == other);
  }

  Length baseline_shift_value;

  Color flood_color;
  Color lighting_color;

  float flood_opacity;

  bool flood_color_is_current_color;
  bool lighting_color_is_current_color;

 private:
  StyleMiscData();
  StyleMiscData(const StyleMiscData&);
};

// Non-inherited resources
class StyleResourceData : public RefCounted<StyleResourceData> {
  USING_FAST_MALLOC(StyleResourceData);

 public:
  static scoped_refptr<StyleResourceData> Create() {
    return base::AdoptRef(new StyleResourceData);
  }
  ~StyleResourceData();
  scoped_refptr<StyleResourceData> Copy() const {
    return base::AdoptRef(new StyleResourceData(*this));
  }

  bool operator==(const StyleResourceData&) const;
  bool operator!=(const StyleResourceData& other) const {
    return !(*this == other);
  }

  scoped_refptr<StyleSVGResource> masker;

 private:
  StyleResourceData();
  StyleResourceData(const StyleResourceData&);
};

// Inherited resources
class StyleInheritedResourceData
    : public RefCounted<StyleInheritedResourceData> {
  USING_FAST_MALLOC(StyleInheritedResourceData);

 public:
  static scoped_refptr<StyleInheritedResourceData> Create() {
    return base::AdoptRef(new StyleInheritedResourceData);
  }
  ~StyleInheritedResourceData();
  scoped_refptr<StyleInheritedResourceData> Copy() const {
    return base::AdoptRef(new StyleInheritedResourceData(*this));
  }

  bool operator==(const StyleInheritedResourceData&) const;
  bool operator!=(const StyleInheritedResourceData& other) const {
    return !(*this == other);
  }

  scoped_refptr<StyleSVGResource> marker_start;
  scoped_refptr<StyleSVGResource> marker_mid;
  scoped_refptr<StyleSVGResource> marker_end;

 private:
  StyleInheritedResourceData();
  StyleInheritedResourceData(const StyleInheritedResourceData&);
};

// Geometry properties
class StyleGeometryData : public RefCounted<StyleGeometryData> {
  USING_FAST_MALLOC(StyleGeometryData);

 public:
  static scoped_refptr<StyleGeometryData> Create() {
    return base::AdoptRef(new StyleGeometryData);
  }
  scoped_refptr<StyleGeometryData> Copy() const;
  bool operator==(const StyleGeometryData&) const;
  bool operator!=(const StyleGeometryData& other) const {
    return !(*this == other);
  }
  scoped_refptr<StylePath> d;
  Length cx;
  Length cy;
  Length x;
  Length y;
  Length r;
  Length rx;
  Length ry;

 private:
  StyleGeometryData();
  StyleGeometryData(const StyleGeometryData&);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_SVG_COMPUTED_STYLE_DEFS_H_
