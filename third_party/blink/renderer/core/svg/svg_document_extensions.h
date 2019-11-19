/*
 * Copyright (C) 2006 Apple Inc. All rights reserved.
 * Copyright (C) 2006, 2008 Nikolas Zimmermann <zimmermann@kde.org>
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SVG_SVG_DOCUMENT_EXTENSIONS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SVG_SVG_DOCUMENT_EXTENSIONS_H_

#include "base/macros.h"
#include "third_party/blink/renderer/core/layout/svg/svg_resources_cache.h"
#include "third_party/blink/renderer/platform/geometry/float_point.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"

namespace blink {

class Document;
class SVGElement;
class SVGSVGElement;
class SubtreeLayoutScope;

class SVGDocumentExtensions final
    : public GarbageCollected<SVGDocumentExtensions> {
 public:
  explicit SVGDocumentExtensions(Document*);
  ~SVGDocumentExtensions();

  void AddTimeContainer(SVGSVGElement*);
  void RemoveTimeContainer(SVGSVGElement*);

  // Records the SVG element as having a Web Animation on an SVG attribute that
  // needs applying.
  void AddWebAnimationsPendingSVGElement(SVGElement&);

  static void ServiceOnAnimationFrame(Document&);

  void StartAnimations();
  void PauseAnimations();
  void ServiceAnimations();

  void DispatchSVGLoadEventToOutermostSVGElements();

  SVGResourcesCache& ResourcesCache() { return resources_cache_; }

  void AddSVGRootWithRelativeLengthDescendents(SVGSVGElement*);
  void RemoveSVGRootWithRelativeLengthDescendents(SVGSVGElement*);
  void InvalidateSVGRootsWithRelativeLengthDescendents(SubtreeLayoutScope*);

  bool ZoomAndPanEnabled() const;

  void StartPan(const FloatPoint& start);
  void UpdatePan(const FloatPoint& pos) const;

  static SVGSVGElement* rootElement(const Document&);
  SVGSVGElement* rootElement() const;

  void Trace(blink::Visitor*);

 private:
  Member<Document> document_;
  HeapHashSet<Member<SVGSVGElement>> time_containers_;
  using SVGElementSet = HeapHashSet<Member<SVGElement>>;
  SVGElementSet web_animations_pending_svg_elements_;
  SVGResourcesCache resources_cache_;
  // Root SVG elements with relative length descendants.
  HeapHashSet<Member<SVGSVGElement>> relative_length_svg_roots_;
  FloatPoint translate_;
#if DCHECK_IS_ON()
  bool in_relative_length_svg_roots_invalidation_ = false;
#endif
  DISALLOW_COPY_AND_ASSIGN(SVGDocumentExtensions);
};

}  // namespace blink

#endif
