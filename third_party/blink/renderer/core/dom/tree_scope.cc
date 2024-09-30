/*
 * Copyright (C) 2011 Google Inc. All Rights Reserved.
 * Copyright (C) 2012 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/dom/tree_scope.h"

#include "third_party/blink/renderer/core/animation/document_animations.h"
#include "third_party/blink/renderer/core/css/resolver/scoped_style_resolver.h"
#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/container_node.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/events/event_path.h"
#include "third_party/blink/renderer/core/dom/id_target_observer_registry.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/scroll_marker_group_pseudo_element.h"
#include "third_party/blink/renderer/core/dom/scroll_marker_pseudo_element.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/dom/tree_scope_adopter.h"
#include "third_party/blink/renderer/core/editing/dom_selection.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/picture_in_picture_controller.h"
#include "third_party/blink/renderer/core/fullscreen/fullscreen.h"
#include "third_party/blink/renderer/core/html/html_anchor_element.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/html/html_map_element.h"
#include "third_party/blink/renderer/core/html/html_slot_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/layout/hit_test_location.h"
#include "third_party/blink/renderer/core/layout/hit_test_result.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/svg/svg_text_content_element.h"
#include "third_party/blink/renderer/core/svg/svg_tree_scope_resources.h"
#include "third_party/blink/renderer/platform/bindings/script_forbidden_scope.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "ui/gfx/geometry/point_conversions.h"

namespace blink {

TreeScope::TreeScope(ContainerNode& root_node, Document& document)
    : document_(&document),
      root_node_(&root_node),
      parent_tree_scope_(&document) {
  DCHECK_NE(root_node, document);
  root_node_->SetTreeScope(this);
}

TreeScope::TreeScope(Document& document)
    : document_(&document), root_node_(document) {
  root_node_->SetTreeScope(this);
}

TreeScope::~TreeScope() = default;

bool TreeScope::IsInclusiveAncestorTreeScopeOf(const TreeScope& scope) const {
  for (const TreeScope* current = &scope; current;
       current = current->ParentTreeScope()) {
    if (current == this)
      return true;
  }
  return false;
}

void TreeScope::SetParentTreeScope(TreeScope& new_parent_scope) {
  // A document node cannot be re-parented.
  DCHECK(!RootNode().IsDocumentNode());

  parent_tree_scope_ = &new_parent_scope;
  SetDocument(new_parent_scope.GetDocument());
}

ScopedStyleResolver& TreeScope::EnsureScopedStyleResolver() {
  CHECK(this);
  if (!scoped_style_resolver_)
    scoped_style_resolver_ = MakeGarbageCollected<ScopedStyleResolver>(*this);
  return *scoped_style_resolver_;
}

void TreeScope::ClearScopedStyleResolver() {
  if (scoped_style_resolver_)
    scoped_style_resolver_->ResetStyle();
  scoped_style_resolver_.Clear();
}

Element* TreeScope::getElementById(const AtomicString& element_id) const {
  if (element_id.empty())
    return nullptr;
  if (!elements_by_id_)
    return nullptr;
  return elements_by_id_->GetElementById(element_id, *this);
}

const HeapVector<Member<Element>>& TreeScope::GetAllElementsById(
    const AtomicString& element_id) const {
  DEFINE_STATIC_LOCAL(Persistent<HeapVector<Member<Element>>>, empty_vector,
                      (MakeGarbageCollected<HeapVector<Member<Element>>>()));
  if (element_id.empty())
    return *empty_vector;
  if (!elements_by_id_)
    return *empty_vector;
  return elements_by_id_->GetAllElementsById(element_id, *this);
}

void TreeScope::AddElementById(const AtomicString& element_id,
                               Element& element) {
  if (!elements_by_id_) {
    elements_by_id_ = MakeGarbageCollected<TreeOrderedMap>();
  }
  elements_by_id_->Add(element_id, element);
  if (id_target_observer_registry_) {
    id_target_observer_registry_->NotifyObservers(element_id);
  }
}

void TreeScope::RemoveElementById(const AtomicString& element_id,
                                  Element& element) {
  if (!elements_by_id_) {
    return;
  }
  elements_by_id_->Remove(element_id, element);
  if (id_target_observer_registry_) {
    id_target_observer_registry_->NotifyObservers(element_id);
  }
}

Node* TreeScope::AncestorInThisScope(Node* node) const {
  while (node) {
    if (node->GetTreeScope() == this)
      return node;
    if (!node->IsInShadowTree())
      return nullptr;

    node = node->OwnerShadowHost();
  }

  return nullptr;
}

void TreeScope::AddImageMap(HTMLMapElement& image_map) {
  const AtomicString& name = image_map.GetName();
  const AtomicString& id = image_map.GetIdAttribute();
  if (!name && !id) {
    return;
  }
  if (!image_maps_by_name_)
    image_maps_by_name_ = MakeGarbageCollected<TreeOrderedMap>();
  if (name)
    image_maps_by_name_->Add(name, image_map);
  if (id) {
    image_maps_by_name_->Add(id, image_map);
  }
}

void TreeScope::RemoveImageMap(HTMLMapElement& image_map) {
  if (!image_maps_by_name_)
    return;
  if (const AtomicString& name = image_map.GetName())
    image_maps_by_name_->Remove(name, image_map);
  if (const AtomicString& id = image_map.GetIdAttribute()) {
    image_maps_by_name_->Remove(id, image_map);
  }
}

HTMLMapElement* TreeScope::GetImageMap(const String& url) const {
  if (url.IsNull())
    return nullptr;
  if (!image_maps_by_name_)
    return nullptr;
  wtf_size_t hash_pos = url.find('#');
  if (hash_pos == kNotFound)
    return nullptr;
  String name = url.Substring(hash_pos + 1);
  if (name.empty()) {
    return nullptr;
  }
  return To<HTMLMapElement>(
      image_maps_by_name_->GetElementByMapName(AtomicString(name), *this));
}

// If the point is not in the viewport, returns false. Otherwise, adjusts the
// point to account for the frame's zoom and scroll.
static bool PointInFrameContentIfVisible(Document& document,
                                         gfx::PointF& point_in_frame) {
  LocalFrame* frame = document.GetFrame();
  if (!frame)
    return false;
  LocalFrameView* frame_view = frame->View();
  if (!frame_view)
    return false;

  // The VisibleContentRect check below requires that scrollbars are up-to-date.
  document.UpdateStyleAndLayout(DocumentUpdateReason::kHitTest);

  auto* scrollable_area = frame_view->LayoutViewport();
  gfx::Rect visible_frame_rect(scrollable_area->VisibleContentRect().size());
  visible_frame_rect = gfx::ScaleToRoundedRect(visible_frame_rect,
                                               1 / frame->LayoutZoomFactor());
  if (!visible_frame_rect.Contains(gfx::ToRoundedPoint(point_in_frame)))
    return false;

  point_in_frame.Scale(frame->LayoutZoomFactor());
  return true;
}

HitTestResult HitTestInDocument(Document* document,
                                double x,
                                double y,
                                const HitTestRequest& request) {
  if (!document->IsActive())
    return HitTestResult();

  gfx::PointF hit_point(x, y);
  if (!PointInFrameContentIfVisible(*document, hit_point))
    return HitTestResult();

  HitTestLocation location(hit_point);
  HitTestResult result(request, location);
  document->GetLayoutView()->HitTest(location, result);
  return result;
}

Element* TreeScope::ElementFromPoint(double x, double y) const {
  return HitTestPoint(x, y,
                      HitTestRequest::kReadOnly | HitTestRequest::kActive);
}

Element* TreeScope::HitTestPoint(double x,
                                 double y,
                                 const HitTestRequest& request) const {
  HitTestResult result =
      HitTestInDocument(&RootNode().GetDocument(), x, y, request);
  if (request.AllowsChildFrameContent()) {
    return HitTestPointInternal(result.InnerNode(),
                                HitTestPointType::kInternal);
  }
  return HitTestPointInternal(result.InnerNode(),
                              HitTestPointType::kWebExposed);
}

Element* TreeScope::HitTestPointInternal(Node* node,
                                         HitTestPointType type) const {
  if (!node || node->IsDocumentNode())
    return nullptr;
  Element* element;
  if ((node->IsPseudoElement() && !node->IsScrollMarkerPseudoElement()) ||
      node->IsTextNode()) {
    element = node->ParentOrShadowHostElement();
  } else {
    element = To<Element>(node);
  }
  if (!element)
    return nullptr;
  if (type == HitTestPointType::kWebExposed)
    return &Retarget(*element);
  return element;
}

static bool ShouldAcceptNonElementNode(const Node& node) {
  Node* parent = node.parentNode();
  if (!parent)
    return false;
  // In some cases the hit test doesn't return slot elements, so we can only
  // get it through its child and can't skip it.
  if (IsA<HTMLSlotElement>(*parent))
    return true;
  // SVG text content elements has no background, and are thus not
  // hit during the background phase of hit-testing. Because of that
  // we need to allow any child (Text) node of these elements.
  return IsA<SVGTextContentElement>(parent);
}

HeapVector<Member<Element>> TreeScope::ElementsFromHitTestResult(
    HitTestResult& result) const {
  HeapVector<Member<Element>> elements;
  Node* last_node = nullptr;
  for (const auto& rect_based_node : result.ListBasedTestResult()) {
    Node* node = rect_based_node.Get();
    if (!node->IsElementNode() && !ShouldAcceptNonElementNode(*node))
      continue;
    node = HitTestPointInternal(node, HitTestPointType::kWebExposed);
    // Prune duplicate entries. A pseduo ::before content above its parent
    // node should only result in a single entry.
    if (node == last_node)
      continue;

    if (auto* element = DynamicTo<Element>(node)) {
      elements.push_back(element);
      last_node = node;
    }
  }
  if (Element* document_element = GetDocument().documentElement()) {
    if (elements.empty() || elements.back() != document_element)
      elements.push_back(document_element);
  }
  return elements;
}

HeapVector<Member<Element>> TreeScope::ElementsFromPoint(double x,
                                                         double y) const {
  Document& document = RootNode().GetDocument();
  gfx::PointF hit_point(x, y);
  if (!PointInFrameContentIfVisible(document, hit_point))
    return HeapVector<Member<Element>>();

  HitTestLocation location(hit_point);
  HitTestRequest request(HitTestRequest::kReadOnly | HitTestRequest::kActive |
                         HitTestRequest::kListBased |
                         HitTestRequest::kPenetratingList);
  HitTestResult result(request, location);
  document.GetLayoutView()->HitTest(location, result);

  return ElementsFromHitTestResult(result);
}

SVGTreeScopeResources& TreeScope::EnsureSVGTreeScopedResources() {
  if (!svg_tree_scoped_resources_) {
    svg_tree_scoped_resources_ =
        MakeGarbageCollected<SVGTreeScopeResources>(this);
  }
  return *svg_tree_scoped_resources_;
}

V8ObservableArrayCSSStyleSheet& TreeScope::EnsureAdoptedStyleSheets() {
  if (!adopted_style_sheets_) [[unlikely]] {
    adopted_style_sheets_ =
        MakeGarbageCollected<V8ObservableArrayCSSStyleSheet>(
            this, &OnAdoptedStyleSheetSet, &OnAdoptedStyleSheetDelete);
  }
  return *adopted_style_sheets_;
}

bool TreeScope::HasAdoptedStyleSheets() const {
  return adopted_style_sheets_ && adopted_style_sheets_->size();
}

void TreeScope::StyleSheetWasAdded(CSSStyleSheet* sheet) {
  GetDocument().GetStyleEngine().AdoptedStyleSheetAdded(*this, sheet);
}

void TreeScope::StyleSheetWasRemoved(CSSStyleSheet* sheet) {
  GetDocument().GetStyleEngine().AdoptedStyleSheetRemoved(*this, sheet);
}

// We pass TreeScope to the bindings array to be informed via set and delete
// callbacks. Bindings doesn't know about DOM types, so we can only pass
// ScriptWrappable (i.e. Document or ShadowRoot) or a GarbageCollectedMixin. We
// choose the mixin as that avoids dispatching from Document back to TreeScope
// essentially implementing a cast. The mixin is passed as void*-like object
// that is only passed back from the observable array into the set/delete
// callbacks where it is again used as TreeScope.
//
// static
void TreeScope::OnAdoptedStyleSheetSet(
    GarbageCollectedMixin* tree_scope,
    ScriptState* script_state,
    V8ObservableArrayCSSStyleSheet& observable_array,
    uint32_t index,
    Member<CSSStyleSheet>& sheet,
    ExceptionState& exception_state) {
  if (!sheet->IsConstructed()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotAllowedError,
        "Can't adopt non-constructed stylesheets.");
    return;
  }
  TreeScope* self = reinterpret_cast<TreeScope*>(tree_scope);
  Document* document = sheet->ConstructorDocument();
  if (document && *document != self->GetDocument()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotAllowedError,
                                      "Sharing constructed stylesheets in "
                                      "multiple documents is not allowed");
    return;
  }
  self->StyleSheetWasAdded(sheet.Get());
}

// See OnAdoptedStyleSheetSet() for description around inner workings.
//
// static
void TreeScope::OnAdoptedStyleSheetDelete(
    GarbageCollectedMixin* tree_scope,
    ScriptState* script_state,
    V8ObservableArrayCSSStyleSheet& observable_array,
    uint32_t index,
    ExceptionState& exception_state) {
  TreeScope* self = reinterpret_cast<TreeScope*>(tree_scope);
  self->StyleSheetWasRemoved(self->adopted_style_sheets_->at(index));
}

void TreeScope::ClearAdoptedStyleSheets() {
  if (!HasAdoptedStyleSheets()) {
    return;
  }
  HeapVector<Member<CSSStyleSheet>> removed;
  removed.AppendRange(adopted_style_sheets_->begin(),
                      adopted_style_sheets_->end());
  adopted_style_sheets_->clear();
  for (auto sheet : removed) {
    StyleSheetWasRemoved(sheet);
  }
}

void TreeScope::SetAdoptedStyleSheetsForTesting(
    HeapVector<Member<CSSStyleSheet>>& adopted_style_sheets) {
  ClearAdoptedStyleSheets();
  EnsureAdoptedStyleSheets();
  for (auto sheet : adopted_style_sheets) {
    DCHECK(sheet->IsConstructed());
    DCHECK_EQ(sheet->ConstructorDocument(), GetDocument());
    adopted_style_sheets_->push_back(sheet);
    StyleSheetWasAdded(sheet);
  }
}

DOMSelection* TreeScope::GetSelection() const {
  if (!RootNode().GetDocument().GetFrame())
    return nullptr;

  if (selection_)
    return selection_.Get();

  // FIXME: The correct selection in Shadow DOM requires that Position can have
  // a ShadowRoot as a container.  See
  // https://bugs.webkit.org/show_bug.cgi?id=82697
  selection_ = MakeGarbageCollected<DOMSelection>(this);
  return selection_.Get();
}

Element* TreeScope::FindAnchorWithName(const String& name) {
  if (name.empty())
    return nullptr;
  if (Element* element = getElementById(AtomicString(name)))
    return element;
  // TODO(crbug.com/369219144): Should this be Traversal<HTMLAnchorElementBase>?
  for (HTMLAnchorElement& anchor :
       Traversal<HTMLAnchorElement>::StartsAfter(RootNode())) {
    if (RootNode().GetDocument().InQuirksMode()) {
      // Quirks mode, case insensitive comparison of names.
      if (DeprecatedEqualIgnoringCase(anchor.GetName(), name))
        return &anchor;
    } else {
      // Strict mode, names need to match exactly.
      if (anchor.GetName() == name)
        return &anchor;
    }
  }
  return nullptr;
}

Node* TreeScope::FindAnchor(const String& fragment) {
  Node* anchor = nullptr;
  // https://html.spec.whatwg.org/C/#the-indicated-part-of-the-document
  // 1. Let fragment be the document's URL's fragment.

  // 2. If fragment is "", top of the document.
  // TODO(1117212) Move empty check to here.

  // 3. Try the raw fragment (for HTML documents; skip it for `svgView()`).
  // TODO(1117212) Remove this 'raw' check, or make it actually 'raw'
  if (!GetDocument().IsSVGDocument()) {
    anchor = FindAnchorWithName(fragment);
    if (anchor)
      return anchor;
  }

  // 4. Let fragmentBytes be the percent-decoded fragment.
  // 5. Let decodedFragment be the UTF-8 decode without BOM of fragmentBytes.
  String name = DecodeURLEscapeSequences(fragment, DecodeURLMode::kUTF8);
  // 6. Try decodedFragment.
  anchor = FindAnchorWithName(name);
  if (anchor)
    return anchor;

  // 7. If decodedFragment is "top", top of the document.
  // TODO(1117212) Move the IsEmpty check to step 2.
  if (fragment.empty() || EqualIgnoringASCIICase(name, "top"))
    anchor = &GetDocument();

  return anchor;
}

void TreeScope::AdoptIfNeeded(Node& node) {
  DCHECK(!node.IsDocumentNode());
  if (&node.GetTreeScope() == this) [[likely]] {
    return;
  }

  // Script is forbidden to protect against event handlers firing in the middle
  // of rescoping in |didMoveToNewDocument| callbacks. See
  // https://crbug.com/605766 and https://crbug.com/606651.
  ScriptForbiddenScope forbid_script;
  TreeScopeAdopter adopter(node, *this);
  if (adopter.NeedsScopeChange())
    adopter.Execute();
}

// This method corresponds to the Retarget algorithm specified in
// https://dom.spec.whatwg.org/#retarget
// This retargets |target| against the root of |this|.
// The steps are different with the spec for performance reasons,
// but the results should be the same.
Element& TreeScope::Retarget(const Element& target) const {
  const TreeScope& target_scope = target.GetTreeScope();
  if (!target_scope.RootNode().IsShadowRoot())
    return const_cast<Element&>(target);

  HeapVector<Member<const TreeScope>> target_ancestor_scopes;
  HeapVector<Member<const TreeScope>> context_ancestor_scopes;
  for (const TreeScope* tree_scope = &target_scope; tree_scope;
       tree_scope = tree_scope->ParentTreeScope())
    target_ancestor_scopes.push_back(tree_scope);
  for (const TreeScope* tree_scope = this; tree_scope;
       tree_scope = tree_scope->ParentTreeScope())
    context_ancestor_scopes.push_back(tree_scope);

  auto target_ancestor_riterator = target_ancestor_scopes.rbegin();
  auto context_ancestor_riterator = context_ancestor_scopes.rbegin();
  while (context_ancestor_riterator != context_ancestor_scopes.rend() &&
         target_ancestor_riterator != target_ancestor_scopes.rend() &&
         *context_ancestor_riterator == *target_ancestor_riterator) {
    ++context_ancestor_riterator;
    ++target_ancestor_riterator;
  }

  if (target_ancestor_riterator == target_ancestor_scopes.rend())
    return const_cast<Element&>(target);
  Node& first_different_scope_root =
      (*target_ancestor_riterator).Get()->RootNode();
  return To<ShadowRoot>(first_different_scope_root).host();
}

Element* TreeScope::AdjustedFocusedElementInternal(
    const Element& target) const {
  for (const Element* ancestor = &target; ancestor;
       ancestor = ancestor->OwnerShadowHost()) {
    if (this == ancestor->GetTreeScope())
      return const_cast<Element*>(ancestor);
  }
  return nullptr;
}

Element* TreeScope::AdjustedFocusedElement() const {
  Document& document = RootNode().GetDocument();
  Element* element = document.FocusedElement();
  if (!element && document.GetPage())
    element = document.GetPage()->GetFocusController().FocusedFrameOwnerElement(
        *document.GetFrame());
  if (!element)
    return nullptr;

  // https://github.com/flackr/carousel/tree/main/scroll-marker#what-is-the-documentactiveelement-of-a-focused-pseudo-element
  if (auto* scroll_marker = DynamicTo<ScrollMarkerPseudoElement>(element)) {
    CHECK(scroll_marker->ScrollMarkerGroup());
    element = scroll_marker->ScrollMarkerGroup()->OriginatingElement();
  } else if (auto* pseudo_element = DynamicTo<PseudoElement>(element)) {
    element = pseudo_element->OriginatingElement();
  }

  CHECK(!element->IsPseudoElement());

  if (RootNode().IsInShadowTree()) {
    if (Element* retargeted = AdjustedFocusedElementInternal(*element)) {
      return (this == &retargeted->GetTreeScope()) ? retargeted : nullptr;
    }
    return nullptr;
  }

  EventPath* event_path = MakeGarbageCollected<EventPath>(*element);
  for (const auto& context : event_path->NodeEventContexts()) {
    if (context.GetNode() == RootNode()) {
      // context.target() is one of the followings:
      // - InsertionPoint
      // - shadow host
      // - Document::focusedElement()
      // So, it's safe to do To<Element>().
      return To<Element>(context.Target()->ToNode());
    }
  }
  return nullptr;
}

Element* TreeScope::AdjustedElement(const Element& target) const {
  const Element* adjusted_target = &target;
  for (const Element* ancestor = &target; ancestor;
       ancestor = ancestor->OwnerShadowHost()) {
    if (ancestor->GetShadowRoot())
      adjusted_target = ancestor;
    if (this == ancestor->GetTreeScope())
      return const_cast<Element*>(adjusted_target);
  }
  return nullptr;
}

StyleSheetList& TreeScope::StyleSheets() {
  if (!style_sheet_list_) {
    style_sheet_list_ = MakeGarbageCollected<StyleSheetList>(this);
  }
  return *style_sheet_list_;
}

Element* TreeScope::activeElement() const {
  if (Element* element = AdjustedFocusedElement()) {
    return element;
  }
  return document_ == this ? document_->body() : nullptr;
}

HeapVector<Member<Animation>> TreeScope::getAnimations() {
  return GetDocument().GetDocumentAnimations().getAnimations(*this);
}

Element* TreeScope::pointerLockElement() {
  UseCounter::Count(GetDocument(), WebFeature::kShadowRootPointerLockElement);
  const Element* target = GetDocument().PointerLockElement();
  return target ? AdjustedElement(*target) : nullptr;
}

Element* TreeScope::fullscreenElement() {
  return Fullscreen::FullscreenElementForBindingFrom(*this);
}

Element* TreeScope::pictureInPictureElement() {
  return PictureInPictureController::From(GetDocument())
      .PictureInPictureElement(*this);
}

uint16_t TreeScope::ComparePosition(const TreeScope& other_scope) const {
  if (other_scope == this)
    return Node::kDocumentPositionEquivalent;

  HeapVector<Member<const TreeScope>, 16> chain1;
  HeapVector<Member<const TreeScope>, 16> chain2;
  const TreeScope* current;
  for (current = this; current; current = current->ParentTreeScope())
    chain1.push_back(current);
  for (current = &other_scope; current; current = current->ParentTreeScope())
    chain2.push_back(current);

  unsigned index1 = chain1.size();
  unsigned index2 = chain2.size();
  if (chain1[index1 - 1] != chain2[index2 - 1])
    return Node::kDocumentPositionDisconnected |
           Node::kDocumentPositionImplementationSpecific;

  for (unsigned i = std::min(index1, index2); i; --i) {
    const TreeScope* child1 = chain1[--index1];
    const TreeScope* child2 = chain2[--index2];
    if (child1 != child2) {
      Node* shadow_host1 = child1->RootNode().ParentOrShadowHostNode();
      Node* shadow_host2 = child2->RootNode().ParentOrShadowHostNode();
      if (shadow_host1 != shadow_host2)
        return shadow_host1->compareDocumentPosition(
            shadow_host2, Node::kTreatShadowTreesAsDisconnected);
      return Node::kDocumentPositionPreceding;
    }
  }

  // There was no difference between the two parent chains, i.e., one was a
  // subset of the other. The shorter chain is the ancestor.
  return index1 < index2 ? Node::kDocumentPositionFollowing |
                               Node::kDocumentPositionContainedBy
                         : Node::kDocumentPositionPreceding |
                               Node::kDocumentPositionContains;
}

const TreeScope* TreeScope::CommonAncestorTreeScope(
    const TreeScope& other) const {
  HeapVector<Member<const TreeScope>, 16> this_chain;
  for (const TreeScope* tree = this; tree; tree = tree->ParentTreeScope())
    this_chain.push_back(tree);

  HeapVector<Member<const TreeScope>, 16> other_chain;
  for (const TreeScope* tree = &other; tree; tree = tree->ParentTreeScope())
    other_chain.push_back(tree);

  // Keep popping out the last elements of these chains until a mismatched pair
  // is found. If |this| and |other| belong to different documents, null will be
  // returned.
  const TreeScope* last_ancestor = nullptr;
  while (!this_chain.empty() && !other_chain.empty() &&
         this_chain.back() == other_chain.back()) {
    last_ancestor = this_chain.back();
    this_chain.pop_back();
    other_chain.pop_back();
  }
  return last_ancestor;
}

TreeScope* TreeScope::CommonAncestorTreeScope(TreeScope& other) {
  return const_cast<TreeScope*>(
      static_cast<const TreeScope&>(*this).CommonAncestorTreeScope(other));
}

bool TreeScope::IsInclusiveAncestorOf(const TreeScope& scope) const {
  for (const TreeScope* current = &scope; current;
       current = current->ParentTreeScope()) {
    if (current == this)
      return true;
  }
  return false;
}

Element* TreeScope::GetElementByAccessKey(const String& key) const {
  if (key.empty())
    return nullptr;
  Element* result = nullptr;
  Node& root = RootNode();
  for (Element& element : ElementTraversal::DescendantsOf(root)) {
    if (DeprecatedEqualIgnoringCase(
            element.FastGetAttribute(html_names::kAccesskeyAttr), key))
      result = &element;
    if (ShadowRoot* shadow_root = element.GetShadowRoot()) {
      if (Element* shadow_result = shadow_root->GetElementByAccessKey(key))
        result = shadow_result;
    }
  }
  return result;
}

void TreeScope::Trace(Visitor* visitor) const {
  visitor->Trace(root_node_);
  visitor->Trace(document_);
  visitor->Trace(parent_tree_scope_);
  visitor->Trace(id_target_observer_registry_);
  visitor->Trace(selection_);
  visitor->Trace(elements_by_id_);
  visitor->Trace(image_maps_by_name_);
  visitor->Trace(scoped_style_resolver_);
  visitor->Trace(radio_button_group_scope_);
  visitor->Trace(svg_tree_scoped_resources_);
  visitor->Trace(style_sheet_list_);
  visitor->Trace(adopted_style_sheets_);
}

IdTargetObserverRegistry& TreeScope::EnsureIdTargetObserverRegistry() {
  if (!id_target_observer_registry_) [[unlikely]] {
    id_target_observer_registry_ =
        MakeGarbageCollected<IdTargetObserverRegistry>();
  }
  return *id_target_observer_registry_;
}

}  // namespace blink
