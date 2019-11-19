/*
 * Copyright (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SVG_SVG_TEXT_PATH_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SVG_SVG_TEXT_PATH_ELEMENT_H_

#include "third_party/blink/renderer/core/svg/svg_text_content_element.h"
#include "third_party/blink/renderer/core/svg/svg_uri_reference.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

enum SVGTextPathMethodType {
  kSVGTextPathMethodUnknown = 0,
  kSVGTextPathMethodAlign,
  kSVGTextPathMethodStretch
};
DECLARE_SVG_ENUM_MAP(SVGTextPathMethodType);

enum SVGTextPathSpacingType {
  kSVGTextPathSpacingUnknown = 0,
  kSVGTextPathSpacingAuto,
  kSVGTextPathSpacingExact
};
DECLARE_SVG_ENUM_MAP(SVGTextPathSpacingType);

class SVGTextPathElement final : public SVGTextContentElement,
                                 public SVGURIReference {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(SVGTextPathElement);

 public:
  // Forward declare enumerations in the W3C naming scheme, for IDL generation.
  enum {
    kTextpathMethodtypeUnknown = kSVGTextPathMethodUnknown,
    kTextpathMethodtypeAlign = kSVGTextPathMethodAlign,
    kTextpathMethodtypeStretch = kSVGTextPathMethodStretch,
    kTextpathSpacingtypeUnknown = kSVGTextPathSpacingUnknown,
    kTextpathSpacingtypeAuto = kSVGTextPathSpacingAuto,
    kTextpathSpacingtypeExact = kSVGTextPathSpacingExact
  };

  explicit SVGTextPathElement(Document&);

  SVGAnimatedLength* startOffset() const { return start_offset_.Get(); }
  SVGAnimatedEnumeration<SVGTextPathMethodType>* method() {
    return method_.Get();
  }
  SVGAnimatedEnumeration<SVGTextPathSpacingType>* spacing() {
    return spacing_.Get();
  }

  void Trace(blink::Visitor*) override;

 private:
  ~SVGTextPathElement() override;

  void ClearResourceReferences();

  void BuildPendingResource() override;
  InsertionNotificationRequest InsertedInto(ContainerNode&) override;
  void RemovedFrom(ContainerNode&) override;

  void SvgAttributeChanged(const QualifiedName&) override;

  LayoutObject* CreateLayoutObject(const ComputedStyle&, LegacyLayout) override;
  bool LayoutObjectIsNeeded(const ComputedStyle&) const override;

  bool SelfHasRelativeLengths() const override;

  Member<SVGAnimatedLength> start_offset_;
  Member<SVGAnimatedEnumeration<SVGTextPathMethodType>> method_;
  Member<SVGAnimatedEnumeration<SVGTextPathSpacingType>> spacing_;
  Member<IdTargetObserver> target_id_observer_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SVG_SVG_TEXT_PATH_ELEMENT_H_
