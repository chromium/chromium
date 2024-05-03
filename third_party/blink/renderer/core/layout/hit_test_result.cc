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

#include "cc/base/region.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_utilities.h"
#include "third_party/blink/renderer/core/dom/flat_tree_traversal.h"
#include "third_party/blink/renderer/core/dom/pseudo_element.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/position_with_affinity.h"
#include "third_party/blink/renderer/core/editing/text_affinity.h"
#include "third_party/blink/renderer/core/editing/visible_units.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/forms/html_text_area_element.h"
#include "third_party/blink/renderer/core/html/html_area_element.h"
#include "third_party/blink/renderer/core/html/html_embed_element.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/html/html_map_element.h"
#include "third_party/blink/renderer/core/html/html_object_element.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/core/html/media/media_source_handle.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/input_type_names.h"
#include "third_party/blink/renderer/core/layout/hit_test_location.h"
#include "third_party/blink/renderer/core/layout/layout_block.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_content.h"
#include "third_party/blink/renderer/core/layout/layout_image.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_image.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/scrolling/top_document_root_scroller_controller.h"
#include "third_party/blink/renderer/core/scroll/scrollbar.h"
#include "third_party/blink/renderer/core/svg/svg_element.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_descriptor.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace blink {

using mojom::blink::FormControlType;

namespace {

bool HasImageSourceURL(const Node& node) {
  // Always return a url for image elements and input elements with type=image,
  // even if they don't have a LayoutImage (e.g. because the image didn't load
  // and we are using an alt container). For other elements we don't create alt
  // containers so ensure they contain a loaded image.
  auto* html_input_element = DynamicTo<HTMLInputElement>(node);
  if (IsA<HTMLImageElement>(node) ||
      (html_input_element &&
       html_input_element->FormControlType() == FormControlType::kInputImage)) {
    return true;
  }
  const LayoutObject* layout_object = node.GetLayoutObject();
  if (!layout_object) {
    return false;
  }
  if (layout_object->IsImage() &&
      (IsA<HTMLEmbedElement>(node) || IsA<HTMLObjectElement>(node))) {
    return true;
  }
  if (layout_object->IsSVGImage()) {
    return true;
  }
  return false;
}

}  // namespace

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
      inner_element_(other.InnerElement()),
      inner_possibly_pseudo_node_(other.inner_possibly_pseudo_node_),
      point_in_inner_node_frame_(other.point_in_inner_node_frame_),
      local_point_(other.LocalPoint()),
      inner_url_element_(other.URLElement()),
      scrollbar_(other.GetScrollbar()),
      is_over_embedded_content_view_(other.IsOverEmbeddedContentView()),
      is_over_resizer_(other.is_over_resizer_),
      is_over_scroll_corner_(other.is_over_scroll_corner_) {
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
         inner_node_ == other.InnerNode() &&
         inner_element_ == other.InnerElement() &&
         inner_possibly_pseudo_node_ == other.InnerPossiblyPseudoNode() &&
         point_in_inner_node_frame_ == other.point_in_inner_node_frame_ &&
         local_point_ == other.LocalPoint() &&
         inner_url_element_ == other.URLElement() &&
         scrollbar_ == other.GetScrollbar() &&
         is_over_embedded_content_view_ == other.IsOverEmbeddedContentView();
}

void HitTestResult::CacheValues(const HitTestResult& other) {
  hit_test_request_ =
      other.hit_test_request_.GetType() & ~HitTestRequest::kAvoidCache;
}

