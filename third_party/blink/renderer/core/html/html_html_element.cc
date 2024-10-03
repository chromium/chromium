/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Simon Hausmann (hausmann@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2006, 2010 Apple Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/html/html_html_element.h"

#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/document_parser.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/html_body_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_text_combine.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/frame_loader.h"
#include "third_party/blink/renderer/core/loader/render_blocking_resource_manager.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

HTMLHtmlElement::HTMLHtmlElement(Document& document)
    : HTMLElement(html_names::kHTMLTag, document) {}

bool HTMLHtmlElement::IsURLAttribute(const Attribute& attribute) const {
  return attribute.GetName() == html_names::kManifestAttr ||
         HTMLElement::IsURLAttribute(attribute);
}

void HTMLHtmlElement::InsertedByParser() {
  // When parsing a fragment, its dummy document has a null parser.
  if (!GetDocument().Parser())
    return;

  GetDocument().Parser()->DocumentElementAvailable();
  if (GetDocument().GetFrame()) {
    GetDocument().GetFrame()->Loader().DispatchDocumentElementAvailable();
    GetDocument().GetFrame()->Loader().RunScriptsAtDocumentElementAvailable();
    // RunScriptsAtDocumentElementAvailable might have invalidated
    // GetDocument().
  }
}

namespace {

bool NeedsLayoutStylePropagation(const ComputedStyle& layout_style,
                                 const ComputedStyle& propagated_style) {
  return layout_style.GetWritingMode() != propagated_style.GetWritingMode() ||
         layout_style.Direction() != propagated_style.Direction();
}

const ComputedStyle* CreateLayoutStyle(const ComputedStyle& style,
                                       const ComputedStyle& propagated_style) {
  ComputedStyleBuilder builder(style);
  builder.SetDirection(propagated_style.Direction());
  builder.SetWritingMode(propagated_style.GetWritingMode());
  builder.UpdateFontOrientation();
  return builder.TakeStyle();
}

}  // namespace

const ComputedStyle* HTMLHtmlElement::LayoutStyleForElement(
    const ComputedStyle* style) {
  DCHECK(style);
  DCHECK(GetDocument().InStyleRecalc());
  DCHECK(GetLayoutObject());
  StyleResolver& resolver = GetDocument().GetStyleResolver();
  if (resolver.ShouldStopBodyPropagation(*this))
    return style;
  if (const Element* body_element = GetDocument().FirstBodyElement()) {
    if (resolver.ShouldStopBodyPropagation(*body_element))
      return style;
    if (const ComputedStyle* body_style = body_element->GetComputedStyle()) {
      if (NeedsLayoutStylePropagation(*style, *body_style))
        return CreateLayoutStyle(*style, *body_style);
    }
  }
  return style;
}

void HTMLHtmlElement::PropagateWritingModeAndDirectionFromBody() {
  if (NeedsReattachLayoutTree()) {
    // This means we are being called from RecalcStyle(). Since we need to
    // reattach the layout tree, we will re-enter this method from
    // RebuildLayoutTree().
    return;
  }
  if (Element* body_element = GetDocument().FirstBodyElement()) {
    // Same as above.
    if (body_element->NeedsReattachLayoutTree())
      return;
  }

  auto* const layout_object = GetLayoutObject();
  if (!layout_object)
    return;

  const ComputedStyle* const old_style = layout_object->Style();
  const ComputedStyle* new_style =
      LayoutStyleForElement(layout_object->Style());

  if (old_style == new_style)
    return;

  const bool is_orthogonal = old_style->IsHorizontalWritingMode() !=
                             new_style->IsHorizontalWritingMode();

  // We need to propagate the style to text children because the used
  // writing-mode and direction affects text children. Child elements,
  // however, inherit the computed value, which is unaffected by the
  // propagated used value from body.
  for (Node* node = firstChild(); node; node = node->nextSibling()) {
    if (!node->IsTextNode() || node->NeedsReattachLayoutTree())
      continue;
    LayoutObject* const layout_text = node->GetLayoutObject();
    if (!layout_text)
      continue;
    if (is_orthogonal) {
      // If the old and new writing-modes are orthogonal, reattach the layout
      // objects to make sure we create or remove any LayoutTextCombine.
      node->SetNeedsReattachLayoutTree();
      continue;
    }
    auto* const text_combine =
        DynamicTo<LayoutTextCombine>(layout_text->Parent());
    if (text_combine) [[unlikely]] {
      layout_text->SetStyle(text_combine->Style());
      continue;
    }
    layout_text->SetStyle(new_style);
  }

  // Note: We should not call |Node::SetComputedStyle()| because computed
  // style keeps original style instead.
  // See wm-propagation-body-computed-root.html
  layout_object->SetStyle(new_style);

  // TODO(crbug.com/371033184): We should propagate `writing-mode` and
  // `direction` to ComputedStyles of pseudo elements of `this`.
  // * We can't use Element::RecalcStyle() because it refers to the
  //   ComputedStyle stored in this element, not `layout_object`.
  // * We should not copy `writing-mode` and `direction` values of `new_style`
  //   if `writing-mode` or `direction` is specified explicitly for a pseudo
  //   element.
  // See css/css-writing-modes/wm-propagation-body-{042,047,049,054}.html.
}

}  // namespace blink
