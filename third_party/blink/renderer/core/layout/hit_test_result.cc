/*
 * Copyright (C) 2006, 2008, 2011 Apple Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/layout/hit_test_result.h"

#include "third_party/blink/renderer/core/dom/flat_tree_traversal.h"
#include "third_party/blink/renderer/core/dom/pseudo_element.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/position_with_affinity.h"
#include "third_party/blink/renderer/core/editing/visible_units.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/forms/html_text_area_element.h"
#include "third_party/blink/renderer/core/html/html_area_element.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/html/html_map_element.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/input_type_names.h"
#include "third_party/blink/renderer/core/layout/layout_block.h"
#include "third_party/blink/renderer/core/layout/layout_image.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/scrolling/top_document_root_scroller_controller.h"
#include "third_party/blink/renderer/core/scroll/scrollbar.h"
#include "third_party/blink/renderer/core/svg/svg_element.h"
#include "third_party/blink/renderer/platform/geometry/region.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_descriptor.h"

namespace blink {

HitTestResult::HitTestResult()
    : hit_test_request_(HitTestRequest::kReadOnly | HitTestRequest::kActive),
      cacheable_(true),
      is_over_embedded_content_view_(false) {}

HitTestResult::HitTestResult(const HitTestRequest& other_request,
                             const HitTestLocation& location)
    : hit_test_request_(other_request),
      cacheable_(true),
      point_in_inner_node_frame_(location.Point()),
      is_over_embedded_content_view_(false) {}

HitTestResult::HitTestResult(const HitTestResult& other)
    : hit_test_request_(other.hit_test_request_),
      cacheable_(other.cacheable_),
      inner_node_(other.InnerNode()),
      inert_node_(other.InertNode()),
      inner_element_(other.InnerElement()),
      inner_possibly_pseudo_node_(other.inner_possibly_pseudo_node_),
      point_in_inner_node_frame_(other.point_in_inner_node_frame_),
      local_point_(other.LocalPoint()),
      inner_url_element_(other.URLElement()),
      scrollbar_(other.GetScrollbar()),
      box_fragment_(other.box_fragment_),
      is_over_embedded_content_view_(other.IsOverEmbeddedContentView()),
      canvas_region_id_(other.CanvasRegionId()) {
  // Only copy the NodeSet in case of list hit test.
  list_based_test_result_ =
      other.list_based_test_result_
          ? MakeGarbageCollected<NodeSet>(*other.list_based_test_result_)
          : nullptr;
}

HitTestResult::~HitTestResult() = default;

HitTestResult& HitTestResult::operator=(const HitTestResult& other) {
  hit_test_request_ = other.hit_test_request_;
  PopulateFromCachedResult(other);

  return *this;
}

bool HitTestResult::EqualForCacheability(const HitTestResult& other) const {
  return hit_test_request_.EqualForCacheability(other.hit_test_request_) &&
         inner_node_ == other.InnerNode() && inert_node_ == other.InertNode() &&
         inner_element_ == other.InnerElement() &&
         inner_possibly_pseudo_node_ == other.InnerPossiblyPseudoNode() &&
         point_in_inner_node_frame_ == other.point_in_inner_node_frame_ &&
         local_point_ == other.LocalPoint() &&
         inner_url_element_ == other.URLElement() &&
         scrollbar_ == other.GetScrollbar() &&
         box_fragment_ == other.box_fragment_ &&
         is_over_embedded_content_view_ == other.IsOverEmbeddedContentView();
}

void HitTestResult::CacheValues(const HitTestResult& other) {
  hit_test_request_ =
      other.hit_test_request_.GetType() & ~HitTestRequest::kAvoidCache;
}

void HitTestResult::PopulateFromCachedResult(const HitTestResult& other) {
  inner_node_ = other.InnerNode();
  inert_node_ = other.InertNode();
  inner_element_ = other.InnerElement();
  inner_possibly_pseudo_node_ = other.InnerPossiblyPseudoNode();
  point_in_inner_node_frame_ = other.point_in_inner_node_frame_;
  local_point_ = other.LocalPoint();
  inner_url_element_ = other.URLElement();
  scrollbar_ = other.GetScrollbar();
  box_fragment_ = other.box_fragment_;

  is_over_embedded_content_view_ = other.IsOverEmbeddedContentView();
  cacheable_ = other.cacheable_;
  canvas_region_id_ = other.CanvasRegionId();

  // Only copy the NodeSet in case of list hit test.
  list_based_test_result_ =
      other.list_based_test_result_
          ? MakeGarbageCollected<NodeSet>(*other.list_based_test_result_)
          : nullptr;
}

void HitTestResult::Trace(Visitor* visitor) const {
  visitor->Trace(inner_node_);
  visitor->Trace(inert_node_);
  visitor->Trace(inner_element_);
  visitor->Trace(inner_possibly_pseudo_node_);
  visitor->Trace(inner_url_element_);
  visitor->Trace(scrollbar_);
  visitor->Trace(list_based_test_result_);
}

void HitTestResult::SetNodeAndPosition(
    Node* node,
    scoped_refptr<const NGPhysicalBoxFragment> box_fragment,
    const PhysicalOffset& position) {
  SetBoxFragment(std::move(box_fragment));
  SetNodeAndPosition(node, position);
}

void HitTestResult::SetBoxFragment(
    scoped_refptr<const NGPhysicalBoxFragment> box_fragment) {
  DCHECK(!box_fragment || !box_fragment->IsInlineBox());
  box_fragment_ = std::move(box_fragment);
}

PositionWithAffinity HitTestResult::GetPosition() const {
  if (!inner_possibly_pseudo_node_)
    return PositionWithAffinity();
  LayoutObject* layout_object = GetLayoutObject();
  if (!layout_object)
    return PositionWithAffinity();
  if (inner_possibly_pseudo_node_->IsPseudoElement() &&
      inner_possibly_pseudo_node_->GetPseudoId() == kPseudoIdBefore) {
    return PositionWithAffinity(MostForwardCaretPosition(
        Position(inner_node_, PositionAnchorType::kBeforeChildren)));
  }
  if (box_fragment_ && NGPhysicalBoxFragment::SupportsPositionForPoint())
    return box_fragment_->PositionForPoint(LocalPoint());
  return layout_object->PositionForPoint(LocalPoint());
}

PositionWithAffinity HitTestResult::GetPositionForInnerNodeOrImageMapImage()
    const {
  Node* node = InnerPossiblyPseudoNode();
  if (node && !node->IsPseudoElement())
    node = InnerNodeOrImageMapImage();
  if (!node)
    return PositionWithAffinity();
  LayoutObject* layout_object = node->GetLayoutObject();
  if (!layout_object)
    return PositionWithAffinity();
  PositionWithAffinity position;
  if (box_fragment_ && NGPhysicalBoxFragment::SupportsPositionForPoint() &&
      layout_object == GetLayoutObject())
    position = box_fragment_->PositionForPoint(LocalPoint());
  else
    position = layout_object->PositionForPoint(LocalPoint());
  if (position.IsNull())
    return PositionWithAffinity(FirstPositionInOrBeforeNode(*node));
  return position;
}

LayoutObject* HitTestResult::GetLayoutObject() const {
  return inner_node_ ? inner_node_->GetLayoutObject() : nullptr;
}

void HitTestResult::SetToShadowHostIfInRestrictedShadowRoot() {
  Node* node = InnerNode();
  if (!node)
    return;

  ShadowRoot* containing_shadow_root = node->ContainingShadowRoot();
  Element* shadow_host = nullptr;

  // Consider a closed shadow tree of SVG's <use> element as a special
  // case so that a toolip title in the shadow tree works.
  while (containing_shadow_root &&
         (containing_shadow_root->IsUserAgent() ||
          IsA<SVGUseElement>(containing_shadow_root->host()))) {
    shadow_host = &containing_shadow_root->host();
    containing_shadow_root = shadow_host->ContainingShadowRoot();
    SetInnerNode(node->OwnerShadowHost());
  }

  if (shadow_host)
    SetInnerNode(shadow_host);
}

CompositorElementId HitTestResult::GetScrollableContainer() const {
  DCHECK(InnerNode());
  LayoutBox* cur_box = InnerNode()->GetLayoutObject()->EnclosingBox();

  // Scrolling propagates along the containing block chain and ends at the
  // RootScroller node. The RootScroller node will have a custom applyScroll
  // callback that performs scrolling as well as associated "root" actions like
  // browser control movement and overscroll glow.
  while (cur_box) {
    if (cur_box->IsGlobalRootScroller() ||
        cur_box->NeedsScrollNode(CompositingReason::kNone)) {
      return CompositorElementIdFromUniqueObjectId(
          cur_box->UniqueId(), CompositorElementIdNamespace::kScroll);
    }

    cur_box = cur_box->ContainingBlock();
  }

  return InnerNode()
      ->GetDocument()
      .GetPage()
      ->GetVisualViewport()
      .GetScrollElementId();
}

HTMLAreaElement* HitTestResult::ImageAreaForImage() const {
  DCHECK(inner_node_);
  auto* image_element = DynamicTo<HTMLImageElement>(inner_node_.Get());
  if (!image_element && inner_node_->IsInShadowTree()) {
    if (inner_node_->ContainingShadowRoot()->IsUserAgent()) {
      image_element =
          DynamicTo<HTMLImageElement>(inner_node_->OwnerShadowHost());
    }
  }

  if (!image_element || !image_element->GetLayoutObject() ||
      !image_element->GetLayoutObject()->IsBox())
    return nullptr;

  HTMLMapElement* map = image_element->GetTreeScope().GetImageMap(
      image_element->FastGetAttribute(html_names::kUsemapAttr));
  if (!map)
    return nullptr;

  return map->AreaForPoint(LocalPoint(), image_element->GetLayoutObject());
}

void HitTestResult::SetInnerNode(Node* n) {
  if (!n) {
    inner_possibly_pseudo_node_ = nullptr;
    inner_node_ = nullptr;
    inner_element_ = nullptr;
    box_fragment_ = nullptr;
    return;
  }

  if (RuntimeEnabledFeatures::InertAttributeEnabled()) {
    if (GetHitTestRequest().RetargetForInert()) {
      if (n->IsInert()) {
        if (!inert_node_)
          inert_node_ = n;

        return;
      }

      if (inert_node_ && n != inert_node_ &&
          !n->IsShadowIncludingInclusiveAncestorOf(*inert_node_)) {
        return;
      }
    }
  }

  if (NGPhysicalBoxFragment::SupportsPositionForPoint()) {
    if (const LayoutBox* layout_box = n->GetLayoutBox()) {
      // Fragmentation-aware code will set the correct box fragment on its own,
      // but sometimes we enter legacy layout code when hit-testing, e.g. for
      // replaced content. In such cases we need to set it here.
      if (box_fragment_) {
        DCHECK(!box_fragment_->GetLayoutObject() ||
               layout_box == box_fragment_->GetLayoutObject());
      } else if (layout_box->PhysicalFragmentCount() > 0) {
        // If we set the fragment on our own, make sure that there's only one of
        // them, since there's no way for us to pick the right one here.
        DCHECK_EQ(layout_box->PhysicalFragmentCount(), 1u);
        box_fragment_ = layout_box->GetPhysicalFragment(0);
      }
    }
  }

  inner_possibly_pseudo_node_ = n;
  if (auto* pseudo_element = DynamicTo<PseudoElement>(n))
    n = pseudo_element->InnerNodeForHitTesting();
  inner_node_ = n;
  if (HTMLAreaElement* area = ImageAreaForImage()) {
    inner_node_ = area;
    inner_possibly_pseudo_node_ = area;
  }
  if (auto* element = DynamicTo<Element>(inner_node_.Get()))
    inner_element_ = element;
  else
    inner_element_ = FlatTreeTraversal::ParentElement(*inner_node_);
}

void HitTestResult::SetInertNode(Node* n) {
  // Don't overwrite an existing value for inert_node_
  if (inert_node_)
    DCHECK(n == inert_node_);

  inert_node_ = n;
}

void HitTestResult::SetURLElement(Element* n) {
  inner_url_element_ = n;
}

void HitTestResult::SetScrollbar(Scrollbar* s) {
  scrollbar_ = s;
}

LocalFrame* HitTestResult::InnerNodeFrame() const {
  if (inner_node_)
    return inner_node_->GetDocument().GetFrame();
  return nullptr;
}

bool HitTestResult::IsSelected(const HitTestLocation& location) const {
  if (!inner_node_)
    return false;

  if (LocalFrame* frame = inner_node_->GetDocument().GetFrame())
    return frame->Selection().Contains(location.Point());
  return false;
}

String HitTestResult::Title(TextDirection& dir) const {
  dir = TextDirection::kLtr;
  // Find the title in the nearest enclosing DOM node.
  // For <area> tags in image maps, walk the tree for the <area>, not the <img>
  // using it.
  if (inner_node_.Get())
    inner_node_->UpdateDistributionForFlatTreeTraversal();
  for (Node* title_node = inner_node_.Get(); title_node;
       title_node = FlatTreeTraversal::Parent(*title_node)) {
    if (auto* element = DynamicTo<Element>(title_node)) {
      String title = element->title();
      if (!title.IsNull()) {
        if (LayoutObject* layout_object = title_node->GetLayoutObject())
          dir = layout_object->StyleRef().Direction();
        return title;
      }
    }
  }
  return String();
}

const AtomicString& HitTestResult::AltDisplayString() const {
  Node* inner_node_or_image_map_image = InnerNodeOrImageMapImage();
  if (!inner_node_or_image_map_image)
    return g_null_atom;

  if (auto* image = DynamicTo<HTMLImageElement>(*inner_node_or_image_map_image))
    return image->FastGetAttribute(html_names::kAltAttr);

  if (auto* input = DynamicTo<HTMLInputElement>(*inner_node_or_image_map_image))
    return input->Alt();

  return g_null_atom;
}

Image* HitTestResult::GetImage() const {
  Node* inner_node_or_image_map_image = InnerNodeOrImageMapImage();
  if (!inner_node_or_image_map_image)
    return nullptr;

  LayoutObject* layout_object =
      inner_node_or_image_map_image->GetLayoutObject();
  if (layout_object && layout_object->IsImage()) {
    auto* image = To<LayoutImage>(layout_object);
    if (image->CachedImage() && !image->CachedImage()->ErrorOccurred())
      return image->CachedImage()->GetImage();
  }

  return nullptr;
}

IntRect HitTestResult::ImageRect() const {
  if (!GetImage())
    return IntRect();
  return InnerNodeOrImageMapImage()
      ->GetLayoutBox()
      ->AbsoluteContentQuad()
      .EnclosingBoundingBox();
}

KURL HitTestResult::AbsoluteImageURL() const {
  Node* inner_node_or_image_map_image = InnerNodeOrImageMapImage();
  if (!inner_node_or_image_map_image)
    return KURL();

  AtomicString url_string;
  // Always return a url for image elements and input elements with type=image,
  // even if they don't have a LayoutImage (e.g. because the image didn't load
  // and we are using an alt container). For other elements we don't create alt
  // containers so ensure they contain a loaded image.
  auto* html_input_element =
      DynamicTo<HTMLInputElement>(inner_node_or_image_map_image);
  if (IsA<HTMLImageElement>(*inner_node_or_image_map_image) ||
      (html_input_element &&
       html_input_element->type() == input_type_names::kImage))
    url_string = To<Element>(*inner_node_or_image_map_image).ImageSourceURL();
  else if ((inner_node_or_image_map_image->GetLayoutObject() &&
            inner_node_or_image_map_image->GetLayoutObject()->IsImage()) &&
           (IsA<HTMLEmbedElement>(*inner_node_or_image_map_image) ||
            IsA<HTMLObjectElement>(*inner_node_or_image_map_image) ||
            IsA<SVGImageElement>(*inner_node_or_image_map_image)))
    url_string = To<Element>(*inner_node_or_image_map_image).ImageSourceURL();
  if (url_string.IsEmpty())
    return KURL();

  return inner_node_or_image_map_image->GetDocument().CompleteURL(
      StripLeadingAndTrailingHTMLSpaces(url_string));
}

KURL HitTestResult::AbsoluteMediaURL() const {
  if (HTMLMediaElement* media_elt = MediaElement())
    return media_elt->currentSrc();
  return KURL();
}

MediaStreamDescriptor* HitTestResult::GetMediaStreamDescriptor() const {
  if (HTMLMediaElement* media_elt = MediaElement())
    return media_elt->GetSrcObject();
  return nullptr;
}

HTMLMediaElement* HitTestResult::MediaElement() const {
  if (!inner_node_)
    return nullptr;

  if (!(inner_node_->GetLayoutObject() &&
        inner_node_->GetLayoutObject()->IsMedia()))
    return nullptr;

  return DynamicTo<HTMLMediaElement>(*inner_node_);
}

KURL HitTestResult::AbsoluteLinkURL() const {
  if (!inner_url_element_)
    return KURL();
  return inner_url_element_->HrefURL();
}

bool HitTestResult::IsLiveLink() const {
  return inner_url_element_ && inner_url_element_->IsLiveLink();
}

bool HitTestResult::IsOverLink() const {
  return inner_url_element_ && inner_url_element_->IsLink();
}

String HitTestResult::TextContent() const {
  if (!inner_url_element_)
    return String();
  return inner_url_element_->textContent();
}

// FIXME: This function needs a better name and may belong in a different class.
// It's not really isContentEditable(); it's more like needsEditingContextMenu.
// In many ways, this function would make more sense in the ContextMenu class,
// except that WebElementDictionary hooks into it. Anyway, we should architect
// this better.
bool HitTestResult::IsContentEditable() const {
  if (!inner_node_)
    return false;

  if (auto* textarea = DynamicTo<HTMLTextAreaElement>(*inner_node_))
    return !textarea->IsDisabledOrReadOnly();

  if (auto* input = DynamicTo<HTMLInputElement>(*inner_node_))
    return !input->IsDisabledOrReadOnly() && input->IsTextField();

  return HasEditableStyle(*inner_node_);
}

ListBasedHitTestBehavior HitTestResult::AddNodeToListBasedTestResult(
    Node* node,
    const HitTestLocation& location,
    const PhysicalRect& rect) {
  // If we are in the process of retargeting for `inert`, continue.
  if (GetHitTestRequest().RetargetForInert() && InertNode() && !InnerNode())
    return kContinueHitTesting;

  // If not a list-based test, stop testing because the hit has been found.
  if (!GetHitTestRequest().ListBased())
    return kStopHitTesting;

  if (!node)
    return kContinueHitTesting;

  MutableListBasedTestResult().insert(node);

  if (GetHitTestRequest().PenetratingList())
    return kContinueHitTesting;

  return rect.Contains(location.BoundingBox()) ? kStopHitTesting
                                               : kContinueHitTesting;
}

ListBasedHitTestBehavior HitTestResult::AddNodeToListBasedTestResult(
    Node* node,
    const HitTestLocation& location,
    const Region& region) {
  // If we are in the process of retargeting for `inert`, continue.
  if (GetHitTestRequest().RetargetForInert() && InertNode() && !InnerNode())
    return kContinueHitTesting;

  // If not a list-based test, stop testing because the hit has been found.
  if (!GetHitTestRequest().ListBased())
    return kStopHitTesting;

  if (!node)
    return kContinueHitTesting;

  MutableListBasedTestResult().insert(node);

  if (GetHitTestRequest().PenetratingList())
    return kContinueHitTesting;

  return region.Contains(location.EnclosingIntRect()) ? kStopHitTesting
                                                      : kContinueHitTesting;
}

void HitTestResult::Append(const HitTestResult& other) {
  DCHECK(GetHitTestRequest().ListBased());

  if (!scrollbar_ && other.GetScrollbar()) {
    SetScrollbar(other.GetScrollbar());
  }

  if (!inner_node_ && other.InnerNode()) {
    inner_node_ = other.InnerNode();
    inner_element_ = other.InnerElement();
    inner_possibly_pseudo_node_ = other.InnerPossiblyPseudoNode();
    local_point_ = other.LocalPoint();
    point_in_inner_node_frame_ = other.point_in_inner_node_frame_;
    inner_url_element_ = other.URLElement();
    is_over_embedded_content_view_ = other.IsOverEmbeddedContentView();
    canvas_region_id_ = other.CanvasRegionId();
  }

  if (!inert_node_ && other.InertNode())
    SetInertNode(other.InertNode());

  if (other.list_based_test_result_) {
    NodeSet& set = MutableListBasedTestResult();
    for (NodeSet::const_iterator it = other.list_based_test_result_->begin(),
                                 last = other.list_based_test_result_->end();
         it != last; ++it)
      set.insert(it->Get());
  }
}

const HitTestResult::NodeSet& HitTestResult::ListBasedTestResult() const {
  if (!list_based_test_result_)
    list_based_test_result_ = MakeGarbageCollected<NodeSet>();
  return *list_based_test_result_;
}

HitTestResult::NodeSet& HitTestResult::MutableListBasedTestResult() {
  if (!list_based_test_result_)
    list_based_test_result_ = MakeGarbageCollected<NodeSet>();
  return *list_based_test_result_;
}

HitTestLocation HitTestResult::ResolveRectBasedTest(
    Node* resolved_inner_node,
    const PhysicalOffset& resolved_point_in_main_frame) {
  point_in_inner_node_frame_ = resolved_point_in_main_frame;
  SetInnerNode(nullptr);
  list_based_test_result_ = nullptr;

  // Update the HitTestResult as if the supplied node had been hit in normal
  // point-based hit-test.
  // Note that we don't know the local point after a rect-based hit-test, but we
  // never use it so shouldn't bother with the cost of computing it.
  DCHECK(resolved_inner_node);
  if (auto* layout_object = resolved_inner_node->GetLayoutObject())
    layout_object->UpdateHitTestResult(*this, PhysicalOffset());

  return HitTestLocation(resolved_point_in_main_frame);
}

Node* HitTestResult::InnerNodeOrImageMapImage() const {
  if (!inner_node_)
    return nullptr;

  HTMLImageElement* image_map_image_element = nullptr;
  if (auto* area = DynamicTo<HTMLAreaElement>(inner_node_.Get()))
    image_map_image_element = area->ImageElement();
  else if (auto* map = DynamicTo<HTMLMapElement>(inner_node_.Get()))
    image_map_image_element = map->ImageElement();

  if (!image_map_image_element)
    return inner_node_.Get();

  return image_map_image_element;
}

}  // namespace blink