void HitTestResult::PopulateFromCachedResult(const HitTestResult& other) {
  inner_node_ = other.InnerNode();
  inner_element_ = other.InnerElement();
  inner_possibly_pseudo_node_ = other.InnerPossiblyPseudoNode();
  point_in_inner_node_frame_ = other.point_in_inner_node_frame_;
  local_point_ = other.LocalPoint();
  inner_url_element_ = other.URLElement();
  scrollbar_ = other.GetScrollbar();

  is_over_embedded_content_view_ = other.IsOverEmbeddedContentView();
  cacheable_ = other.cacheable_;
  is_over_resizer_ = other.IsOverResizer();
  is_over_scroll_corner_ = other.IsOverScrollCorner();

  // Only copy the NodeSet in case of list hit test.
  list_based_test_result_ =
      other.list_based_test_result_
          ? MakeGarbageCollected<NodeSet>(*other.list_based_test_result_)
          : nullptr;
}

void HitTestResult::Trace(Visitor* visitor) const {
  visitor->Trace(hit_test_request_);
  visitor->Trace(inner_node_);
  visitor->Trace(inner_element_);
  visitor->Trace(inner_possibly_pseudo_node_);
  visitor->Trace(inner_url_element_);
  visitor->Trace(scrollbar_);
  visitor->Trace(list_based_test_result_);
}

void HitTestResult::SetNodeAndPosition(Node* node,
                                       const PhysicalBoxFragment* box_fragment,
                                       const PhysicalOffset& position) {
  if (box_fragment) {
    local_point_ = position + box_fragment->OffsetFromOwnerLayoutBox();
  } else {
    local_point_ = position;
  }
  SetInnerNode(node);
}

void HitTestResult::OverrideNodeAndPosition(Node* node,
                                            PhysicalOffset position) {
  local_point_ = position;
  SetInnerNode(node);
}

PositionWithAffinity HitTestResult::GetPosition() const {
  const Node* node = inner_possibly_pseudo_node_;
  if (!node)
    return PositionWithAffinity();
  // |LayoutObject::PositionForPoint()| requires |kPrePaintClean|.
  DCHECK_GE(node->GetDocument().Lifecycle().GetState(),
            DocumentLifecycle::kPrePaintClean);
  LayoutObject* layout_object = node->GetLayoutObject();
  if (!layout_object)
    return PositionWithAffinity();

  // We should never have a layout object that is within a locked subtree.
  CHECK(!DisplayLockUtilities::LockedAncestorPreventingPaint(*layout_object));

  // If the layout object is blocked by display lock, we return the beginning of
  // the node as the position. This is because we don't paint contents of the
  // element. Furthermore, any caret adjustments below can access layout-dirty
  // state in the subtree of this object.
  if (layout_object->ChildPaintBlockedByDisplayLock())
    return PositionWithAffinity(Position(*node, 0), TextAffinity::kDefault);

  if (node->IsPseudoElement() && node->GetPseudoId() == kPseudoIdBefore) {
    return PositionWithAffinity(
        MostForwardCaretPosition(Position::FirstPositionInNode(*inner_node_)));
  }

  return layout_object->PositionForPoint(LocalPoint());
}

PositionWithAffinity HitTestResult::GetPositionForInnerNodeOrImageMapImage()
    const {
  Node* node = InnerPossiblyPseudoNode();
  if (node && !node->IsPseudoElement())
    node = InnerNodeOrImageMapImage();
  if (!node)
    return PositionWithAffinity();
  // |LayoutObject::PositionForPoint()| requires |kPrePaintClean|.
  DCHECK_GE(node->GetDocument().Lifecycle().GetState(),
            DocumentLifecycle::kPrePaintClean);
  LayoutObject* layout_object = node->GetLayoutObject();
  if (!layout_object)
    return PositionWithAffinity();
  // We should never have a layout object that is within a locked subtree.
  CHECK(!DisplayLockUtilities::LockedAncestorPreventingPaint(*layout_object));

  // If the layout object is blocked by display lock, we return the beginning of
  // the node as the position. This is because we don't paint contents of the
  // element. Furthermore, any caret adjustments below can access layout-dirty
  // state in the subtree of this object.
  if (layout_object->ChildPaintBlockedByDisplayLock())
    return PositionWithAffinity(Position(*node, 0), TextAffinity::kDefault);

  PositionWithAffinity position = layout_object->PositionForPoint(LocalPoint());
  if (position.IsNull())
    return PositionWithAffinity(FirstPositionInOrBeforeNode(*node));
  return position;
}

