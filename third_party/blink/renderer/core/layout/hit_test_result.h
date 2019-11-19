/*
 * Copyright (C) 2006 Apple Computer, Inc.
 * Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies)
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_HIT_TEST_RESULT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_HIT_TEST_RESULT_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/editing/forward.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_offset.h"
#include "third_party/blink/renderer/core/layout/hit_test_location.h"
#include "third_party/blink/renderer/core/layout/hit_test_request.h"
#include "third_party/blink/renderer/platform/geometry/float_quad.h"
#include "third_party/blink/renderer/platform/geometry/float_rect.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/text/text_direction.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector_traits.h"

namespace blink {

class Element;
class LocalFrame;
class HTMLAreaElement;
class HTMLMediaElement;
class Image;
class KURL;
class MediaStreamDescriptor;
class Node;
class LayoutObject;
class Region;
class Scrollbar;
struct PhysicalOffset;

// List-based hit test testing can continue even after a hit has been found.
// This is used to support fuzzy matching with rect-based hit tests as well as
// penetrating tests which collect all nodes (see: HitTestRequest::RequestType).
enum ListBasedHitTestBehavior { kContinueHitTesting, kStopHitTesting };

class CORE_EXPORT HitTestResult {
  DISALLOW_NEW();

 public:
  typedef HeapLinkedHashSet<Member<Node>> NodeSet;

  HitTestResult();
  HitTestResult(const HitTestRequest&, const HitTestLocation&);
  HitTestResult(const HitTestResult&);
  ~HitTestResult();
  HitTestResult& operator=(const HitTestResult&);
  void Trace(blink::Visitor*);

  bool EqualForCacheability(const HitTestResult&) const;
  void CacheValues(const HitTestResult& other);

  // Populate this object based on another HitTestResult; similar to assignment
  // operator but don't assign any of the request parameters. ie. This method
  // avoids setting |m_hitTestLocation|, |m_hitTestRequest|.
  void PopulateFromCachedResult(const HitTestResult&);

  // For point-based hit tests, these accessors provide information about the
  // node under the point. For rect-based hit tests they are meaningless
  // (reflect the last candidate node observed in the rect).
  // FIXME: Make these less error-prone for rect-based hit tests (center point
  // or fail).
  Node* InnerNode() const { return inner_node_.Get(); }
  Node* InertNode() const { return inert_node_.Get(); }
  Node* InnerPossiblyPseudoNode() const {
    return inner_possibly_pseudo_node_.Get();
  }
  Element* InnerElement() const { return inner_element_.Get(); }

  // If innerNode is an image map or image map area, return the associated image
  // node.
  Node* InnerNodeOrImageMapImage() const;

  Element* URLElement() const { return inner_url_element_.Get(); }
  Scrollbar* GetScrollbar() const { return scrollbar_.Get(); }
  bool IsOverEmbeddedContentView() const {
    return is_over_embedded_content_view_;
  }

  // The hit-tested point in the coordinates of the innerNode frame, the frame
  // containing innerNode.
  const PhysicalOffset& PointInInnerNodeFrame() const {
    return point_in_inner_node_frame_;
  }
  void SetPointInInnerNodeFrame(const PhysicalOffset& point) {
    point_in_inner_node_frame_ = point;
  }
  IntPoint RoundedPointInInnerNodeFrame() const {
    return RoundedIntPoint(PointInInnerNodeFrame());
  }
  LocalFrame* InnerNodeFrame() const;

  // The hit-tested point in the coordinates of the inner node.
  const PhysicalOffset& LocalPoint() const { return local_point_; }
  void SetNodeAndPosition(Node* node, const PhysicalOffset& p) {
    local_point_ = p;
    SetInnerNode(node);
  }

  PositionWithAffinity GetPosition() const;
  LayoutObject* GetLayoutObject() const;

  void SetToShadowHostIfInRestrictedShadowRoot();

  const HitTestRequest& GetHitTestRequest() const { return hit_test_request_; }

  void SetInnerNode(Node*);
  void SetInertNode(Node*);
  HTMLAreaElement* ImageAreaForImage() const;
  void SetURLElement(Element*);
  void SetScrollbar(Scrollbar*);
  void SetIsOverEmbeddedContentView(bool b) {
    is_over_embedded_content_view_ = b;
  }

  bool IsSelected(const HitTestLocation& location) const;
  String Title(TextDirection&) const;
  const AtomicString& AltDisplayString() const;
  Image* GetImage() const;
  IntRect ImageRect() const;
  KURL AbsoluteImageURL() const;
  KURL AbsoluteMediaURL() const;
  MediaStreamDescriptor* GetMediaStreamDescriptor() const;
  KURL AbsoluteLinkURL() const;
  String TextContent() const;
  bool IsLiveLink() const;
  bool IsContentEditable() const;

  const String& CanvasRegionId() const { return canvas_region_id_; }
  void SetCanvasRegionId(const String& id) { canvas_region_id_ = id; }

  bool IsOverLink() const;

  bool IsCacheable() const { return cacheable_; }
  void SetCacheable(bool cacheable) { cacheable_ = cacheable; }

  // TODO(pdr): When using the default rect argument, this function does not
  // check if the tapped area is entirely contained by the HitTestLocation's
  // bounding box. Callers should pass a PhysicalRect as the third parameter so
  // hit testing can early-out when a tapped area is covered.
  ListBasedHitTestBehavior AddNodeToListBasedTestResult(
      Node*,
      const HitTestLocation&,
      const PhysicalRect& = PhysicalRect());
  ListBasedHitTestBehavior AddNodeToListBasedTestResult(Node*,
                                                        const HitTestLocation&,
                                                        const Region&);

  void Append(const HitTestResult&);

  // If m_listBasedTestResult is 0 then set it to a new NodeSet. Return
  // *m_listBasedTestResult. Lazy allocation makes sense because the NodeSet is
  // seldom necessary, and it's somewhat expensive to allocate and initialize.
  // This method does the same thing as mutableListBasedTestResult(), but here
  // the return value is const.
  const NodeSet& ListBasedTestResult() const;

  // Collapse the rect-based test result into a single target at the specified
  // location.
  HitTestLocation ResolveRectBasedTest(
      Node* resolved_inner_node,
      const PhysicalOffset& resolved_point_in_main_frame);

 private:
  NodeSet& MutableListBasedTestResult();  // See above.
  HTMLMediaElement* MediaElement() const;

  HitTestRequest hit_test_request_;
  bool cacheable_;

  Member<Node> inner_node_;
  Member<Node> inert_node_;
  // This gets calculated in the first call to InnerElement function.
  Member<Element> inner_element_;
  Member<Node> inner_possibly_pseudo_node_;
  // FIXME: Nothing changes this to a value different from m_hitTestLocation!
  // The hit-tested point in innerNode frame coordinates.
  PhysicalOffset point_in_inner_node_frame_;
  // A point in the local coordinate space of m_innerNode's layoutObject.Allows
  // us to efficiently determine where inside the layoutObject we hit on
  // subsequent operations.
  PhysicalOffset local_point_;
  // For non-URL, this is the enclosing that triggers navigation.
  Member<Element> inner_url_element_;
  Member<Scrollbar> scrollbar_;
  // Returns true if we are over a EmbeddedContentView (and not in the
  // border/padding area of a LayoutEmbeddedContent for example).
  bool is_over_embedded_content_view_;

  mutable Member<NodeSet> list_based_test_result_;
  String canvas_region_id_;
};

}  // namespace blink

WTF_ALLOW_CLEAR_UNUSED_SLOTS_WITH_MEM_FUNCTIONS(blink::HitTestResult)

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_HIT_TEST_RESULT_H_
