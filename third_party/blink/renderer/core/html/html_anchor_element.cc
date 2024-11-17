/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Simon Hausmann <hausmann@kde.org>
 * Copyright (C) 2003, 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights
 * reserved.
 *           (C) 2006 Graham Dennis (graham.dennis@gmail.com)
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

#include "third_party/blink/renderer/core/html/html_anchor_element.h"

#include <utility>

#include "base/metrics/histogram_macros.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/navigation/impression.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/mojom/input/focus_type.mojom-blink.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/web/web_link_preview_triggerer.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/events/mouse_event.h"
#include "third_party/blink/renderer/core/events/pointer_event.h"
#include "third_party/blink/renderer/core/events/web_input_event_conversion.h"
#include "third_party/blink/renderer/core/frame/ad_tracker.h"
#include "third_party/blink/renderer/core/frame/attribution_src_loader.h"
#include "third_party/blink/renderer/core/frame/deprecation/deprecation.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/anchor_element_metrics_sender.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/loader/anchor_element_interaction_tracker.h"
#include "third_party/blink/renderer/core/loader/frame_load_request.h"
#include "third_party/blink/renderer/core/loader/navigation_policy.h"
#include "third_party/blink/renderer/core/loader/ping_loader.h"
#include "third_party/blink/renderer/core/loader/render_blocking_resource_manager.h"
#include "third_party/blink/renderer/core/navigation_api/navigation_api.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/speculation_rules/document_speculation_rules.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/timer.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/weborigin/security_policy.h"
#include "ui/events/event_constants.h"
#include "ui/gfx/geometry/point_conversions.h"

