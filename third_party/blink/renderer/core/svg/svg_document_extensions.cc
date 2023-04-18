/*
 * Copyright (C) 2006 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
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

#include "third_party/blink/renderer/core/svg/svg_document_extensions.h"

#include "base/auto_reset.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/svg/animation/smil_time_container.h"
#include "third_party/blink/renderer/core/svg/svg_svg_element.h"

namespace blink {

SVGDocumentExtensions::SVGDocumentExtensions(Document* document)
    : document_(document) {}

SVGDocumentExtensions::~SVGDocumentExtensions() = default;

void SVGDocumentExtensions::AddTimeContainer(SVGSVGElement* element) {
  time_containers_.insert(element);
}

void SVGDocumentExtensions::RemoveTimeContainer(SVGSVGElement* element) {
  time_containers_.erase(element);
}

void SVGDocumentExtensions::AddWebAnimationsPendingSVGElement(
    SVGElement& element) {
  web_animations_pending_svg_elements_.insert(&element);
}

bool SVGDocumentExtensions::ServiceSmilOnAnimationFrame(Document& document) {
  if (!document.SvgExtensions())
    return false;
  return document.AccessSVGExtensions().ServiceSmilAnimations();
}

void SVGDocumentExtensions::ServiceWebAnimationsOnAnimationFrame(
    Document& document) {
  if (!document.SvgExtensions())
    return;
  document.AccessSVGExtensions().ServiceWebAnimations();
}

bool SVGDocumentExtensions::ServiceSmilAnimations() {
  bool did_schedule_animation_frame = false;
  HeapVector<Member<SVGSVGElement>> time_containers(time_containers_);
  for (const auto& container : time_containers) {
    did_schedule_animation_frame |=
        container->TimeContainer()->ServiceAnimations();
  }
  return did_schedule_animation_frame;
}

void SVGDocumentExtensions::ServiceWebAnimations() {
  SVGElementSet web_animations_pending_svg_elements;
  web_animations_pending_svg_elements.swap(
      web_animations_pending_svg_elements_);

  // TODO(alancutter): Make SVG animation effect application a separate document
  // lifecycle phase from servicing animations to be responsive to Javascript
  // manipulation of exposed animation objects.
  for (auto& svg_element : web_animations_pending_svg_elements)
    svg_element->ApplyActiveWebAnimations();

  DCHECK(web_animations_pending_svg_elements_.empty());
}

void SVGDocumentExtensions::StartAnimations() {
  // FIXME: Eventually every "Time Container" will need a way to latch on to
  // some global timer starting animations for a document will do this
  // "latching"
  // FIXME: We hold a ref pointers to prevent a shadow tree from getting removed
  // out from underneath us.  In the future we should refactor the use-element
  // to avoid this. See https://webkit.org/b/53704
  HeapVector<Member<SVGSVGElement>> time_containers(time_containers_);
  for (const auto& container : time_containers) {
    SMILTimeContainer* time_container = container->TimeContainer();
    if (!time_container->IsStarted())
      time_container->Start();
  }
}

void SVGDocumentExtensions::PauseAnimations() {
  for (SVGSVGElement* element : time_containers_)
    element->pauseAnimations();
}

bool SVGDocumentExtensions::HasSmilAnimations() const {
  for (SVGSVGElement* element : time_containers_) {
    if (element->TimeContainer()->HasAnimations())
      return true;
  }
  return false;
}

void SVGDocumentExtensions::DispatchSVGLoadEventToOutermostSVGElements() {
  HeapVector<Member<SVGSVGElement>> time_containers(time_containers_);
  for (const auto& container : time_containers) {
    SVGSVGElement* outer_svg = container.Get();
    if (!outer_svg->IsOutermostSVGSVGElement())
      continue;

    // Don't dispatch the load event document is not wellformed (for
    // XML/standalone svg).
    if (outer_svg->GetDocument().WellFormed() ||
        !outer_svg->GetDocument().IsSVGDocument())
      outer_svg->SendSVGLoadEventIfPossible();
  }
}

void SVGDocumentExtensions::AddSVGRootWithRelativeLengthDescendents(
    SVGSVGElement* svg_root) {
#if DCHECK_IS_ON()
  DCHECK(!in_relative_length_svg_roots_invalidation_);
#endif
  relative_length_svg_roots_.insert(svg_root);
}

void SVGDocumentExtensions::RemoveSVGRootWithRelativeLengthDescendents(
    SVGSVGElement* svg_root) {
#if DCHECK_IS_ON()
  DCHECK(!in_relative_length_svg_roots_invalidation_);
#endif
  relative_length_svg_roots_.erase(svg_root);
}

void SVGDocumentExtensions::InvalidateSVGRootsWithRelativeLengthDescendents() {
#if DCHECK_IS_ON()
  DCHECK(!in_relative_length_svg_roots_invalidation_);
  base::AutoReset<bool> in_relative_length_svg_roots_change(
      &in_relative_length_svg_roots_invalidation_, true);
#endif

  for (SVGSVGElement* element : relative_length_svg_roots_)
    element->InvalidateRelativeLengthClients();
}

bool SVGDocumentExtensions::ZoomAndPanEnabled() const {
  SVGSVGElement* svg = rootElement(*document_);
  return !svg || svg->ZoomAndPanEnabled();
}

void SVGDocumentExtensions::StartPan(const gfx::PointF& start) {
  if (SVGSVGElement* svg = rootElement(*document_)) {
    translate_ = gfx::Vector2dF(start.x() - svg->CurrentTranslate().x(),
                                start.y() - svg->CurrentTranslate().y());
  }
}

void SVGDocumentExtensions::UpdatePan(const gfx::PointF& pos) const {
  if (SVGSVGElement* svg = rootElement(*document_)) {
    svg->SetCurrentTranslate(
        gfx::Vector2dF(pos.x() - translate_.x(), pos.y() - translate_.y()));
  }
}

SVGSVGElement* SVGDocumentExtensions::rootElement(const Document& document) {
  return DynamicTo<SVGSVGElement>(document.documentElement());
}

void SVGDocumentExtensions::Trace(Visitor* visitor) const {
  visitor->Trace(document_);
  visitor->Trace(time_containers_);
  visitor->Trace(web_animations_pending_svg_elements_);
  visitor->Trace(relative_length_svg_roots_);
}

}  // namespace blink