void HitTestResult::SetToShadowHostIfInUAShadowRoot() {
  Node* node = InnerNode();
  if (!node)
    return;

  ShadowRoot* containing_shadow_root = node->ContainingShadowRoot();
  Element* shadow_host = nullptr;

  // Consider a closed shadow tree of SVG's <use> element as a special
  // case so that a toolip title in the shadow tree works.
  while (containing_shadow_root && containing_shadow_root->IsUserAgent()) {
    shadow_host = &containing_shadow_root->host();
    containing_shadow_root = shadow_host->ContainingShadowRoot();
    // TODO(layout-dev): Not updating local_point_ here seems like a mistake?
    OverrideNodeAndPosition(node->OwnerShadowHost(), local_point_);
  }

  // TODO(layout-dev): Not updating local_point_ here seems like a mistake?
  if (shadow_host)
    OverrideNodeAndPosition(shadow_host, local_point_);
}

CompositorElementId HitTestResult::GetScrollableContainer() const {
  // If no node was found, return an invalid element ID, which we check for in
  // InputHandlerProxy::ContinueScrollBeginAfterMainThreadHitTest.
  if (!InnerNode())
    return CompositorElementId();

  LayoutBox* cur_box = InnerNode()->GetLayoutObject()->EnclosingBox();

  // Scrolling propagates along the containing block chain and ends at the
  // RootScroller node. The RootScroller node will have a custom applyScroll
  // callback that performs scrolling as well as associated "root" actions like
  // browser control movement and overscroll glow.
  while (cur_box) {
    if (cur_box->IsGlobalRootScroller() ||
        (cur_box->IsScrollContainer() &&
         cur_box->GetScrollableArea()->ScrollsOverflow())) {
      return cur_box->GetScrollableArea()->GetScrollElementId();
    }

    if (IsA<LayoutView>(cur_box))
      cur_box = cur_box->GetFrame()->OwnerLayoutObject();
    else
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
    return;
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
  return GetImage(InnerNodeOrImageMapImage());
}

Image* HitTestResult::GetImage(const Node* node) {
  if (!node) {
    return nullptr;
  }
  const LayoutObject* layout_object = node->GetLayoutObject();
  if (!layout_object) {
    return nullptr;
  }
  const LayoutImageResource* layout_image_resource = nullptr;
  if (layout_object->IsImage()) {
    layout_image_resource = To<LayoutImage>(layout_object)->ImageResource();
  } else if (auto* svg_image = DynamicTo<LayoutSVGImage>(layout_object)) {
    layout_image_resource = svg_image->ImageResource();
  }
  const ImageResourceContent* image_content =
      layout_image_resource ? layout_image_resource->CachedImage() : nullptr;
  if (image_content && !image_content->ErrorOccurred()) {
    return image_content->GetImage();
  }
  return nullptr;
}

gfx::Rect HitTestResult::ImageRect() const {
  if (!GetImage())
    return gfx::Rect();
  return gfx::ToEnclosingRect(InnerNodeOrImageMapImage()
                                  ->GetLayoutBox()
                                  ->AbsoluteContentQuad()
                                  .BoundingBox());
}

KURL HitTestResult::AbsoluteImageURL(const Node* node) {
  if (!node || !HasImageSourceURL(*node)) {
    return KURL();
  }
  AtomicString url_string = To<Element>(*node).ImageSourceURL();
  if (url_string.empty()) {
    return KURL();
  }
  return node->GetDocument().CompleteURL(
      StripLeadingAndTrailingHTMLSpaces(url_string));
}

KURL HitTestResult::AbsoluteImageURL() const {
  return AbsoluteImageURL(InnerNodeOrImageMapImage());
}

KURL HitTestResult::AbsoluteMediaURL() const {
  if (HTMLMediaElement* media_elt = MediaElement())
    return media_elt->currentSrc();
  return KURL();
}

MediaStreamDescriptor* HitTestResult::GetMediaStreamDescriptor() const {
  if (HTMLMediaElement* media_elt = MediaElement()) {
    auto variant = media_elt->GetSrcObjectVariant();
    if (absl::holds_alternative<MediaStreamDescriptor*>(variant)) {
      // It might be nullptr-valued variant, too, here, but we return nullptr
      // for that, regardless.
      return absl::get<MediaStreamDescriptor*>(variant);
    }
  }
  return nullptr;
}

MediaSourceHandle* HitTestResult::GetMediaSourceHandle() const {
  if (HTMLMediaElement* media_elt = MediaElement()) {
    auto variant = media_elt->GetSrcObjectVariant();
    if (absl::holds_alternative<MediaSourceHandle*>(variant)) {
      // It might be a nullptr-valued MediaStreamDescriptor* variant, here, but
      // we return nullptr for that, regardless.
      return absl::get<MediaSourceHandle*>(variant);
    }
  }
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

  return IsEditable(*inner_node_);
}

std::tuple<bool, ListBasedHitTestBehavior>
HitTestResult::AddNodeToListBasedTestResultInternal(
    Node* node,
    const HitTestLocation& location) {
  // If not a list-based test, stop testing because the hit has been found.
  if (!GetHitTestRequest().ListBased())
    return std::make_tuple(false, kStopHitTesting);

  if (!node)
    return std::make_tuple(false, kContinueHitTesting);

  MutableListBasedTestResult().insert(node);
  if (GetHitTestRequest().PenetratingList()) {
    ListBasedHitTestBehavior behavior = kContinueHitTesting;
    if (GetHitTestRequest().UseHitNodeCb()) {
      LocalFrameView::InvalidationDisallowedScope invalidation_disallowed(
          *node->GetDocument().View());
      behavior = GetHitTestRequest().RunHitNodeCb(*node);
    }
    return std::make_tuple(false, behavior);
  }

  // The second argument will be ignored.
  return std::make_tuple(true, kContinueHitTesting);
}

ListBasedHitTestBehavior HitTestResult::AddNodeToListBasedTestResult(
    Node* node,
    const HitTestLocation& location,
    const PhysicalRect& rect) {
  bool should_check_containment;
  ListBasedHitTestBehavior behavior;
  std::tie(should_check_containment, behavior) =
      AddNodeToListBasedTestResultInternal(node, location);
  if (!should_check_containment)
    return behavior;
  return rect.Contains(location.BoundingBox()) ? kStopHitTesting
                                               : kContinueHitTesting;
}

ListBasedHitTestBehavior HitTestResult::AddNodeToListBasedTestResult(
    Node* node,
    const HitTestLocation& location,
    const gfx::QuadF& quad) {
  bool should_check_containment;
  ListBasedHitTestBehavior behavior;
  std::tie(should_check_containment, behavior) =
      AddNodeToListBasedTestResultInternal(node, location);
  if (!should_check_containment)
    return behavior;
  return quad.ContainsQuad(gfx::QuadF(gfx::RectF(location.BoundingBox())))
             ? kStopHitTesting
             : kContinueHitTesting;
}

ListBasedHitTestBehavior HitTestResult::AddNodeToListBasedTestResult(
    Node* node,
    const HitTestLocation& location,
    const cc::Region& region) {
  bool should_check_containment;
  ListBasedHitTestBehavior behavior;
  std::tie(should_check_containment, behavior) =
      AddNodeToListBasedTestResultInternal(node, location);
  if (!should_check_containment)
    return behavior;
  return region.Contains(location.ToEnclosingRect()) ? kStopHitTesting
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
    is_over_resizer_ = other.IsOverResizer();
    is_over_scroll_corner_ = other.is_over_scroll_corner_;
  }

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
