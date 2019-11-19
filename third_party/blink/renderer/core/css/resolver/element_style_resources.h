/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc.
 * All rights reserved.
 * Copyright (C) 2013 Google Inc. All rights reserved.
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
 *
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_RESOLVER_ELEMENT_STYLE_RESOURCES_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_RESOLVER_ELEMENT_STYLE_RESOURCES_H_

#include "base/macros.h"
#include "third_party/blink/renderer/core/css/css_property_id_templates.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/loader/fetch/cross_origin_attribute_value.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_parameters.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"

namespace blink {

class CSSImageGeneratorValue;
class CSSImageSetValue;
class CSSImageValue;
class CSSValue;
class ComputedStyle;
class Element;
class PseudoElement;
class SVGResource;
class StyleImage;
class StylePendingImage;
class TreeScope;

namespace cssvalue {

class CSSURIValue;

}

// Holds information about resources, requested by stylesheets.
// Lifetime: per-element style resolve.
class ElementStyleResources {
  STACK_ALLOCATED();

 public:
  ElementStyleResources(Element&,
                        float device_scale_factor,
                        PseudoElement* pseudo_element);

  StyleImage* GetStyleImage(CSSPropertyID, const CSSValue&);
  StyleImage* CachedOrPendingFromValue(CSSPropertyID, const CSSImageValue&);
  StyleImage* SetOrPendingFromValue(CSSPropertyID, const CSSImageSetValue&);

  enum AllowExternal { kDontAllowExternalResource, kAllowExternalResource };
  SVGResource* GetSVGResourceFromValue(
      TreeScope&,
      const cssvalue::CSSURIValue&,
      AllowExternal = kDontAllowExternalResource) const;

  void LoadPendingResources(ComputedStyle*);

 private:
  StyleImage* GeneratedOrPendingFromValue(CSSPropertyID,
                                          const CSSImageGeneratorValue&);

  void LoadPendingSVGResources(ComputedStyle*);
  void LoadPendingImages(ComputedStyle*);

  StyleImage* LoadPendingImage(
      ComputedStyle*,
      StylePendingImage*,
      FetchParameters::ImageRequestOptimization,
      CrossOriginAttributeValue = kCrossOriginAttributeNotSet);

  Member<Element> element_;
  HashSet<CSSPropertyID> pending_image_properties_;
  float device_scale_factor_;
  Member<PseudoElement> pseudo_element_;
  DISALLOW_COPY_AND_ASSIGN(ElementStyleResources);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_RESOLVER_ELEMENT_STYLE_RESOURCES_H_
