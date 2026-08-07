// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/mathml/mathml_anchor_element.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/events/mouse_event.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/anchor_element_utils.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/layout/mathml/layout_mathml_block.h"
#include "third_party/blink/renderer/core/loader/frame_load_request.h"
#include "third_party/blink/renderer/core/loader/navigation_policy.h"
#include "third_party/blink/renderer/core/mathml_names.h"
#include "third_party/blink/renderer/core/page/frame_tree.h"
#include "third_party/blink/renderer/core/url/dom_origin.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

MathMLAnchorElement::MathMLAnchorElement(Document& document)
    : MathMLElement(mathml_names::kATag, document),
      rel_list_(MakeGarbageCollected<RelList>(this, html_names::kRelAttr)) {}

void MathMLAnchorElement::Trace(Visitor* visitor) const {
  visitor->Trace(rel_list_);
  MathMLElement::Trace(visitor);
}

LayoutObject* MathMLAnchorElement::CreateLayoutObject(
    const ComputedStyle& style) {
  return style.IsDisplayMath() ? MakeGarbageCollected<LayoutMathMLBlock>(this)
                               : MathMLElement::CreateLayoutObject(style);
}

void MathMLAnchorElement::ParseAttribute(
    const AttributeModificationParams& params) {
  if (params.name == html_names::kHrefAttr) {
    bool was_link = IsLink();
    SetIsLink(!params.new_value.IsNull());
    if (was_link != IsLink()) {
      PseudoStateChanged(CSSSelector::kPseudoLink);
      PseudoStateChanged(CSSSelector::kPseudoVisited);
      PseudoStateChanged(CSSSelector::kPseudoAnyLink);
    }
  } else if (params.name == html_names::kRelAttr) {
    link_relations_ =
        AnchorElementUtils::ParseRelAttribute(params.new_value, GetDocument());
    rel_list_->DidUpdateAttributeValue(params.old_value, params.new_value);
  } else {
    MathMLElement::ParseAttribute(params);
  }
}

bool MathMLAnchorElement::IsURLAttribute(const Attribute& attribute) const {
  return attribute.GetName() == html_names::kHrefAttr ||
         MathMLElement::IsURLAttribute(attribute);
}

DOMOrigin* MathMLAnchorElement::GetDOMOrigin(LocalDOMWindow*) const {
  // No access check is necessary, as anchor elements are not accessible
  // cross-origin.
  if (FastHasAttribute(html_names::kHrefAttr)) {
    return DOMOrigin::Create(SecurityOrigin::Create(Url()));
  }
  return nullptr;
}

KURL MathMLAnchorElement::Url() const {
  KURL url = GetDocument().CompleteURL(StripLeadingAndTrailingHtmlSpaces(
      FastGetAttribute(html_names::kHrefAttr)));
  if (!url.IsValid()) {
    return KURL();
  }
  return url;
}

void MathMLAnchorElement::SetUrl(const KURL& url) {
  setAttribute(html_names::kHrefAttr, AtomicString(url.GetString()));
}

String MathMLAnchorElement::Input() const {
  return Url();
}

bool MathMLAnchorElement::HasActivationBehavior() const {
  return IsLink();
}

void MathMLAnchorElement::DefaultEventHandler(Event& event) {
  if (IsLink()) {
    if (IsFocused() && KeyboardEvent::IsEnterKeyKeydownEvent(event)) {
      event.SetDefaultHandled();
      DispatchSimulatedClick(&event);
      return;
    }

    if (AnchorElementUtils::IsLinkClick(event)) {
      HandleClick(To<MouseEvent>(event));
      return;
    }
  }
  MathMLElement::DefaultEventHandler(event);
}

void MathMLAnchorElement::HandleClick(MouseEvent& event) {
  event.SetDefaultHandled();

  LocalDOMWindow* window = GetDocument().domWindow();
  if (!window) {
    return;
  }

  LocalFrame* frame = window->GetFrame();
  if (!frame) {
    return;
  }

  const KURL& completed_url =
      GetDocument().CompleteURL(StripLeadingAndTrailingHtmlSpaces(
          FastGetAttribute(html_names::kHrefAttr)));

  AnchorElementUtils::SendPings(completed_url, GetDocument(),
                                FastGetAttribute(html_names::kPingAttr));

  ResourceRequest request(completed_url);
  AnchorElementUtils::HandleReferrerPolicyAttribute(
      request, FastGetAttribute(html_names::kReferrerpolicyAttr),
      link_relations_, GetDocument());

  request.SetHasUserGesture(LocalFrame::HasTransientUserActivation(frame));
  NavigationPolicy navigation_policy = NavigationPolicyFromEvent(&event);

  if (FastHasAttribute(html_names::kDownloadAttr) &&
      navigation_policy != kNavigationPolicyDownload &&
      window->GetSecurityOrigin()->CanReadContent(completed_url)) {
    const String download_attr = FastGetAttribute(html_names::kDownloadAttr);
    AnchorElementUtils::HandleDownloadAttribute(
        this, download_attr, completed_url, window, event.isTrusted(),
        std::move(request));
    return;
  }

  FrameLoadRequest frame_request(window, request);
  frame_request.SetNavigationPolicy(navigation_policy);
  frame_request.SetClientNavigationReason(ClientNavigationReason::kAnchorClick);
  frame_request.SetSourceElement(this);

  AtomicString target(FastGetAttribute(html_names::kTargetAttr));
  AnchorElementUtils::HandleRelAttribute(frame_request, frame->GetSettings(),
                                         GetExecutionContext(), target,
                                         link_relations_);
  AnchorElementUtils::EnforceBlobUrlNoopenerIfNeeded(frame_request,
                                                     completed_url, *window);
  frame_request.SetTriggeringEventInfo(
      event.isTrusted()
          ? mojom::blink::TriggeringEventInfo::kFromTrustedEvent
          : mojom::blink::TriggeringEventInfo::kFromUntrustedEvent);

  if (Frame* target_frame =
          frame->Tree()
              .FindOrCreateFrameForNavigation(frame_request, target)
              .frame) {
    target_frame->Navigate(frame_request, WebFrameLoadType::kStandard);
  }
}

FocusableState MathMLAnchorElement::SupportsFocus(
    UpdateBehavior update_behavior) const {
  if (IsLink()) {
    return FocusableState::kFocusable;
  }
  return MathMLElement::SupportsFocus(update_behavior);
}

/*
 * The default tabindex for mathml anchor element is 0.
 * See https://html.spec.whatwg.org/multipage/interaction.html#dom-tabindex
 */
int MathMLAnchorElement::DefaultTabIndex() const {
  return 0;
}

}  // namespace blink
