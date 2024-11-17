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

#include "third_party/blink/public/mojom/input/focus_type.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/attr.h"
#include "third_party/blink/renderer/core/dom/attribute.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/events/mouse_event.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/forms/html_form_element.h"
#include "third_party/blink/renderer/core/html/html_anchor_element.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_inline.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_transformable_container.h"
#include "third_party/blink/renderer/core/loader/frame_load_request.h"
#include "third_party/blink/renderer/core/loader/frame_loader.h"
#include "third_party/blink/renderer/core/loader/frame_loader_types.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/svg/animation/svg_smil_element.h"
#include "third_party/blink/renderer/core/svg/svg_animated_string.h"
#include "third_party/blink/renderer/core/svg_names.h"
#include "third_party/blink/renderer/core/xlink_names.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"

namespace blink {

SVGAElement::SVGAElement(Document& document)
    : SVGGraphicsElement(svg_names::kATag, document),
      SVGURIReference(this),
      svg_target_(
          MakeGarbageCollected<SVGAnimatedString>(this,
                                                  svg_names::kTargetAttr)) {}

void SVGAElement::Trace(Visitor* visitor) const {
  visitor->Trace(svg_target_);
  SVGGraphicsElement::Trace(visitor);
  SVGURIReference::Trace(visitor);
}

String SVGAElement::title() const {
  // If the xlink:title is set (non-empty string), use it.
  const AtomicString& title = FastGetAttribute(xlink_names::kTitleAttr);
  if (!title.empty())
    return title;

  // Otherwise, use the title of this element.
  return SVGElement::title();
}

void SVGAElement::SvgAttributeChanged(const SvgAttributeChangedParams& params) {
  // Unlike other SVG*Element classes, SVGAElement only listens to
  // SVGURIReference changes as none of the other properties changes the linking
  // behaviour for our <a> element.
  if (SVGURIReference::IsKnownAttribute(params.name)) {
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

  SVGGraphicsElement::SvgAttributeChanged(params);
}

LayoutObject* SVGAElement::CreateLayoutObject(const ComputedStyle&) {
  auto* svg_element = DynamicTo<SVGElement>(parentNode());
  if (svg_element && svg_element->IsTextContent())
    return MakeGarbageCollected<LayoutSVGInline>(this);

  return MakeGarbageCollected<LayoutSVGTransformableContainer>(this);
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

      if (!GetDocument().GetFrame())
        return;

      FrameLoadRequest frame_request(
          GetDocument().domWindow(),
          ResourceRequest(GetDocument().CompleteURL(url)));

      AtomicString target = frame_request.CleanNavigationTarget(
          AtomicString(svg_target_->CurrentValue()->Value()));
      if (target.empty() && FastGetAttribute(xlink_names::kShowAttr) == "new") {
        target = AtomicString("_blank");
      }
      event.SetDefaultHandled();

      NavigationPolicy navigation_policy = NavigationPolicyFromEvent(&event);
      if (navigation_policy == kNavigationPolicyLinkPreview) {
        // TODO(b:302649777): Support LinkPreview for SVG <a> element.
        return;
      }
      frame_request.SetNavigationPolicy(navigation_policy);
      frame_request.SetClientNavigationReason(
          ClientNavigationReason::kAnchorClick);
      frame_request.SetSourceElement(this);
      frame_request.SetTriggeringEventInfo(
          event.isTrusted()
              ? mojom::blink::TriggeringEventInfo::kFromTrustedEvent
              : mojom::blink::TriggeringEventInfo::kFromUntrustedEvent);
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

Element* SVGAElement::interestTargetElement() {
  CHECK(RuntimeEnabledFeatures::HTMLInterestTargetAttributeEnabled());

  if (!IsInTreeScope()) {
    return nullptr;
  }

  return GetElementAttributeResolvingReferenceTarget(
      svg_names::kInteresttargetAttr);
}

AtomicString SVGAElement::interestAction() const {
  CHECK(RuntimeEnabledFeatures::HTMLInterestTargetAttributeEnabled());
  const AtomicString& attribute_value =
      FastGetAttribute(svg_names::kInterestactionAttr);
  if (attribute_value && !attribute_value.IsNull() &&
      !attribute_value.empty()) {
    return attribute_value;
  }
  return g_empty_atom;
}

bool SVGAElement::HasActivationBehavior() const {
  return true;
}

int SVGAElement::DefaultTabIndex() const {
  return 0;
}

FocusableState SVGAElement::SupportsFocus(
    UpdateBehavior update_behavior) const {
  if (IsEditable(*this)) {
    return SVGGraphicsElement::SupportsFocus(update_behavior);
  }
  if (IsLink()) {
    return FocusableState::kFocusable;
  }
  // If not a link we should still be able to focus the element if it has
  // tabIndex.
  return SVGGraphicsElement::SupportsFocus(update_behavior);
}

bool SVGAElement::ShouldHaveFocusAppearance() const {
  return (GetDocument().LastFocusType() != mojom::blink::FocusType::kMouse) ||
         SVGGraphicsElement::SupportsFocus(
             UpdateBehavior::kNoneForFocusManagement) !=
             FocusableState::kNotFocusable;
}

bool SVGAElement::IsURLAttribute(const Attribute& attribute) const {
  return attribute.GetName().LocalName() == html_names::kHrefAttr ||
         SVGGraphicsElement::IsURLAttribute(attribute);
}

bool SVGAElement::IsKeyboardFocusable(UpdateBehavior update_behavior) const {
  if (IsLink() && !GetDocument().GetPage()->GetChromeClient().TabsToLinks()) {
    return false;
  }
  return SVGElement::IsKeyboardFocusable(update_behavior);
}

bool SVGAElement::CanStartSelection() const {
  if (!IsLink())
    return SVGElement::CanStartSelection();
  return IsEditable(*this);
}

bool SVGAElement::WillRespondToMouseClickEvents() {
  return IsLink() || SVGGraphicsElement::WillRespondToMouseClickEvents();
}

SVGAnimatedPropertyBase* SVGAElement::PropertyFromAttribute(
    const QualifiedName& attribute_name) const {
  if (attribute_name == svg_names::kTargetAttr) {
    return svg_target_.Get();
  } else {
    SVGAnimatedPropertyBase* ret =
        SVGURIReference::PropertyFromAttribute(attribute_name);
    if (ret) {
      return ret;
    } else {
      return SVGGraphicsElement::PropertyFromAttribute(attribute_name);
    }
  }
}

void SVGAElement::SynchronizeAllSVGAttributes() const {
  SVGAnimatedPropertyBase* attrs[]{svg_target_.Get()};
  SynchronizeListOfSVGAttributes(attrs);
  SVGURIReference::SynchronizeAllSVGAttributes();
  SVGGraphicsElement::SynchronizeAllSVGAttributes();
}

}  // namespace blink
