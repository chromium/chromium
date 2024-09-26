/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/dom/pseudo_element.h"

#include <utility>

#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/css/style_containment_scope_tree.h"
#include "third_party/blink/renderer/core/dom/element_rare_data_vector.h"
#include "third_party/blink/renderer/core/dom/first_letter_pseudo_element.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/scroll_button_pseudo_element.h"
#include "third_party/blink/renderer/core/dom/scroll_marker_group_pseudo_element.h"
#include "third_party/blink/renderer/core/dom/scroll_marker_pseudo_element.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/input_type_names.h"
#include "third_party/blink/renderer/core/layout/generated_children.h"
#include "third_party/blink/renderer/core/layout/layout_counter.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_quote.h"
#include "third_party/blink/renderer/core/layout/list/list_marker.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/core/style/content_data.h"
#include "third_party/blink/renderer/core/view_transition/view_transition.h"
#include "third_party/blink/renderer/core/view_transition/view_transition_pseudo_element_base.h"
#include "third_party/blink/renderer/core/view_transition/view_transition_utils.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

namespace blink {

using mojom::blink::FormControlType;

namespace {

// ::scroll-marker-group is represented internally as
// kPseudoIdScrollMarkerGroupBefore or kPseudoIdScrollMarkerGroupAfter,
// depending on scroll-marker property of originating element.
// But the have to resolve to kPseudoIdScrollMarkerGroup to
// correctly match CSS rules to the ::scroll-marker-group element.
PseudoId ResolvePseudoIdAlias(PseudoId pseudo_id) {
  switch (pseudo_id) {
    case kPseudoIdScrollMarkerGroupBefore:
    case kPseudoIdScrollMarkerGroupAfter:
      return kPseudoIdScrollMarkerGroup;
    default:
      return pseudo_id;
  }
}

}  // namespace

PseudoElement* PseudoElement::Create(Element* parent,
                                     PseudoId pseudo_id,
                                     const AtomicString& view_transition_name) {
  if (pseudo_id == kPseudoIdFirstLetter) {
    return MakeGarbageCollected<FirstLetterPseudoElement>(parent);
  } else if (IsTransitionPseudoElement(pseudo_id)) {
    auto* transition =
        ViewTransitionUtils::GetTransition(parent->GetDocument());
    DCHECK(transition);
    return transition->CreatePseudoElement(parent, pseudo_id,
                                           view_transition_name);
  } else if (ResolvePseudoIdAlias(pseudo_id) == kPseudoIdScrollMarkerGroup) {
    return MakeGarbageCollected<ScrollMarkerGroupPseudoElement>(parent,
                                                                pseudo_id);
  } else if (pseudo_id == kPseudoIdScrollMarker) {
    return MakeGarbageCollected<ScrollMarkerPseudoElement>(parent);
  } else if (pseudo_id == kPseudoIdScrollNextButton ||
             pseudo_id == kPseudoIdScrollPrevButton) {
    return MakeGarbageCollected<ScrollButtonPseudoElement>(parent, pseudo_id);
  }
  DCHECK(pseudo_id == kPseudoIdAfter || pseudo_id == kPseudoIdBefore ||
         pseudo_id == kPseudoIdBackdrop || pseudo_id == kPseudoIdMarker ||
         pseudo_id == kPseudoIdColumn);
  return MakeGarbageCollected<PseudoElement>(parent, pseudo_id,
                                             view_transition_name);
}

const QualifiedName& PseudoElementTagName(PseudoId pseudo_id) {
  switch (pseudo_id) {
    case kPseudoIdAfter: {
      DEFINE_STATIC_LOCAL(QualifiedName, after, (AtomicString("::after")));
      return after;
    }
    case kPseudoIdBefore: {
      DEFINE_STATIC_LOCAL(QualifiedName, before, (AtomicString("::before")));
      return before;
    }
    case kPseudoIdBackdrop: {
      DEFINE_STATIC_LOCAL(QualifiedName, backdrop,
                          (AtomicString("::backdrop")));
      return backdrop;
    }
    case kPseudoIdColumn: {
      DEFINE_STATIC_LOCAL(QualifiedName, first_letter,
                          (AtomicString("::column")));
      return first_letter;
    }
    case kPseudoIdFirstLetter: {
      DEFINE_STATIC_LOCAL(QualifiedName, first_letter,
                          (AtomicString("::first-letter")));
      return first_letter;
    }
    case kPseudoIdMarker: {
      DEFINE_STATIC_LOCAL(QualifiedName, marker, (AtomicString("::marker")));
      return marker;
    }
    case kPseudoIdScrollMarkerGroup: {
      DEFINE_STATIC_LOCAL(QualifiedName, scroll_marker_group,
                          (AtomicString("::scroll-marker-group")));
      return scroll_marker_group;
    }
    case kPseudoIdScrollNextButton: {
      DEFINE_STATIC_LOCAL(QualifiedName, scroll_next_button,
                          (AtomicString("::scroll-next-button")));
      return scroll_next_button;
    }
    case kPseudoIdScrollPrevButton: {
      DEFINE_STATIC_LOCAL(QualifiedName, scroll_prev_button,
                          (AtomicString("::scroll-prev-button")));
      return scroll_prev_button;
    }
    case kPseudoIdScrollMarker: {
      DEFINE_STATIC_LOCAL(QualifiedName, scroll_marker,
                          (AtomicString("::scroll-marker")));
      return scroll_marker;
    }
    case kPseudoIdViewTransition: {
      DEFINE_STATIC_LOCAL(QualifiedName, transition,
                          (AtomicString("::view-transition")));
      return transition;
    }
    case kPseudoIdViewTransitionGroup: {
      // TODO(khushalsagar) : Update these tag names to include the additional
      // ID.
      DEFINE_STATIC_LOCAL(QualifiedName, transition_container,
                          (AtomicString("::view-transition-group")));
      return transition_container;
    }
    case kPseudoIdViewTransitionImagePair: {
      DEFINE_STATIC_LOCAL(QualifiedName, transition_image_wrapper,
                          (AtomicString("::view-transition-image-pair")));
      return transition_image_wrapper;
    }
    case kPseudoIdViewTransitionNew: {
      DEFINE_STATIC_LOCAL(QualifiedName, transition_incoming_image,
                          (AtomicString("::view-transition-new")));
      return transition_incoming_image;
    }
    case kPseudoIdViewTransitionOld: {
      DEFINE_STATIC_LOCAL(QualifiedName, transition_outgoing_image,
                          (AtomicString("::view-transition-old")));
      return transition_outgoing_image;
    }
    default:
      NOTREACHED_IN_MIGRATION();
  }
  DEFINE_STATIC_LOCAL(QualifiedName, name, (AtomicString("::unknown")));
  return name;
}

AtomicString PseudoElement::PseudoElementNameForEvents(Element* element) {
  DCHECK(element);
  auto pseudo_id = element->GetPseudoIdForStyling();
  switch (pseudo_id) {
    case kPseudoIdNone:
      return g_null_atom;
    case kPseudoIdViewTransitionGroup:
    case kPseudoIdViewTransitionImagePair:
    case kPseudoIdViewTransitionNew:
    case kPseudoIdViewTransitionOld: {
      auto* pseudo = To<PseudoElement>(element);
      DCHECK(pseudo);
      StringBuilder builder;
      builder.Append(PseudoElementTagName(pseudo_id).LocalName());
      builder.Append("(");
      builder.Append(pseudo->view_transition_name());
      builder.Append(")");
      return AtomicString(builder.ReleaseString());
    }
    default:
      break;
  }
  return PseudoElementTagName(pseudo_id).LocalName();
}

PseudoId PseudoElement::GetPseudoIdForStyling() const {
  return ResolvePseudoIdAlias(pseudo_id_);
}

bool PseudoElement::IsWebExposed(PseudoId pseudo_id, const Node* parent) {
  switch (pseudo_id) {
    case kPseudoIdMarker:
      if (parent && parent->IsPseudoElement())
        return RuntimeEnabledFeatures::CSSMarkerNestedPseudoElementEnabled();
      return true;
    default:
      return true;
  }
}

PseudoElement::PseudoElement(Element* parent,
                             PseudoId pseudo_id,
                             const AtomicString& view_transition_name)
    : Element(PseudoElementTagName(ResolvePseudoIdAlias(pseudo_id)),
              &parent->GetDocument(),
              kCreateElement),
      pseudo_id_(pseudo_id),
      view_transition_name_(view_transition_name) {
  DCHECK_NE(pseudo_id, kPseudoIdNone);
  parent->GetTreeScope().AdoptIfNeeded(*this);
  SetParentOrShadowHostNode(parent);
  SetHasCustomStyleCallbacks();
  if ((pseudo_id == kPseudoIdBefore || pseudo_id == kPseudoIdAfter) &&
      parent->HasTagName(html_names::kInputTag)) {
    UseCounter::Count(parent->GetDocument(),
                      WebFeature::kPseudoBeforeAfterForInputElement);
    if (HTMLInputElement* input = DynamicTo<HTMLInputElement>(parent)) {
      if (input->FormControlType() == FormControlType::kInputDate ||
          input->FormControlType() == FormControlType::kInputDatetimeLocal ||
          input->FormControlType() == FormControlType::kInputMonth ||
          input->FormControlType() == FormControlType::kInputWeek ||
          input->FormControlType() == FormControlType::kInputTime) {
        UseCounter::Count(
            parent->GetDocument(),
            WebFeature::kPseudoBeforeAfterForDateTimeInputElement);
      }
    }
  }
}

const ComputedStyle* PseudoElement::CustomStyleForLayoutObject(
    const StyleRecalcContext& style_recalc_context) {
  // This method is not used for highlight pseudos that require an
  // originating element.
  DCHECK(!IsHighlightPseudoElement(pseudo_id_));
  Element* parent = ParentOrShadowHostElement();
  return parent->StyleForPseudoElement(
      style_recalc_context,
      StyleRequest(GetPseudoIdForStyling(), parent->GetComputedStyle(),
                   /* originating_element_style */ nullptr,
                   view_transition_name_));
}

const ComputedStyle* PseudoElement::LayoutStyleForDisplayContents(
    const ComputedStyle& style) {
  // For display:contents we should not generate a box, but we generate a non-
  // observable inline box for pseudo elements to be able to locate the
  // anonymous layout objects for generated content during DetachLayoutTree().
  ComputedStyleBuilder builder =
      GetDocument().GetStyleResolver().CreateComputedStyleBuilderInheritingFrom(
          style);
  builder.SetContent(style.GetContentData());
  builder.SetDisplay(EDisplay::kInline);
  builder.SetStyleType(GetPseudoIdForStyling());
  return builder.TakeStyle();
}

void PseudoElement::Dispose() {
  DCHECK(ParentOrShadowHostElement());

  probe::PseudoElementDestroyed(this);

  DCHECK(!nextSibling());
  DCHECK(!previousSibling());

  DetachLayoutTree();
  Element* parent = ParentOrShadowHostElement();
  GetDocument().AdoptIfNeeded(*this);
  SetParentOrShadowHostNode(nullptr);
  RemovedFrom(*parent);
}

PseudoElement::AttachLayoutTreeScope::AttachLayoutTreeScope(
    PseudoElement* element)
    : element_(element) {
  if (const ComputedStyle* style = element->GetComputedStyle()) {
    if (style->Display() == EDisplay::kContents) {
      original_style_ = style;
      element->SetComputedStyle(element->LayoutStyleForDisplayContents(*style));
    }
  }
}

PseudoElement::AttachLayoutTreeScope::~AttachLayoutTreeScope() {
  if (original_style_)
    element_->SetComputedStyle(std::move(original_style_));
}

void PseudoElement::AttachLayoutTree(AttachContext& context) {
  DCHECK(!GetLayoutObject());

  // Some elements may have 'display: list-item' but not be list items.
  // Do not create a layout object for the ::marker in that case.
  if (pseudo_id_ == kPseudoIdMarker) {
    LayoutObject* originating_layout = parentNode()->GetLayoutObject();
    if (!originating_layout || !originating_layout->IsListItem()) {
      const LayoutObject* layout_object = GetLayoutObject();
      if (layout_object) {
        context.counters_context.EnterObject(*layout_object);
      }
      Node::AttachLayoutTree(context);
      if (layout_object) {
        context.counters_context.LeaveObject(*layout_object);
      }
      return;
    }
  }

  {
    AttachLayoutTreeScope scope(this);
    Element::AttachLayoutTree(context);
  }
  LayoutObject* layout_object = GetLayoutObject();
  if (!layout_object)
    return;

  context.counters_context.EnterObject(*layout_object);

  // This is to ensure that bypassing the CanHaveGeneratedChildren() check in
  // LayoutTreeBuilderForElement::ShouldCreateLayoutObject() does not result in
  // the backdrop pseudo element's layout object becoming the child of a layout
  // object that doesn't allow children.
  DCHECK(layout_object->Parent());
  DCHECK(CanHaveGeneratedChildren(*layout_object->Parent()));

  const ComputedStyle& style = layout_object->StyleRef();
  switch (GetPseudoId()) {
    case kPseudoIdMarker: {
      if (ListMarker* marker = ListMarker::Get(layout_object))
        marker->UpdateMarkerContentIfNeeded(*layout_object);
      if (style.ContentBehavesAsNormal()) {
        context.counters_context.LeaveObject(*layout_object);
        return;
      }
      break;
    }
    case kPseudoIdScrollNextButton:
    case kPseudoIdScrollPrevButton:
      if (style.ContentBehavesAsNormal()) {
        context.counters_context.LeaveObject(*layout_object);
        return;
      }
      break;
    case kPseudoIdBefore:
    case kPseudoIdAfter:
      break;
    case kPseudoIdScrollMarker: {
      To<ScrollMarkerGroupPseudoElement>(context.parent->GetNode())
          ->AddToFocusGroup(*To<ScrollMarkerPseudoElement>(this));
      break;
    }
    default: {
      context.counters_context.LeaveObject(*layout_object);
      return;
    }
  }

  DCHECK(!style.ContentBehavesAsNormal());
  DCHECK(!style.ContentPreventsBoxGeneration());
  for (const ContentData* content = style.GetContentData(); content;
       content = content->Next()) {
    if (!content->IsAltText()) {
      LayoutObject* child = content->CreateLayoutObject(*layout_object);
      if (layout_object->IsChildAllowed(child, style)) {
        layout_object->AddChild(child);
        if (child->IsQuote()) {
          StyleContainmentScopeTree& tree =
              GetDocument().GetStyleEngine().EnsureStyleContainmentScopeTree();
          StyleContainmentScope* scope =
              tree.FindOrCreateEnclosingScopeForElement(*this);
          scope->AttachQuote(*To<LayoutQuote>(child));
          tree.UpdateOutermostQuotesDirtyScope(scope);
        }
        if (auto* layout_counter = DynamicTo<LayoutCounter>(child)) {
          if (context.counters_context.AttachmentRootIsDocumentElement()) {
            Vector<int> counter_values =
                context.counters_context.GetCounterValues(
                    *layout_object, layout_counter->Identifier(),
                    layout_counter->Separator().IsNull());
            layout_counter->UpdateCounter(std::move(counter_values));
          } else {
            GetDocument().GetStyleEngine().MarkCountersDirty();
          }
        }
      } else {
        child->Destroy();
      }
    }
  }
  context.counters_context.LeaveObject(*layout_object);
}

bool PseudoElement::CanGenerateContent() const {
  switch (GetPseudoIdForStyling()) {
    case kPseudoIdMarker:
    case kPseudoIdBefore:
    case kPseudoIdAfter:
    case kPseudoIdScrollMarker:
    case kPseudoIdScrollMarkerGroup:
    case kPseudoIdScrollNextButton:
    case kPseudoIdScrollPrevButton:
      return true;
    default:
      return false;
  }
}

bool PseudoElement::LayoutObjectIsNeeded(const DisplayStyle& style) const {
  return PseudoElementLayoutObjectIsNeeded(GetPseudoId(), style,
                                           parentElement());
}

bool PseudoElement::CanGeneratePseudoElement(PseudoId pseudo_id) const {
  switch (GetPseudoId()) {
    case kPseudoIdBefore:
    case kPseudoIdAfter:
      if (pseudo_id != kPseudoIdMarker)
        return false;
      break;
    default:
      return false;
  }
  return Element::CanGeneratePseudoElement(pseudo_id);
}

Node* PseudoElement::InnerNodeForHitTesting() {
  Node* parent = ParentOrShadowHostNode();
  if (parent && parent->IsPseudoElement())
    return To<PseudoElement>(parent)->InnerNodeForHitTesting();
  return parent;
}

void PseudoElement::AccessKeyAction(
    SimulatedClickCreationScope creation_scope) {
  // Even though pseudo elements can't use the accesskey attribute, assistive
  // tech can still attempt to interact with pseudo elements if they are in
  // the AX tree (usually due to their text/image content).
  // Just pass this request to the originating element.
  DCHECK(OriginatingElement());
  OriginatingElement()->AccessKeyAction(creation_scope);
}

Element* PseudoElement::OriginatingElement() const {
  auto* parent = parentElement();

  while (parent && parent->IsPseudoElement())
    parent = parent->parentElement();

  return parent;
}

bool PseudoElementLayoutObjectIsNeeded(PseudoId pseudo_id,
                                       const ComputedStyle* pseudo_style,
                                       const Element* originating_element) {
  if (!pseudo_style)
    return false;
  return PseudoElementLayoutObjectIsNeeded(
      pseudo_id, pseudo_style->GetDisplayStyle(), originating_element);
}

bool PseudoElementLayoutObjectIsNeeded(PseudoId pseudo_id,
                                       const DisplayStyle& pseudo_style,
                                       const Element* originating_element) {
  if (pseudo_style.Display() == EDisplay::kNone) {
    return false;
  }
  switch (pseudo_id) {
    case kPseudoIdFirstLetter:
    case kPseudoIdScrollMarkerGroupBefore:
    case kPseudoIdScrollMarkerGroupAfter:
    case kPseudoIdBackdrop:
    case kPseudoIdViewTransition:
    case kPseudoIdViewTransitionGroup:
    case kPseudoIdViewTransitionImagePair:
    case kPseudoIdViewTransitionNew:
    case kPseudoIdViewTransitionOld:
      return true;
    case kPseudoIdBefore:
    case kPseudoIdAfter:
      return !pseudo_style.ContentPreventsBoxGeneration();
    case kPseudoIdScrollMarker:
    case kPseudoIdScrollNextButton:
    case kPseudoIdScrollPrevButton:
      return !pseudo_style.ContentBehavesAsNormal();
    case kPseudoIdMarker: {
      if (!pseudo_style.ContentBehavesAsNormal()) {
        return !pseudo_style.ContentPreventsBoxGeneration();
      }
      const ComputedStyle* parent_style =
          originating_element->GetComputedStyle();
      return parent_style && (parent_style->ListStyleType() ||
                              parent_style->GeneratesMarkerImage());
    }
    default:
      NOTREACHED_IN_MIGRATION();
      return false;
  }
}

}  // namespace blink
