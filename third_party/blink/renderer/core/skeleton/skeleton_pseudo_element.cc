// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/skeleton/skeleton_pseudo_element.h"

#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/html/html_body_element.h"
#include "third_party/blink/renderer/core/html/html_html_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"

namespace blink {

SkeletonPseudoElement::SkeletonPseudoElement(Element* originating_element)
    : PseudoElement(originating_element, kPseudoIdSkeleton) {
  CHECK_EQ(originating_element,
           originating_element->GetDocument().documentElement());
}

void SkeletonPseudoElement::AttachLayoutTree(AttachContext& context) {
  AttachContext skeleton_context(context);
  skeleton_context.parent = GetDocument().GetLayoutView();
  PseudoElement::AttachLayoutTree(skeleton_context);
}

namespace {

void SetCanvasAndColorScheme(const Document& document,
                             ComputedStyleBuilder& builder,
                             mojom::blink::ColorScheme color_scheme) {
  // The color-scheme needs to be propagated from the skeleton root element so
  // that the background color here follows that.
  const ui::ColorProvider* color_provider =
      document.GetColorProviderForPainting(color_scheme);
  Color color = StyleColor::ColorFromKeyword(CSSValueID::kCanvas, color_scheme,
                                             color_provider,
                                             /*can_expose_accent_color=*/false);
  builder.SetDarkColorScheme(color_scheme == mojom::blink::ColorScheme::kDark);
  builder.SetBackgroundColor(StyleColor(color, CSSValueID::kCanvas));
}

}  // namespace

mojom::blink::ColorScheme SkeletonPseudoElement::SkeletonUsedColorScheme() {
  if (ShadowRoot* shadow_root = GetShadowRoot()) {
    if (HTMLHtmlElement* root =
            DynamicTo<HTMLHtmlElement>(shadow_root->firstChild())) {
      if (const ComputedStyle* root_style = root->GetComputedStyle()) {
        return root_style->UsedColorScheme();
      }
    }
  }
  return mojom::blink::ColorScheme::kLight;
}

void SkeletonPseudoElement::DidRecalcStyle(const StyleRecalcChange) {
  if (LayoutObject* layout_object = GetLayoutObject()) {
    mojom::blink::ColorScheme used_color_scheme = SkeletonUsedColorScheme();
    if (used_color_scheme != layout_object->StyleRef().UsedColorScheme()) {
      ComputedStyleBuilder builder(layout_object->StyleRef());
      // TODO(crbug.com/513276602): <meta> color-scheme elements do not have
      // an effect as the hosting document's page color-scheme is currently
      // used.
      SetCanvasAndColorScheme(GetDocument(), builder, used_color_scheme);
      layout_object->SetStyle(builder.TakeStyle());
    }
  }
}

const ComputedStyle* SkeletonPseudoElement::AdjustedLayoutStyle(
    const ComputedStyle& style,
    const ComputedStyle& layout_parent_style) {
  // The ::skeleton pseudo element does not match any styles, but we generate
  // a box for it which serves as a canvas for the skeleton document so that
  // the page we are navigating from is not seen through the skeleton.
  // SetCanvasAndColorScheme() sets the opaque background.
  mojom::blink::ColorScheme used_color_scheme = SkeletonUsedColorScheme();
  if (style.UsedColorScheme() == used_color_scheme) {
    return nullptr;
  }
  ComputedStyleBuilder builder(style);
  SetCanvasAndColorScheme(GetDocument(), builder, used_color_scheme);
  return builder.TakeStyle();
}

}  // namespace blink