namespace blink {

namespace {

// The download attribute specifies a filename, and an excessively long one can
// crash the browser process. Filepaths probably can't be longer than 4096
// characters, but this is enough to prevent the browser process from becoming
// unresponsive or crashing.
const int kMaxDownloadAttrLength = 1000000;

// Note: Here it covers download originated from clicking on <a download> link
// that results in direct download. Features in this method can also be logged
// from browser for download due to navigations to non-web-renderable content.
bool ShouldInterveneDownloadByFramePolicy(LocalFrame* frame) {
  bool should_intervene_download = false;
  Document& document = *(frame->GetDocument());
  UseCounter::Count(document, WebFeature::kDownloadPrePolicyCheck);
  bool has_gesture = LocalFrame::HasTransientUserActivation(frame);
  if (!has_gesture) {
    UseCounter::Count(document, WebFeature::kDownloadWithoutUserGesture);
  }
  if (frame->IsAdFrame()) {
    UseCounter::Count(document, WebFeature::kDownloadInAdFrame);
    if (!has_gesture) {
      UseCounter::Count(document,
                        WebFeature::kDownloadInAdFrameWithoutUserGesture);
      should_intervene_download = true;
    }
  }
  if (frame->DomWindow()->IsSandboxed(
          network::mojom::blink::WebSandboxFlags::kDownloads)) {
    UseCounter::Count(document, WebFeature::kDownloadInSandbox);
    should_intervene_download = true;
  }
  if (!should_intervene_download)
    UseCounter::Count(document, WebFeature::kDownloadPostPolicyCheck);
  return should_intervene_download;
}

void EmitDidAnchorElementReceiveMouseEvent(
    HTMLAnchorElementBase& anchor_element,
    Event& event) {
  if (!event.IsMouseEvent()) {
    return;
  }
  auto* mev = To<MouseEvent>(&event);
  LocalFrame* local_frame = anchor_element.GetDocument().GetFrame();
  if (!local_frame) {
    return;
  }

  WebLinkPreviewTriggerer* triggerer =
      local_frame->GetOrCreateLinkPreviewTriggerer();
  if (!triggerer) {
    return;
  }

  auto button = WebMouseEvent::Button(mev->button());
  if (event.type() == event_type_names::kMousedown) {
    triggerer->DidAnchorElementReceiveMouseDownEvent(
        WebElement(&anchor_element), button, mev->ClickCount());
  } else if (event.type() == event_type_names::kMouseup) {
    triggerer->DidAnchorElementReceiveMouseUpEvent(WebElement(&anchor_element),
                                                   button, mev->ClickCount());
  }
}

}  // namespace

HTMLAnchorElementBase::HTMLAnchorElementBase(const QualifiedName& tag_name,
                                             Document& document)
    : HTMLElement(tag_name, document),
      link_relations_(0),
      cached_visited_link_hash_(0),
      rel_list_(MakeGarbageCollected<RelList>(this)) {}

HTMLAnchorElementBase::~HTMLAnchorElementBase() = default;

FocusableState HTMLAnchorElementBase::SupportsFocus(
    UpdateBehavior update_behavior) const {
  if (IsLink() && !IsEditable(*this)) {
    return FocusableState::kFocusable;
  }
  return HTMLElement::SupportsFocus(update_behavior);
}

bool HTMLAnchorElementBase::ShouldHaveFocusAppearance() const {
  // TODO(crbug.com/1444450): Can't this be done with focus-visible now?
  return (GetDocument().LastFocusType() != mojom::blink::FocusType::kMouse) ||
         HTMLElement::SupportsFocus(UpdateBehavior::kNoneForFocusManagement) !=
             FocusableState::kNotFocusable;
}

FocusableState HTMLAnchorElementBase::IsFocusableState(
    UpdateBehavior update_behavior) const {
  if (!IsFocusableStyle(update_behavior)) {
    return FocusableState::kNotFocusable;
  }
  if (IsLink()) {
    return SupportsFocus(update_behavior);
  }
  return HTMLElement::IsFocusableState(update_behavior);
}

bool HTMLAnchorElementBase::IsKeyboardFocusable(
    UpdateBehavior update_behavior) const {
  if (!IsFocusableStyle(update_behavior)) {
    return false;
  }

  // Anchor is focusable if the base element is focusable. Note that
  // because HTMLAnchorElementBase overrides IsFocusable, we need to check
  // both SupportsFocus and IsFocusable.
  if (Element::SupportsFocus(update_behavior) !=
          FocusableState::kNotFocusable &&
      IsFocusable(update_behavior)) {
    return HTMLElement::IsKeyboardFocusable(update_behavior);
  }

  if (IsLink() && !GetDocument().GetPage()->GetChromeClient().TabsToLinks())
    return false;
  return HTMLElement::IsKeyboardFocusable(update_behavior);
}

static void AppendServerMapMousePosition(StringBuilder& url, Event* event) {
  auto* mouse_event = DynamicTo<MouseEvent>(event);
  if (!mouse_event)
    return;

  DCHECK(event->target());
  Node* target = event->target()->ToNode();
  DCHECK(target);
  auto* image_element = DynamicTo<HTMLImageElement>(target);
  if (!image_element || !image_element->IsServerMap())
    return;

  LayoutObject* layout_object = image_element->GetLayoutObject();
  if (!layout_object || !layout_object->IsBox())
    return;

  // The coordinates sent in the query string are relative to the height and
  // width of the image element, ignoring CSS transform/zoom.
  gfx::PointF map_point =
      layout_object->AbsoluteToLocalPoint(mouse_event->AbsoluteLocation());

  // The origin (0,0) is at the upper left of the content area, inside the
  // padding and border.
  map_point -=
      gfx::Vector2dF(To<LayoutBox>(layout_object)->PhysicalContentBoxOffset());

  // CSS zoom is not reflected in the map coordinates.
  float scale_factor = 1 / layout_object->Style()->EffectiveZoom();
  map_point.Scale(scale_factor, scale_factor);

  // Negative coordinates are clamped to 0 such that clicks in the left and
  // top padding/border areas receive an X or Y coordinate of 0.
  gfx::Point clamped_point = gfx::ToRoundedPoint(map_point);
  clamped_point.SetToMax(gfx::Point());

  url.Append('?');
  url.AppendNumber(clamped_point.x());
  url.Append(',');
  url.AppendNumber(clamped_point.y());
}

void HTMLAnchorElementBase::DefaultEventHandler(Event& event) {
  if (IsLink()) {
    EmitDidAnchorElementReceiveMouseEvent(*this, event);

    if (IsFocused() && IsEnterKeyKeydownEvent(event) && IsLiveLink()) {
      event.SetDefaultHandled();
      DispatchSimulatedClick(&event);
      return;
    }

    if (IsLinkClick(event) && IsLiveLink()) {
      // IsLinkClick validates that |event| is a MouseEvent.
      HandleClick(To<MouseEvent>(event));
      return;
    }
  }

  HTMLElement::DefaultEventHandler(event);
}

bool HTMLAnchorElementBase::HasActivationBehavior() const {
  return IsLink();
}

void HTMLAnchorElementBase::SetActive(bool active) {
  if (active && IsEditable(*this))
    return;

  HTMLElement::SetActive(active);
}

void HTMLAnchorElementBase::AttributeChanged(
    const AttributeModificationParams& params) {
  HTMLElement::AttributeChanged(params);

  if (params.reason != AttributeModificationReason::kDirectly)
    return;
  if (params.name != html_names::kHrefAttr)
    return;
  if (!IsLink() && AdjustedFocusedElementInTreeScope() == this)
    blur();
}

void HTMLAnchorElementBase::ParseAttribute(
    const AttributeModificationParams& params) {
  if (params.name == html_names::kHrefAttr) {
    if (params.old_value == params.new_value) {
      return;
    }
    bool was_link = IsLink();
    SetIsLink(!params.new_value.IsNull());
    if (was_link || IsLink()) {
      PseudoStateChanged(CSSSelector::kPseudoLink);
      PseudoStateChanged(CSSSelector::kPseudoVisited);
      if (was_link != IsLink()) {
        PseudoStateChanged(CSSSelector::kPseudoWebkitAnyLink);
        PseudoStateChanged(CSSSelector::kPseudoAnyLink);
      }
    }
    if (isConnected() && params.old_value != params.new_value) {
      if (auto* document_rules =
              DocumentSpeculationRules::FromIfExists(GetDocument())) {
        document_rules->HrefAttributeChanged(this, params.old_value,
                                             params.new_value);
      }
    }
    InvalidateCachedVisitedLinkHash();
    LogUpdateAttributeIfIsolatedWorldAndInDocument("a", params);
  } else if (params.name == html_names::kNameAttr) {
    if (GetDocument().HasRenderBlockingExpectLinkElements() && isConnected() &&
        IsFinishedParsingChildren() && !params.new_value.empty()) {
      DCHECK(GetDocument().GetRenderBlockingResourceManager());
      GetDocument()
          .GetRenderBlockingResourceManager()
          ->RemovePendingParsingElement(params.new_value, this);
    }
  } else if (params.name == html_names::kTitleAttr) {
    // Do nothing.
  } else if (params.name == html_names::kRelAttr) {
    SetRel(params.new_value);
    rel_list_->DidUpdateAttributeValue(params.old_value, params.new_value);
    if (isConnected() && IsLink() && params.old_value != params.new_value) {
      if (auto* document_rules =
              DocumentSpeculationRules::FromIfExists(GetDocument())) {
        document_rules->RelAttributeChanged(this);
      }
    }
  } else if (params.name == html_names::kReferrerpolicyAttr) {
    if (isConnected() && IsLink() && params.old_value != params.new_value) {
      if (auto* document_rules =
              DocumentSpeculationRules::FromIfExists(GetDocument())) {
        document_rules->ReferrerPolicyAttributeChanged(this);
      }
    }
  } else if (params.name == html_names::kTargetAttr) {
    if (isConnected() && IsLink() && params.old_value != params.new_value) {
      if (auto* document_rules =
              DocumentSpeculationRules::FromIfExists(GetDocument())) {
        document_rules->TargetAttributeChanged(this);
      }
    }
  } else {
    HTMLElement::ParseAttribute(params);
  }
}

bool HTMLAnchorElementBase::IsURLAttribute(const Attribute& attribute) const {
  return attribute.GetName().LocalName() == html_names::kHrefAttr ||
         HTMLElement::IsURLAttribute(attribute);
}

bool HTMLAnchorElementBase::HasLegalLinkAttribute(
    const QualifiedName& name) const {
  return name == html_names::kHrefAttr ||
         HTMLElement::HasLegalLinkAttribute(name);
}

void HTMLAnchorElementBase::FinishParsingChildren() {
  Element::FinishParsingChildren();
  if (GetDocument().HasRenderBlockingExpectLinkElements()) {
    DCHECK(GetDocument().GetRenderBlockingResourceManager());
    GetDocument()
        .GetRenderBlockingResourceManager()
        ->RemovePendingParsingElement(GetNameAttribute(), this);
  }
}

bool HTMLAnchorElementBase::CanStartSelection() const {
  if (!IsLink())
    return HTMLElement::CanStartSelection();
  return IsEditable(*this);
}

bool HTMLAnchorElementBase::draggable() const {
  // Should be draggable if we have an href attribute.
  const AtomicString& value = FastGetAttribute(html_names::kDraggableAttr);
  if (EqualIgnoringASCIICase(value, "true"))
    return true;
  if (EqualIgnoringASCIICase(value, "false"))
    return false;
  return FastHasAttribute(html_names::kHrefAttr);
}

KURL HTMLAnchorElementBase::Href() const {
  return GetDocument().CompleteURL(StripLeadingAndTrailingHTMLSpaces(
      FastGetAttribute(html_names::kHrefAttr)));
}

void HTMLAnchorElementBase::SetHref(const AtomicString& value) {
  setAttribute(html_names::kHrefAttr, value);
}

KURL HTMLAnchorElementBase::Url() const {
  KURL href = Href();
  if (!href.IsValid()) {
    return KURL();
  }
  return href;
}

void HTMLAnchorElementBase::SetURL(const KURL& url) {
  SetHref(AtomicString(url.GetString()));
}

String HTMLAnchorElementBase::Input() const {
  return FastGetAttribute(html_names::kHrefAttr);
}

void HTMLAnchorElementBase::setHref(const String& value) {
  SetHref(AtomicString(value));
}

bool HTMLAnchorElementBase::HasRel(uint32_t relation) const {
  return link_relations_ & relation;
}

void HTMLAnchorElementBase::SetRel(const AtomicString& value) {
  link_relations_ = 0;
  SpaceSplitString new_link_relations(value.LowerASCII());
  // FIXME: Add link relations as they are implemented
  if (new_link_relations.Contains(AtomicString("noreferrer"))) {
    link_relations_ |= kRelationNoReferrer;
  }
  if (new_link_relations.Contains(AtomicString("noopener"))) {
    link_relations_ |= kRelationNoOpener;
  }
  if (new_link_relations.Contains(AtomicString("opener"))) {
    link_relations_ |= kRelationOpener;
    UseCounter::Count(GetDocument(), WebFeature::kLinkRelOpener);
  }

  // These don't currently have web-facing behavior, but embedders may wish to
  // expose their presence to users:
  if (new_link_relations.Contains(AtomicString("privacy-policy"))) {
    link_relations_ |= kRelationPrivacyPolicy;
    UseCounter::Count(GetDocument(), WebFeature::kLinkRelPrivacyPolicy);
  }
  if (new_link_relations.Contains(AtomicString("terms-of-service"))) {
    link_relations_ |= kRelationTermsOfService;
    UseCounter::Count(GetDocument(), WebFeature::kLinkRelTermsOfService);
  }

  // Adding or removing a value here whose processing model is web-visible
  // (e.g. if the value is listed as a "supported token" for `<a>`'s `rel`
  // attribute in HTML) also requires you to update the list of tokens in
  // RelList::SupportedTokensAnchorAndAreaAndForm().
}

const AtomicString& HTMLAnchorElementBase::GetName() const {
  return GetNameAttribute();
}

const AtomicString& HTMLAnchorElementBase::GetEffectiveTarget() const {
  const AtomicString& target = FastGetAttribute(html_names::kTargetAttr);
  if (!target.empty())
    return target;
  return GetDocument().BaseTarget();
}

int HTMLAnchorElementBase::DefaultTabIndex() const {
  return 0;
}

bool HTMLAnchorElementBase::IsLiveLink() const {
  return IsLink() && !IsEditable(*this);
}

void HTMLAnchorElementBase::SendPings(const KURL& destination_url) const {
  const AtomicString& ping_value = FastGetAttribute(html_names::kPingAttr);
  if (ping_value.IsNull() || !GetDocument().GetSettings() ||
      !GetDocument().GetSettings()->GetHyperlinkAuditingEnabled()) {
    return;
  }

  // Pings should not be sent if MHTML page is loaded.
  if (GetDocument().Fetcher()->Archive())
    return;

  if ((ping_value.Contains('\n') || ping_value.Contains('\r') ||
       ping_value.Contains('\t')) &&
      ping_value.Contains('<')) {
    Deprecation::CountDeprecation(
        GetExecutionContext(), WebFeature::kCanRequestURLHTTPContainingNewline);
    return;
  }

  UseCounter::Count(GetDocument(), WebFeature::kHTMLAnchorElementPingAttribute);

  SpaceSplitString ping_urls(ping_value);
  for (unsigned i = 0; i < ping_urls.size(); i++) {
    PingLoader::SendLinkAuditPing(GetDocument().GetFrame(),
                                  GetDocument().CompleteURL(ping_urls[i]),
                                  destination_url);
  }
}

void HTMLAnchorElementBase::NavigateToHyperlink(
    ResourceRequest request,
    NavigationPolicy navigation_policy,
    bool is_trusted,
    base::TimeTicks platform_time_stamp,
    KURL completed_url) {
  LocalDOMWindow* window = GetDocument().domWindow();
  if (!window) {
    return;
  }

  LocalFrame* frame = window->GetFrame();
  if (!frame) {
    return;
  }

  if (navigation_policy == kNavigationPolicyLinkPreview) {
    // Ensured by third_party/blink/renderer/core/loader/navigation_policy.cc.
    CHECK(base::FeatureList::IsEnabled(features::kLinkPreview));

    DocumentSpeculationRules::From(GetDocument()).InitiatePreview(Url());
    return;
  }

  request.SetRequestContext(mojom::blink::RequestContextType::HYPERLINK);
  FrameLoadRequest frame_request(window, request);
  frame_request.SetNavigationPolicy(navigation_policy);
  frame_request.SetClientNavigationReason(ClientNavigationReason::kAnchorClick);
  frame_request.SetSourceElement(this);
  const AtomicString& target =
      frame_request.CleanNavigationTarget(GetEffectiveTarget());
  if (HasRel(kRelationNoReferrer)) {
    frame_request.SetNoReferrer();
    frame_request.SetNoOpener();
  }
  if (HasRel(kRelationNoOpener) ||
      (EqualIgnoringASCIICase(target, "_blank") && !HasRel(kRelationOpener) &&
       frame->GetSettings()
           ->GetTargetBlankImpliesNoOpenerEnabledWillBeRemoved())) {
    frame_request.SetNoOpener();
  }
  if (RuntimeEnabledFeatures::RelOpenerBcgDependencyHintEnabled(
          GetExecutionContext()) &&
      HasRel(kRelationOpener) && !frame_request.GetWindowFeatures().noopener) {
    frame_request.SetExplicitOpener();
  }

  frame_request.SetTriggeringEventInfo(
      is_trusted ? mojom::blink::TriggeringEventInfo::kFromTrustedEvent
                 : mojom::blink::TriggeringEventInfo::kFromUntrustedEvent);
  frame_request.SetInputStartTime(platform_time_stamp);

  if (const AtomicString& attribution_src =
          FastGetAttribute(html_names::kAttributionsrcAttr);
      !attribution_src.IsNull()) {
    // An impression must be attached prior to the
    // `FindOrCreateFrameForNavigation()` call, as that call may result in
    // performing a navigation if the call results in creating a new window with
    // noopener set.
    // At this time we don't know if the navigation will navigate a main frame
    // or subframe. For example, a middle click on the anchor element will
    // set `target_frame` to `frame`, but end up targeting a new window.
    // Attach the impression regardless, the embedder will be able to drop
    // impressions for subframe navigations.

    frame_request.SetImpression(
        frame->GetAttributionSrcLoader()->RegisterNavigation(
            /*navigation_url=*/completed_url, attribution_src,
            /*element=*/this, request.HasUserGesture(),
            request.GetReferrerPolicy()));
  }

  Frame* target_frame =
      frame->Tree().FindOrCreateFrameForNavigation(frame_request, target).frame;

  // If hrefTranslate is enabled and set restrict processing it
  // to same frame or navigations with noopener set.
  if (RuntimeEnabledFeatures::HrefTranslateEnabled(GetExecutionContext()) &&
      FastHasAttribute(html_names::kHreftranslateAttr) &&
      (target_frame == frame || frame_request.GetWindowFeatures().noopener)) {
    frame_request.SetHrefTranslate(
        FastGetAttribute(html_names::kHreftranslateAttr));
    UseCounter::Count(GetDocument(),
                      WebFeature::kHTMLAnchorElementHrefTranslateAttribute);
  }

  if (target_frame == frame && HasRel(kRelationOpener)) {
    // TODO(https://crbug.com/1431495): rel=opener is currently only meaningful
    // with target=_blank. Applying it to same-frame navigations is a potential
    // opt-out for issue 1431495, but how many sites would trigger this opt-out
    // inadvertently?
    UseCounter::Count(GetDocument(),
                      WebFeature::kLinkRelOpenerTargetingSameFrame);
  }

  if (target_frame) {
    target_frame->Navigate(frame_request, WebFrameLoadType::kStandard);
  }
}

void HTMLAnchorElementBase::SetHovered(bool hovered) {
  HTMLElement::SetHovered(hovered);
}

Element* HTMLAnchorElementBase::interestTargetElement() {
  CHECK(RuntimeEnabledFeatures::HTMLInterestTargetAttributeEnabled());

  if (!IsInTreeScope()) {
    return nullptr;
  }

  return GetElementAttributeResolvingReferenceTarget(
      html_names::kInteresttargetAttr);
}

AtomicString HTMLAnchorElementBase::interestAction() const {
  CHECK(RuntimeEnabledFeatures::HTMLInterestTargetAttributeEnabled());
  const AtomicString& attribute_value =
      FastGetAttribute(html_names::kInterestactionAttr);
  if (attribute_value && !attribute_value.IsNull() &&
      !attribute_value.empty()) {
    return attribute_value;
  }
  return g_empty_atom;
}

void HTMLAnchorElementBase::HandleClick(MouseEvent& event) {
  event.SetDefaultHandled();

  LocalDOMWindow* window = GetDocument().domWindow();
  if (!window)
    return;

  if (!isConnected()) {
    UseCounter::Count(GetDocument(),
                      WebFeature::kAnchorClickDispatchForNonConnectedNode);
  }

  if (auto* tracker = GetDocument().GetAnchorElementInteractionTracker()) {
    tracker->OnClickEvent(*this, event);
  }

  StringBuilder url;
  url.Append(StripLeadingAndTrailingHTMLSpaces(
      FastGetAttribute(html_names::kHrefAttr)));
  AppendServerMapMousePosition(url, &event);
  KURL completed_url = GetDocument().CompleteURL(url.ToString());

  // Schedule the ping before the frame load. Prerender in Chrome may kill the
  // renderer as soon as the navigation is sent out.
  SendPings(completed_url);

  ResourceRequest request(completed_url);

  network::mojom::ReferrerPolicy policy;
  if (FastHasAttribute(html_names::kReferrerpolicyAttr) &&
      SecurityPolicy::ReferrerPolicyFromString(
          FastGetAttribute(html_names::kReferrerpolicyAttr),
          kSupportReferrerPolicyLegacyKeywords, &policy) &&
      !HasRel(kRelationNoReferrer)) {
    UseCounter::Count(GetDocument(),
                      WebFeature::kHTMLAnchorElementReferrerPolicyAttribute);
    request.SetReferrerPolicy(policy);
  }

  LocalFrame* frame = window->GetFrame();
  request.SetHasUserGesture(LocalFrame::HasTransientUserActivation(frame));

  NavigationPolicy navigation_policy = NavigationPolicyFromEvent(&event);

  // Respect the download attribute only if we can read the content, and the
  // event is not an alt-click or similar.
  if (FastHasAttribute(html_names::kDownloadAttr) &&
      navigation_policy != kNavigationPolicyDownload &&
      window->GetSecurityOrigin()->CanReadContent(completed_url)) {
    if (ShouldInterveneDownloadByFramePolicy(frame))
      return;

    String download_attr =
        static_cast<String>(FastGetAttribute(html_names::kDownloadAttr));
    if (download_attr.length() > kMaxDownloadAttrLength) {
      AddConsoleMessage(
          mojom::blink::ConsoleMessageSource::kRendering,
          mojom::blink::ConsoleMessageLevel::kError,
          String::Format("Download attribute for anchor element is too long. "
                         "Max: %d, given: %d",
                         kMaxDownloadAttrLength, download_attr.length()));
      return;
    }

    auto* params = MakeGarbageCollected<NavigateEventDispatchParams>(
        completed_url, NavigateEventType::kCrossDocument,
        WebFrameLoadType::kStandard);
    if (event.isTrusted())
      params->involvement = UserNavigationInvolvement::kActivation;
    params->download_filename = download_attr;
    params->source_element = this;
    if (window->navigation()->DispatchNavigateEvent(params) !=
        NavigationApi::DispatchResult::kContinue) {
      return;
    }
    // A download will never notify blink about its completion. Tell the
    // NavigationApi that the navigation was dropped, so that it doesn't
    // leave the frame thinking it is loading indefinitely.
    window->navigation()->InformAboutCanceledNavigation(
        CancelNavigationReason::kDropped);

    request.SetSuggestedFilename(download_attr);
    request.SetRequestContext(mojom::blink::RequestContextType::DOWNLOAD);
    request.SetRequestorOrigin(window->GetSecurityOrigin());
    network::mojom::ReferrerPolicy referrer_policy =
        request.GetReferrerPolicy();
    if (referrer_policy == network::mojom::ReferrerPolicy::kDefault)
      referrer_policy = window->GetReferrerPolicy();
    Referrer referrer = SecurityPolicy::GenerateReferrer(
        referrer_policy, completed_url, window->OutgoingReferrer());
    request.SetReferrerString(referrer.referrer);
    request.SetReferrerPolicy(referrer.referrer_policy);
    frame->DownloadURL(request, network::mojom::blink::RedirectMode::kManual);
    return;
  }

  base::OnceClosure navigate_closure = WTF::BindOnce(
      &HTMLAnchorElementBase::NavigateToHyperlink, WrapWeakPersistent(this),
      std::move(request), navigation_policy, event.isTrusted(),
      event.PlatformTimeStamp(), std::move(completed_url));

  if (navigation_policy == kNavigationPolicyDownload ||
      navigation_policy == kNavigationPolicyLinkPreview) {
    // We distinguish single/double click with some modifiers.
    // See the comment of `EventHandler.delayed_navigation_task_handle_`.
    auto task_handle = PostDelayedCancellableTask(
        *base::SingleThreadTaskRunner::GetCurrentDefault(), FROM_HERE,
        std::move(navigate_closure),
        base::Milliseconds(ui::kDoubleClickTimeMs));
    frame->GetEventHandler().SetDelayedNavigationTaskHandle(
        std::move(task_handle));
  } else {
    std::move(navigate_closure).Run();
  }
}

bool IsEnterKeyKeydownEvent(Event& event) {
  auto* keyboard_event = DynamicTo<KeyboardEvent>(event);
  return event.type() == event_type_names::kKeydown && keyboard_event &&
         keyboard_event->key() == keywords::kCapitalEnter &&
         !keyboard_event->repeat();
}

bool IsLinkClick(Event& event) {
  auto* mouse_event = DynamicTo<MouseEvent>(event);
  if ((event.type() != event_type_names::kClick &&
       event.type() != event_type_names::kAuxclick) ||
      !mouse_event) {
    return false;
  }
  int16_t button = mouse_event->button();
  return (button == static_cast<int16_t>(WebPointerProperties::Button::kLeft) ||
          button ==
              static_cast<int16_t>(WebPointerProperties::Button::kMiddle));
}

bool HTMLAnchorElementBase::WillRespondToMouseClickEvents() {
  return IsLink() || HTMLElement::WillRespondToMouseClickEvents();
}

bool HTMLAnchorElementBase::IsInteractiveContent() const {
  return IsLink();
}

Node::InsertionNotificationRequest HTMLAnchorElementBase::InsertedInto(
    ContainerNode& insertion_point) {
  InsertionNotificationRequest request =
      HTMLElement::InsertedInto(insertion_point);
  LogAddElementIfIsolatedWorldAndInDocument("a", html_names::kHrefAttr);

  if (isConnected()) {
    if (auto* sender =
            AnchorElementMetricsSender::GetForFrame(GetDocument().GetFrame())) {
      sender->AddAnchorElement(*this);
    }
  }

  if (isConnected() && IsLink()) {
    if (auto* document_rules =
            DocumentSpeculationRules::FromIfExists(GetDocument())) {
      document_rules->LinkInserted(this);
    }
  }

  return request;
}

void HTMLAnchorElementBase::RemovedFrom(ContainerNode& insertion_point) {
  HTMLElement::RemovedFrom(insertion_point);

  if (insertion_point.isConnected()) {
    if (auto* sender =
            AnchorElementMetricsSender::GetForFrame(GetDocument().GetFrame())) {
      sender->RemoveAnchorElement(*this);
    }
  }

  if (insertion_point.isConnected() && IsLink()) {
    if (auto* document_rules =
            DocumentSpeculationRules::FromIfExists(GetDocument())) {
      document_rules->LinkRemoved(this);
    }
  }
}

void HTMLAnchorElementBase::Trace(Visitor* visitor) const {
  visitor->Trace(rel_list_);
  HTMLElement::Trace(visitor);
}

HTMLAnchorElement::HTMLAnchorElement(Document& document)
    : HTMLAnchorElementBase(html_names::kATag, document) {}

}  // namespace blink
