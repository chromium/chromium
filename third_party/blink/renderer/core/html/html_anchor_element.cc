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

#include "base/metrics/histogram_macros.h"
#include "third_party/blink/public/common/download/download_stats.h"
#include "third_party/blink/renderer/bindings/core/v8/usv_string_or_trusted_url.h"
#include "third_party/blink/renderer/core/dom/user_gesture_indicator.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/events/mouse_event.h"
#include "third_party/blink/renderer/core/frame/ad_tracker.h"
#include "third_party/blink/renderer/core/frame/deprecation.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/use_counter.h"
#include "third_party/blink/renderer/core/html/anchor_element_metrics.h"
#include "third_party/blink/renderer/core/html/anchor_element_metrics_sender.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/loader/frame_load_request.h"
#include "third_party/blink/renderer/core/loader/navigation_policy.h"
#include "third_party/blink/renderer/core/loader/ping_loader.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/trustedtypes/trusted_types_util.h"
#include "third_party/blink/renderer/core/trustedtypes/trusted_url.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/network/network_hints.h"
#include "third_party/blink/renderer/platform/weborigin/security_policy.h"

namespace blink {

namespace {

void RecordDownloadMetrics(LocalFrame* frame) {
  DownloadStats::FrameType frame_type =
      frame->IsMainFrame()
          ? DownloadStats::FrameType::kMainFrame
          : frame->IsAdSubframe()
                ? frame->IsCrossOriginSubframe()
                      ? DownloadStats::FrameType::kCrossOriginAdSubframe
                      : DownloadStats::FrameType::kSameOriginAdSubframe
                : frame->IsCrossOriginSubframe()
                      ? DownloadStats::FrameType::kCrossOriginNonAdSubframe
                      : DownloadStats::FrameType::kSameOriginNonAdSubframe;
  DownloadStats::GestureType gesture_type =
      LocalFrame::HasTransientUserActivation(frame)
          ? DownloadStats::GestureType::kWithGesture
          : DownloadStats::GestureType::kWithoutGesture;
  DownloadStats::Record(frame_type, gesture_type);
}

}  // namespace

using namespace HTMLNames;

HTMLAnchorElement::HTMLAnchorElement(const QualifiedName& tag_name,
                                     Document& document)
    : HTMLElement(tag_name, document),
      link_relations_(0),
      cached_visited_link_hash_(0),
      rel_list_(RelList::Create(this)) {}

HTMLAnchorElement* HTMLAnchorElement::Create(Document& document) {
  return new HTMLAnchorElement(aTag, document);
}

HTMLAnchorElement::~HTMLAnchorElement() = default;

bool HTMLAnchorElement::SupportsFocus() const {
  if (HasEditableStyle(*this))
    return HTMLElement::SupportsFocus();
  // If not a link we should still be able to focus the element if it has
  // tabIndex.
  return IsLink() || HTMLElement::SupportsFocus();
}

bool HTMLAnchorElement::MatchesEnabledPseudoClass() const {
  return IsLink();
}

bool HTMLAnchorElement::ShouldHaveFocusAppearance() const {
  return (GetDocument().LastFocusType() != kWebFocusTypeMouse) ||
         HTMLElement::SupportsFocus();
}

bool HTMLAnchorElement::IsMouseFocusable() const {
  if (IsLink())
    return SupportsFocus();

  return HTMLElement::IsMouseFocusable();
}

bool HTMLAnchorElement::IsKeyboardFocusable() const {
  DCHECK(GetDocument().IsActive());

  if (IsFocusable() && Element::SupportsFocus())
    return HTMLElement::IsKeyboardFocusable();

  if (IsLink() && !GetDocument().GetPage()->GetChromeClient().TabsToLinks())
    return false;
  return HTMLElement::IsKeyboardFocusable();
}

static void AppendServerMapMousePosition(StringBuilder& url, Event* event) {
  if (!event->IsMouseEvent())
    return;

  DCHECK(event->target());
  Node* target = event->target()->ToNode();
  DCHECK(target);
  if (!IsHTMLImageElement(*target))
    return;

  HTMLImageElement& image_element = ToHTMLImageElement(*target);
  if (!image_element.IsServerMap())
    return;

  LayoutObject* layout_object = image_element.GetLayoutObject();
  if (!layout_object || !layout_object->IsBox())
    return;

  // The coordinates sent in the query string are relative to the height and
  // width of the image element, ignoring CSS transform/zoom.
  LayoutPoint map_point(layout_object->AbsoluteToLocal(
      FloatPoint(ToMouseEvent(event)->AbsoluteLocation()), kUseTransforms));

  // The origin (0,0) is at the upper left of the content area, inside the
  // padding and border.
  map_point -= ToLayoutBox(layout_object)->PhysicalContentBoxOffset();

  // CSS zoom is not reflected in the map coordinates.
  float scale_factor = 1 / layout_object->Style()->EffectiveZoom();
  map_point.Scale(scale_factor, scale_factor);

  // Negative coordinates are clamped to 0 such that clicks in the left and
  // top padding/border areas receive an X or Y coordinate of 0.
  IntPoint clamped_point(RoundedIntPoint(map_point));
  clamped_point.ClampNegativeToZero();

  url.Append('?');
  url.AppendNumber(clamped_point.X());
  url.Append(',');
  url.AppendNumber(clamped_point.Y());
}

void HTMLAnchorElement::DefaultEventHandler(Event& event) {
  if (IsLink()) {
    if (IsFocused() && IsEnterKeyKeydownEvent(event) && IsLiveLink()) {
      event.SetDefaultHandled();
      DispatchSimulatedClick(&event);
      return;
    }

    if (IsLinkClick(event) && IsLiveLink()) {
      HandleClick(event);
      return;
    }
  }

  HTMLElement::DefaultEventHandler(event);
}

bool HTMLAnchorElement::HasActivationBehavior() const {
  return true;
}

void HTMLAnchorElement::SetActive(bool down) {
  if (HasEditableStyle(*this))
    return;

  ContainerNode::SetActive(down);
}

const HashSet<AtomicString>& HTMLAnchorElement::GetCheckedAttributeNames()
    const {
  DEFINE_STATIC_LOCAL(HashSet<AtomicString>, attribute_set, ({"href"}));
  return attribute_set;
}

void HTMLAnchorElement::AttributeChanged(
    const AttributeModificationParams& params) {
  HTMLElement::AttributeChanged(params);
  if (params.reason != AttributeModificationReason::kDirectly)
    return;
  if (params.name != hrefAttr)
    return;
  if (!IsLink() && AdjustedFocusedElementInTreeScope() == this)
    blur();
}

void HTMLAnchorElement::ParseAttribute(
    const AttributeModificationParams& params) {
  if (params.name == hrefAttr) {
    bool was_link = IsLink();
    SetIsLink(!params.new_value.IsNull());
    if (was_link || IsLink()) {
      PseudoStateChanged(CSSSelector::kPseudoLink);
      PseudoStateChanged(CSSSelector::kPseudoVisited);
      PseudoStateChanged(CSSSelector::kPseudoWebkitAnyLink);
      PseudoStateChanged(CSSSelector::kPseudoAnyLink);
    }
    if (IsLink()) {
      String parsed_url = StripLeadingAndTrailingHTMLSpaces(params.new_value);
      if (GetDocument().IsDNSPrefetchEnabled()) {
        if (ProtocolIs(parsed_url, "http") || ProtocolIs(parsed_url, "https") ||
            parsed_url.StartsWith("//"))
          PrefetchDNS(GetDocument().CompleteURL(parsed_url).Host());
      }
    }
    InvalidateCachedVisitedLinkHash();
    LogUpdateAttributeIfIsolatedWorldAndInDocument("a", params);
  } else if (params.name == nameAttr || params.name == titleAttr) {
    // Do nothing.
  } else if (params.name == relAttr) {
    SetRel(params.new_value);
    rel_list_->DidUpdateAttributeValue(params.old_value, params.new_value);
  } else {
    HTMLElement::ParseAttribute(params);
  }
}

void HTMLAnchorElement::AccessKeyAction(bool send_mouse_events) {
  DispatchSimulatedClick(
      nullptr, send_mouse_events ? kSendMouseUpDownEvents : kSendNoEvents);
}

bool HTMLAnchorElement::IsURLAttribute(const Attribute& attribute) const {
  return attribute.GetName().LocalName() == hrefAttr ||
         HTMLElement::IsURLAttribute(attribute);
}

bool HTMLAnchorElement::HasLegalLinkAttribute(const QualifiedName& name) const {
  return name == hrefAttr || HTMLElement::HasLegalLinkAttribute(name);
}

bool HTMLAnchorElement::CanStartSelection() const {
  if (!IsLink())
    return HTMLElement::CanStartSelection();
  return HasEditableStyle(*this);
}

bool HTMLAnchorElement::draggable() const {
  // Should be draggable if we have an href attribute.
  const AtomicString& value = getAttribute(draggableAttr);
  if (DeprecatedEqualIgnoringCase(value, "true"))
    return true;
  if (DeprecatedEqualIgnoringCase(value, "false"))
    return false;
  return hasAttribute(hrefAttr);
}

KURL HTMLAnchorElement::Href() const {
  return GetDocument().CompleteURL(
      StripLeadingAndTrailingHTMLSpaces(getAttribute(hrefAttr)));
}

void HTMLAnchorElement::SetHref(const AtomicString& value) {
  setAttribute(hrefAttr, value);
}

void HTMLAnchorElement::setHref(const USVStringOrTrustedURL& stringOrTrustedURL,
                                ExceptionState& exception_state) {
  setAttribute(hrefAttr, stringOrTrustedURL, exception_state);
}

KURL HTMLAnchorElement::Url() const {
  return Href();
}

void HTMLAnchorElement::SetURL(const KURL& url) {
  SetHref(AtomicString(url.GetString()));
}

String HTMLAnchorElement::Input() const {
  return getAttribute(hrefAttr);
}

void HTMLAnchorElement::SetInput(const String& value) {
  SetHref(AtomicString(value));
}

bool HTMLAnchorElement::HasRel(uint32_t relation) const {
  return link_relations_ & relation;
}

void HTMLAnchorElement::SetRel(const AtomicString& value) {
  link_relations_ = 0;
  SpaceSplitString new_link_relations(value.LowerASCII());
  // FIXME: Add link relations as they are implemented
  if (new_link_relations.Contains("noreferrer"))
    link_relations_ |= kRelationNoReferrer;
  if (new_link_relations.Contains("noopener"))
    link_relations_ |= kRelationNoOpener;
}

const AtomicString& HTMLAnchorElement::GetName() const {
  return GetNameAttribute();
}

int HTMLAnchorElement::tabIndex() const {
  // Skip the supportsFocus check in HTMLElement.
  return Element::tabIndex();
}

bool HTMLAnchorElement::IsLiveLink() const {
  return IsLink() && !HasEditableStyle(*this);
}

void HTMLAnchorElement::SendPings(const KURL& destination_url) const {
  const AtomicString& ping_value = getAttribute(pingAttr);
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
        GetDocument(), WebFeature::kCanRequestURLHTTPContainingNewline);
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

void HTMLAnchorElement::HandleClick(Event& event) {
  event.SetDefaultHandled();

  LocalFrame* frame = GetDocument().GetFrame();
  if (!frame)
    return;

  if (!isConnected()) {
    UseCounter::Count(GetDocument(),
                      WebFeature::kAnchorClickDispatchForNonConnectedNode);
  }

  AnchorElementMetrics::MaybeReportClickedMetricsOnClick(this);

  StringBuilder url;
  url.Append(StripLeadingAndTrailingHTMLSpaces(FastGetAttribute(hrefAttr)));
  AppendServerMapMousePosition(url, &event);
  KURL completed_url = GetDocument().CompleteURL(url.ToString());

  // Schedule the ping before the frame load. Prerender in Chrome may kill the
  // renderer as soon as the navigation is sent out.
  SendPings(completed_url);

  ResourceRequest request(completed_url);

  ReferrerPolicy policy;
  if (hasAttribute(referrerpolicyAttr) &&
      SecurityPolicy::ReferrerPolicyFromString(
          FastGetAttribute(referrerpolicyAttr),
          kSupportReferrerPolicyLegacyKeywords, &policy) &&
      !HasRel(kRelationNoReferrer)) {
    UseCounter::Count(GetDocument(),
                      WebFeature::kHTMLAnchorElementReferrerPolicyAttribute);
    request.SetReferrerPolicy(policy);
  }

  if (hasAttribute(downloadAttr)) {
    if (GetDocument().IsSandboxed(kSandboxDownloads)) {
      // TODO(jochen): Also measure navigations resulting in downloads.
      UseCounter::Count(
          GetDocument(),
          UserGestureIndicator::ProcessingUserGesture()
              ? WebFeature::kHTMLAnchorElementDownloadInSandboxWithUserGesture
              : WebFeature::
                    kHTMLAnchorElementDownloadInSandboxWithoutUserGesture);
    }
    // Ignore the download attribute if we either can't read the content, or
    // the event is an alt-click or similar.
    if (NavigationPolicyFromEvent(&event) != kNavigationPolicyDownload &&
        GetDocument().GetSecurityOrigin()->CanReadContent(completed_url)) {
      RecordDownloadMetrics(frame);
      request.SetSuggestedFilename(
          static_cast<String>(FastGetAttribute(downloadAttr)));
      request.SetRequestContext(mojom::RequestContextType::DOWNLOAD);
      request.SetRequestorOrigin(SecurityOrigin::Create(GetDocument().Url()));
      frame->Client()->DownloadURL(request,
                                   DownloadCrossOriginRedirects::kNavigate);
      return;
    }
  }
  request.SetRequestContext(mojom::RequestContextType::HYPERLINK);
  FrameLoadRequest frame_request(&GetDocument(), request,
                                 getAttribute(targetAttr));
  if (HasRel(kRelationNoReferrer)) {
    frame_request.SetShouldSendReferrer(kNeverSendReferrer);
    frame_request.SetShouldSetOpener(kNeverSetOpener);
  }
  if (HasRel(kRelationNoOpener))
    frame_request.SetShouldSetOpener(kNeverSetOpener);
  frame_request.SetHrefTranslate(FastGetAttribute(hreftranslateAttr));
  frame_request.SetTriggeringEventInfo(
      event.isTrusted() ? WebTriggeringEventInfo::kFromTrustedEvent
                        : WebTriggeringEventInfo::kFromUntrustedEvent);
  frame_request.SetInputStartTime(event.PlatformTimeStamp());
  // TODO(japhet): Link clicks can be emulated via JS without a user gesture.
  // Why doesn't this go through NavigationScheduler?
  frame->Loader().StartNavigation(frame_request, WebFrameLoadType::kStandard,
                                  NavigationPolicyFromEvent(&event));
}

bool IsEnterKeyKeydownEvent(Event& event) {
  return event.type() == EventTypeNames::keydown && event.IsKeyboardEvent() &&
         ToKeyboardEvent(event).key() == "Enter" &&
         !ToKeyboardEvent(event).repeat();
}

bool IsLinkClick(Event& event) {
  if ((event.type() != EventTypeNames::click &&
       event.type() != EventTypeNames::auxclick) ||
      !event.IsMouseEvent()) {
    return false;
  }
  auto& mouse_event = ToMouseEvent(event);
  short button = mouse_event.button();
  return (button == static_cast<short>(WebPointerProperties::Button::kLeft) ||
          button == static_cast<short>(WebPointerProperties::Button::kMiddle));
}

bool HTMLAnchorElement::WillRespondToMouseClickEvents() {
  return IsLink() || HTMLElement::WillRespondToMouseClickEvents();
}

bool HTMLAnchorElement::IsInteractiveContent() const {
  return IsLink();
}

Node::InsertionNotificationRequest HTMLAnchorElement::InsertedInto(
    ContainerNode& insertion_point) {
  InsertionNotificationRequest request =
      HTMLElement::InsertedInto(insertion_point);
  LogAddElementIfIsolatedWorldAndInDocument("a", hrefAttr);

  Document& top_document = GetDocument().TopDocument();
  if (AnchorElementMetricsSender::HasAnchorElementMetricsSender(top_document))
    AnchorElementMetricsSender::From(top_document)->AddAnchorElement(*this);

  return request;
}

void HTMLAnchorElement::Trace(blink::Visitor* visitor) {
  visitor->Trace(rel_list_);
  HTMLElement::Trace(visitor);
}

}  // namespace blink
