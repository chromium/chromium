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

#include "third_party/blink/renderer/core/dom/element_rare_data.h"
#include "third_party/blink/renderer/core/dom/first_letter_pseudo_element.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/layout/generated_children.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_quote.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style/content_data.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

namespace blink {

PseudoElement* PseudoElement::Create(Element* parent, PseudoId pseudo_id) {
  if (pseudo_id == kPseudoIdFirstLetter)
    return MakeGarbageCollected<FirstLetterPseudoElement>(parent);
  return MakeGarbageCollected<PseudoElement>(parent, pseudo_id);
}

const QualifiedName& PseudoElementTagName(PseudoId pseudo_id) {
  switch (pseudo_id) {
    case kPseudoIdAfter: {
      DEFINE_STATIC_LOCAL(QualifiedName, after,
                          (g_null_atom, "<pseudo:after>", g_null_atom));
      return after;
    }
    case kPseudoIdBefore: {
      DEFINE_STATIC_LOCAL(QualifiedName, before,
                          (g_null_atom, "<pseudo:before>", g_null_atom));
      return before;
    }
    case kPseudoIdBackdrop: {
      DEFINE_STATIC_LOCAL(QualifiedName, backdrop,
                          (g_null_atom, "<pseudo:backdrop>", g_null_atom));
      return backdrop;
    }
    case kPseudoIdFirstLetter: {
      DEFINE_STATIC_LOCAL(QualifiedName, first_letter,
                          (g_null_atom, "<pseudo:first-letter>", g_null_atom));
      return first_letter;
    }
    default:
      NOTREACHED();
  }
  DEFINE_STATIC_LOCAL(QualifiedName, name,
                      (g_null_atom, "<pseudo>", g_null_atom));
  return name;
}

String PseudoElement::PseudoElementNameForEvents(PseudoId pseudo_id) {
  DEFINE_STATIC_LOCAL(const String, after, ("::after"));
  DEFINE_STATIC_LOCAL(const String, before, ("::before"));
  switch (pseudo_id) {
    case kPseudoIdAfter:
      return after;
    case kPseudoIdBefore:
      return before;
    default:
      return g_empty_string;
  }
}

PseudoElement::PseudoElement(Element* parent, PseudoId pseudo_id)
    : Element(PseudoElementTagName(pseudo_id),
              &parent->GetDocument(),
              kCreateElement),
      pseudo_id_(pseudo_id) {
  DCHECK_NE(pseudo_id, kPseudoIdNone);
  parent->GetTreeScope().AdoptIfNeeded(*this);
  SetParentOrShadowHostNode(parent);
  SetHasCustomStyleCallbacks();
  if ((pseudo_id == kPseudoIdBefore || pseudo_id == kPseudoIdAfter) &&
      parent->HasTagName(html_names::kInputTag)) {
    UseCounter::Count(parent->GetDocument(),
                      WebFeature::kPseudoBeforeAfterForInputElement);
  }
}

scoped_refptr<ComputedStyle> PseudoElement::CustomStyleForLayoutObject() {
  return ParentOrShadowHostElement()->StyleForPseudoElement(
      PseudoElementStyleRequest(pseudo_id_));
}

scoped_refptr<ComputedStyle> PseudoElement::LayoutStyleForDisplayContents(
    const ComputedStyle& style) {
  // For display:contents we should not generate a box, but we generate a non-
  // observable inline box for pseudo elements to be able to locate the
  // anonymous layout objects for generated content during DetachLayoutTree().
  scoped_refptr<ComputedStyle> layout_style = ComputedStyle::Create();
  layout_style->InheritFrom(style);
  layout_style->SetContent(style.GetContentData());
  layout_style->SetDisplay(EDisplay::kInline);
  layout_style->SetStyleType(pseudo_id_);
  return layout_style;
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
  {
    AttachLayoutTreeScope scope(this);
    Element::AttachLayoutTree(context);
  }
  LayoutObject* layout_object = GetLayoutObject();
  if (!layout_object)
    return;

  // This is to ensure that bypassing the CanHaveGeneratedChildren() check in
  // LayoutTreeBuilderForElement::ShouldCreateLayoutObject() does not result in
  // the backdrop pseudo element's layout object becoming the child of a layout
  // object that doesn't allow children.
  DCHECK(layout_object->Parent());
  DCHECK(CanHaveGeneratedChildren(*layout_object->Parent()));

  const ComputedStyle& style = layout_object->StyleRef();
  if (style.StyleType() != kPseudoIdBefore &&
      style.StyleType() != kPseudoIdAfter)
    return;
  DCHECK(style.GetContentData());

  for (const ContentData* content = style.GetContentData(); content;
       content = content->Next()) {
    LegacyLayout legacy = context.force_legacy_layout ? LegacyLayout::kForce
                                                      : LegacyLayout::kAuto;
    if (!content->IsAltText()) {
      LayoutObject* child = content->CreateLayoutObject(*this, style, legacy);
      if (layout_object->IsChildAllowed(child, style)) {
        layout_object->AddChild(child);
        if (child->IsQuote())
          ToLayoutQuote(child)->AttachQuote();
      } else {
        child->Destroy();
      }
    }
  }
}

bool PseudoElement::LayoutObjectIsNeeded(const ComputedStyle& style) const {
  return PseudoElementLayoutObjectIsNeeded(&style);
}

Node* PseudoElement::InnerNodeForHitTesting() const {
  return ParentOrShadowHostNode();
}

bool PseudoElementLayoutObjectIsNeeded(const ComputedStyle* style) {
  if (!style)
    return false;
  if (style->Display() == EDisplay::kNone)
    return false;
  if (style->StyleType() == kPseudoIdFirstLetter ||
      style->StyleType() == kPseudoIdBackdrop)
    return true;
  return style->GetContentData();
}

}  // namespace blink
