/*
 * Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/svg/svg_a_element.h"

#include "third_party/blink/renderer/core/dom/attr.h"
#include "third_party/blink/renderer/core/dom/attribute.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/events/mouse_event.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/forms/html_form_element.h"
#include "third_party/blink/renderer/core/html/html_anchor_element.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_inline.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_text.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_transformable_container.h"
#include "third_party/blink/renderer/core/loader/frame_load_request.h"
#include "third_party/blink/renderer/core/loader/frame_loader.h"
#include "third_party/blink/renderer/core/loader/frame_loader_types.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/svg/animation/svg_smil_element.h"
#include "third_party/blink/renderer/core/svg_names.h"
#include "third_party/blink/renderer/core/xlink_names.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"

namespace blink {

SVGAElement::SVGAElement(Document& document)
    : SVGGraphicsElement(svg_names::kATag, document),
      SVGURIReference(this),
      svg_target_(
          MakeGarbageCollected<SVGAnimatedString>(this,
                                                  svg_names::kTargetAttr)) {
  AddToPropertyMap(svg_target_);
}

void SVGAElement::Trace(blink::Visitor* visitor) {
  visitor->Trace(svg_target_);
  SVGGraphicsElement::Trace(visitor);
  SVGURIReference::Trace(visitor);
}

String SVGAElement::title() const {
  // If the xlink:title is set (non-empty string), use it.
  const AtomicString& title = FastGetAttribute(xlink_names::kTitleAttr);
  if (!title.IsEmpty())
    return title;

  // Otherwise, use the title of this element.
  return SVGElement::title();
}

void SVGAElement::SvgAttributeChanged(const QualifiedName& attr_name) {
  // Unlike other SVG*Element classes, SVGAElement only listens to
  // SVGURIReference changes as none of the other properties changes the linking
  // behaviour for our <a> element.
  if (SVGURIReference::IsKnownAttribute(attr_name)) {
    SVGElement::InvalidationGuard invalidation_guard(this);

    bool was_link = IsLink();
    SetIsLink(!HrefString().IsNull());

    if (was_link || IsLink()) {
      PseudoStateChanged(CSSSelector::kPseudoLink);
      PseudoStateChanged(CSSSelector::kPseudoVisited);
      PseudoStateChanged(CSSSelector::kPseudoWebkitAnyLink);
      PseudoStateChanged(CSSSelector::kPseudoAnyLink);
    }
    return;
  }

  SVGGraphicsElement::SvgAttributeChanged(attr_name);
}

LayoutObject* SVGAElement::CreateLayoutObject(const ComputedStyle&,
                                              LegacyLayout) {
  auto* svg_element = DynamicTo<SVGElement>(parentNode());
  if (svg_element && svg_element->IsTextContent())
    return new LayoutSVGInline(this);

  return new LayoutSVGTransformableContainer(this);
}

void SVGAElement::DefaultEventHandler(Event& event) {
  if (IsLink()) {
    if (IsFocused() && IsEnterKeyKeydownEvent(event)) {
      event.SetDefaultHandled();
      DispatchSimulatedClick(&event);
      return;
    }

    if (IsLinkClick(event)) {
      String url = StripLeadingAndTrailingHTMLSpaces(HrefString());

      if (url[0] == '#') {
        Element* target_element =
            GetTreeScope().getElementById(AtomicString(url.Substring(1)));
        if (auto* svg_smil_element =
                DynamicTo<SVGSMILElement>(target_element)) {
          svg_smil_element->BeginByLinkActivation();
          event.SetDefaultHandled();
          return;
        }
      }

      AtomicString target(svg_target_->CurrentValue()->Value());
      if (target.IsEmpty() && FastGetAttribute(xlink_names::kShowAttr) == "new")
        target = AtomicString("_blank");
      event.SetDefaultHandled();

      if (!GetDocument().GetFrame())
        return;

      FrameLoadRequest frame_request(
          &GetDocument(), ResourceRequest(GetDocument().CompleteURL(url)));
      frame_request.SetNavigationPolicy(NavigationPolicyFromEvent(&event));
      frame_request.SetTriggeringEventInfo(
          event.isTrusted() ? TriggeringEventInfo::kFromTrustedEvent
                            : TriggeringEventInfo::kFromUntrustedEvent);
      frame_request.GetResourceRequest().SetHasUserGesture(
          LocalFrame::HasTransientUserActivation(GetDocument().GetFrame()));

      Frame* frame = GetDocument()
                         .GetFrame()
                         ->Tree()
                         .FindOrCreateFrameForNavigation(frame_request, target)
                         .frame;
      if (!frame)
        return;
      frame->Navigate(frame_request, WebFrameLoadType::kStandard);
      return;
    }
  }

  SVGGraphicsElement::DefaultEventHandler(event);
}

bool SVGAElement::HasActivationBehavior() const {
  return true;
}

int SVGAElement::DefaultTabIndex() const {
  return 0;
}

bool SVGAElement::SupportsFocus() const {
  if (HasEditableStyle(*this))
    return SVGGraphicsElement::SupportsFocus();
  // If not a link we should still be able to focus the element if it has
  // tabIndex.
  return IsLink() || SVGGraphicsElement::SupportsFocus();
}

bool SVGAElement::ShouldHaveFocusAppearance() const {
  return (GetDocument().LastFocusType() != kWebFocusTypeMouse) ||
         SVGGraphicsElement::SupportsFocus();
}

bool SVGAElement::IsURLAttribute(const Attribute& attribute) const {
  return attribute.GetName().LocalName() == html_names::kHrefAttr ||
         SVGGraphicsElement::IsURLAttribute(attribute);
}

bool SVGAElement::IsMouseFocusable() const {
  if (IsLink())
    return SupportsFocus();

  return SVGElement::IsMouseFocusable();
}

bool SVGAElement::IsKeyboardFocusable() const {
  if (IsFocusable() && Element::SupportsFocus())
    return SVGElement::IsKeyboardFocusable();
  if (IsLink() && !GetDocument().GetPage()->GetChromeClient().TabsToLinks())
    return false;
  return SVGElement::IsKeyboardFocusable();
}

bool SVGAElement::CanStartSelection() const {
  if (!IsLink())
    return SVGElement::CanStartSelection();
  return HasEditableStyle(*this);
}

bool SVGAElement::WillRespondToMouseClickEvents() {
  return IsLink() || SVGGraphicsElement::WillRespondToMouseClickEvents();
}

}  // namespace blink
