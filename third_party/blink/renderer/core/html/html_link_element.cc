/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights
 * reserved.
 * Copyright (C) 2009 Rob Buis (rwlbuis@gmail.com)
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/html/html_link_element.h"

#include <utility>

#include "base/numerics/safe_conversions.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/platform/web_icon_sizes_parser.h"
#include "third_party/blink/public/platform/web_prescient_networking.h"
#include "third_party/blink/renderer/core/core_initializer.h"
#include "third_party/blink/renderer/core/dom/attribute.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/frame/frame_console.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/html/cross_origin_attribute.h"
#include "third_party/blink/renderer/core/html/link_manifest.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/loader/link_loader.h"
#include "third_party/blink/renderer/core/loader/render_blocking_resource_manager.h"
#include "third_party/blink/renderer/core/origin_trials/origin_trial_context.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/weborigin/security_policy.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

HTMLLinkElement::HTMLLinkElement(Document& document,
                                 const CreateElementFlags flags)
    : HTMLElement(html_names::kLinkTag, document),
      link_loader_(MakeGarbageCollected<LinkLoader>(this)),
      sizes_(MakeGarbageCollected<DOMTokenList>(*this, html_names::kSizesAttr)),
      rel_list_(MakeGarbageCollected<RelList>(this)),
      blocking_attribute_(MakeGarbageCollected<BlockingAttribute>(this)),
      created_by_parser_(flags.IsCreatedByParser()) {}

HTMLLinkElement::~HTMLLinkElement() = default;

void HTMLLinkElement::ParseAttribute(
    const AttributeModificationParams& params) {
  const QualifiedName& name = params.name;
  const AtomicString& value = params.new_value;
  if (name == html_names::kRelAttr) {
    // We're about to change the rel attribute. If it was "expect", first remove
    // it from a render blocking list.
    RemoveExpectRenderBlockingLink();

    rel_attribute_ = LinkRelAttribute(value);
    // TODO(vmpstr): Add rel=expect to UseCounter.
    AddExpectRenderBlockingLinkIfNeeded();

    if (rel_attribute_.IsMonetization() &&
        GetDocument().IsInOutermostMainFrame()) {
      // TODO(1031476): The Web Monetization specification is an unofficial
      // draft, available at https://webmonetization.org/specification.html
      // Currently it relies on a <meta> tag but there is an open issue about
      // whether the <link rel="monetization"> should be used instead:
      // https://github.com/interledger/webmonetization.org/issues/19
      // For now, only use counters are implemented in Blink.
      UseCounter::Count(&GetDocument(),
                        WebFeature::kHTMLLinkElementMonetization);
    }
    if (rel_attribute_.IsCanonical() &&
        GetDocument().IsInOutermostMainFrame()) {
      UseCounter::Count(&GetDocument(), WebFeature::kLinkRelCanonical);
    }
    if (rel_attribute_.IsPrivacyPolicy()) {
      UseCounter::Count(&GetDocument(), WebFeature::kLinkRelPrivacyPolicy);
    }
    if (rel_attribute_.IsTermsOfService()) {
      UseCounter::Count(&GetDocument(), WebFeature::kLinkRelTermsOfService);
    }
    if (rel_attribute_.IsPayment() && GetDocument().IsInOutermostMainFrame()) {
      UseCounter::Count(&GetDocument(), WebFeature::kLinkRelPayment);
#if BUILDFLAG(IS_ANDROID)
      if (RuntimeEnabledFeatures::PaymentLinkDetectionEnabled()) {
        GetDocument().HandlePaymentLink(
            GetNonEmptyURLAttribute(html_names::kHrefAttr));
      }
#endif
    }
    rel_list_->DidUpdateAttributeValue(params.old_value, value);
    Process();
  } else if (name == html_names::kBlockingAttr) {
    blocking_attribute_->OnAttributeValueChanged(params.old_value, value);
    if (!IsPotentiallyRenderBlocking()) {
      if (GetLinkStyle() && GetLinkStyle()->StyleSheetIsLoading())
        GetLinkStyle()->UnblockRenderingForPendingSheet();
    }
    HandleExpectBlockingChanges();
  } else if (name == html_names::kHrefAttr) {
    // Log href attribute before logging resource fetching in process().
    LogUpdateAttributeIfIsolatedWorldAndInDocument("link", params);
    HandleExpectHrefChanges(params.old_value, value);
    Process();
  } else if (name == html_names::kTypeAttr) {
    type_ = value;
    Process();
  } else if (name == html_names::kAsAttr) {
    as_ = value;
    Process();
  } else if (name == html_names::kReferrerpolicyAttr) {
    if (!value.IsNull()) {
      SecurityPolicy::ReferrerPolicyFromString(
          value, kDoNotSupportReferrerPolicyLegacyKeywords, &referrer_policy_);
      UseCounter::Count(GetDocument(),
                        WebFeature::kHTMLLinkElementReferrerPolicyAttribute);
    }
  } else if (name == html_names::kSizesAttr) {
    sizes_->DidUpdateAttributeValue(params.old_value, value);
    WebVector<gfx::Size> web_icon_sizes =
        WebIconSizesParser::ParseIconSizes(value);
    icon_sizes_.resize(base::checked_cast<wtf_size_t>(web_icon_sizes.size()));
    for (wtf_size_t i = 0; i < icon_sizes_.size(); ++i)
      icon_sizes_[i] = web_icon_sizes[i];
    Process();
  } else if (name == html_names::kMediaAttr) {
    media_ = value.LowerASCII();
    HandleExpectMediaChanges();
    Process(LinkLoadParameters::Reason::kMediaChange);
  } else if (name == html_names::kIntegrityAttr) {
    integrity_ = value;
  } else if (name == html_names::kFetchpriorityAttr) {
    UseCounter::Count(GetDocument(), WebFeature::kPriorityHints);
    fetch_priority_hint_ = value;
  } else if (name == html_names::kDisabledAttr) {
    UseCounter::Count(GetDocument(), WebFeature::kHTMLLinkElementDisabled);
    if (params.reason == AttributeModificationReason::kByParser)
      UseCounter::Count(GetDocument(), WebFeature::kHTMLLinkElementDisabledByParser);
    LinkStyle* link = GetLinkStyle();
    if (!link) {
      link = MakeGarbageCollected<LinkStyle>(this);
      link_ = link;
    }
    link->SetDisabledState(!value.IsNull());
  } else {
    if (name == html_names::kTitleAttr) {
      if (LinkStyle* link = GetLinkStyle())
        link->SetSheetTitle(value);
    }

    HTMLElement::ParseAttribute(params);
  }
}

bool HTMLLinkElement::ShouldLoadLink() {
  // Common case: We should load <link> on document that will be rendered.
  if (!InActiveDocument()) {
    // Handle rare cases.

    if (!isConnected())
      return false;

    // Load:
    // - <link> tags for stylesheets regardless of its document state
    //   (TODO: document why this is the case. kouhei@ doesn't know.)
    if (!rel_attribute_.IsStyleSheet())
      return false;
  }

  // We don't load links for the rel=expect, since that's just an expectation of
  // parsing of some other element on the page.
  if (rel_attribute_.IsExpect()) {
    return false;
  }

  const KURL& href = GetNonEmptyURLAttribute(html_names::kHrefAttr);
  return !href.PotentiallyDanglingMarkup();
}

bool HTMLLinkElement::IsLinkCreatedByParser() {
  return IsCreatedByParser();
}

bool HTMLLinkElement::LoadLink(const LinkLoadParameters& params) {
  return link_loader_->LoadLink(params, GetDocument());
}

void HTMLLinkElement::LoadStylesheet(const LinkLoadParameters& params,
                                     const WTF::TextEncoding& charset,
                                     FetchParameters::DeferOption defer_option,
                                     ResourceClient* link_client,
                                     RenderBlockingBehavior render_blocking) {
  return link_loader_->LoadStylesheet(params, localName(), charset,
                                      defer_option, GetDocument(), link_client,
                                      render_blocking);
}

LinkResource* HTMLLinkElement::LinkResourceToProcess() {
  if (!ShouldLoadLink()) {
    // If we shouldn't load the link, but the link is already of type
    // LinkType::kStyle and has a stylesheet loaded, it is because the
    // rel attribute is modified and we need to process it to remove
    // the sheet from the style engine and do style recalculation.
    if (GetLinkStyle() && GetLinkStyle()->HasSheet())
      return GetLinkStyle();
    return nullptr;
  }

  if (!link_) {
    if (rel_attribute_.IsManifest()) {
      link_ = MakeGarbageCollected<LinkManifest>(this);
    } else {
      auto* link = MakeGarbageCollected<LinkStyle>(this);
      if (FastHasAttribute(html_names::kDisabledAttr)) {
        UseCounter::Count(GetDocument(), WebFeature::kHTMLLinkElementDisabled);
        link->SetDisabledState(true);
      }
      link_ = link;
    }
  }

  return link_.Get();
}

LinkStyle* HTMLLinkElement::GetLinkStyle() const {
  if (!link_ || link_->GetType() != LinkResource::kStyle)
    return nullptr;
  return static_cast<LinkStyle*>(link_.Get());
}

void HTMLLinkElement::Process(LinkLoadParameters::Reason reason) {
  if (LinkResource* link = LinkResourceToProcess()) {
    link->Process(reason);
  }
}

Node::InsertionNotificationRequest HTMLLinkElement::InsertedInto(
    ContainerNode& insertion_point) {
  HTMLElement::InsertedInto(insertion_point);
  LogAddElementIfIsolatedWorldAndInDocument("link", html_names::kRelAttr,
                                            html_names::kHrefAttr);
  if (!insertion_point.isConnected())
    return kInsertionDone;
  DCHECK(isConnected());

  GetDocument().GetStyleEngine().AddStyleSheetCandidateNode(*this);

  if (!ShouldLoadLink() && IsInShadowTree()) {
    String message = "HTML element <link> is ignored in shadow tree.";
    GetDocument().AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
        mojom::ConsoleMessageSource::kJavaScript,
        mojom::ConsoleMessageLevel::kWarning, message));
    return kInsertionDone;
  }

  Process();

  if (link_)
    link_->OwnerInserted();

  AddExpectRenderBlockingLinkIfNeeded();
  return kInsertionDone;
}

void HTMLLinkElement::RemovedFrom(ContainerNode& insertion_point) {
  // Store the result of isConnected() here before Node::removedFrom(..) clears
  // the flags.
  bool was_connected = isConnected();
  HTMLElement::RemovedFrom(insertion_point);
  if (!insertion_point.isConnected() ||
      GetDocument().StatePreservingAtomicMoveInProgress()) {
    return;
  }

  link_loader_->Abort();

  if (!was_connected) {
    DCHECK(!GetLinkStyle() || !GetLinkStyle()->HasSheet());
    return;
  }
  GetDocument().GetStyleEngine().RemoveStyleSheetCandidateNode(*this,
                                                               insertion_point);
  if (link_)
    link_->OwnerRemoved();

  RemoveExpectRenderBlockingLink();
}

void HTMLLinkElement::FinishParsingChildren() {
  created_by_parser_ = false;
  HTMLElement::FinishParsingChildren();
}

bool HTMLLinkElement::HasActivationBehavior() const {
  // TODO(tkent): Implement activation behavior. crbug.com/422732.
  return false;
}

bool HTMLLinkElement::StyleSheetIsLoading() const {
  return GetLinkStyle() && GetLinkStyle()->StyleSheetIsLoading();
}

void HTMLLinkElement::LinkLoaded() {
  if (rel_attribute_.IsLinkPrefetch()) {
    UseCounter::Count(GetDocument(), WebFeature::kLinkPrefetchLoadEvent);
  }
  DispatchEvent(*Event::Create(event_type_names::kLoad));
}

void HTMLLinkElement::LinkLoadingErrored() {
  if (rel_attribute_.IsLinkPrefetch()) {
    UseCounter::Count(GetDocument(), WebFeature::kLinkPrefetchErrorEvent);
  }
  DispatchEvent(*Event::Create(event_type_names::kError));
}

bool HTMLLinkElement::SheetLoaded() {
  DCHECK(GetLinkStyle());
  return GetLinkStyle()->SheetLoaded();
}

void HTMLLinkElement::NotifyLoadedSheetAndAllCriticalSubresources(
    LoadedSheetErrorStatus error_status) {
  DCHECK(GetLinkStyle());
  GetLinkStyle()->NotifyLoadedSheetAndAllCriticalSubresources(error_status);
}

void HTMLLinkElement::DispatchPendingEvent(
    std::unique_ptr<IncrementLoadEventDelayCount> count) {
  DCHECK(link_);
  if (link_->HasLoaded())
    LinkLoaded();
  else
    LinkLoadingErrored();

  // Checks Document's load event synchronously here for performance.
  // This is safe because dispatchPendingEvent() is called asynchronously.
  count->ClearAndCheckLoadEvent();
}

void HTMLLinkElement::ScheduleEvent() {
  GetDocument()
      .GetTaskRunner(TaskType::kDOMManipulation)
      ->PostTask(
          FROM_HERE,
          WTF::BindOnce(
              &HTMLLinkElement::DispatchPendingEvent, WrapPersistent(this),
              std::make_unique<IncrementLoadEventDelayCount>(GetDocument())));
}

void HTMLLinkElement::SetToPendingState() {
  DCHECK(GetLinkStyle());
  GetLinkStyle()->SetToPendingState();
}

bool HTMLLinkElement::IsPotentiallyRenderBlocking() const {
  return blocking_attribute_->HasRenderToken() ||
         (IsCreatedByParser() && rel_attribute_.IsStyleSheet());
}

bool HTMLLinkElement::IsURLAttribute(const Attribute& attribute) const {
  return attribute.GetName().LocalName() == html_names::kHrefAttr ||
         HTMLElement::IsURLAttribute(attribute);
}

bool HTMLLinkElement::HasLegalLinkAttribute(const QualifiedName& name) const {
  return name == html_names::kHrefAttr ||
         HTMLElement::HasLegalLinkAttribute(name);
}

KURL HTMLLinkElement::Href() const {
  const String& url = FastGetAttribute(html_names::kHrefAttr);
  if (url.empty())
    return KURL();
  return GetDocument().CompleteURL(url);
}

const AtomicString& HTMLLinkElement::Rel() const {
  return FastGetAttribute(html_names::kRelAttr);
}

const AtomicString& HTMLLinkElement::GetType() const {
  return FastGetAttribute(html_names::kTypeAttr);
}

bool HTMLLinkElement::Async() const {
  return FastHasAttribute(html_names::kAsyncAttr);
}

mojom::blink::FaviconIconType HTMLLinkElement::GetIconType() const {
  return rel_attribute_.GetIconType();
}

const Vector<gfx::Size>& HTMLLinkElement::IconSizes() const {
  return icon_sizes_;
}

DOMTokenList* HTMLLinkElement::sizes() const {
  return sizes_.Get();
}

void HTMLLinkElement::Trace(Visitor* visitor) const {
  visitor->Trace(link_);
  visitor->Trace(sizes_);
  visitor->Trace(link_loader_);
  visitor->Trace(rel_list_);
  visitor->Trace(blocking_attribute_);
  HTMLElement::Trace(visitor);
  LinkLoaderClient::Trace(visitor);
}

void HTMLLinkElement::HandleExpectBlockingChanges() {
  if (!rel_attribute_.IsExpect()) {
    return;
  }

  if (blocking_attribute_->HasRenderToken()) {
    AddExpectRenderBlockingLinkIfNeeded();
  } else {
    RemoveExpectRenderBlockingLink();
  }
}

void HTMLLinkElement::HandleExpectHrefChanges(const String& old_value,
                                              const String& new_value) {
  if (!rel_attribute_.IsExpect()) {
    return;
  }

  RemoveExpectRenderBlockingLink(old_value);
  AddExpectRenderBlockingLinkIfNeeded(new_value);
}

bool HTMLLinkElement::MediaQueryMatches() const {
  if (LocalFrame* frame = GetDocument().GetFrame(); frame && !media_.empty()) {
    auto* media_queries =
        MediaQuerySet::Create(media_, GetDocument().GetExecutionContext());
    MediaQueryEvaluator* evaluator =
        MakeGarbageCollected<MediaQueryEvaluator>(frame);
    return evaluator->Eval(*media_queries);
  }
  return true;
}

void HTMLLinkElement::HandleExpectMediaChanges() {
  if (!rel_attribute_.IsExpect()) {
    return;
  }

  if (MediaQueryMatches()) {
    AddExpectRenderBlockingLinkIfNeeded(String(),
                                        /*media_known_to_match=*/true);
  } else {
    RemoveExpectRenderBlockingLink();
  }
}

void HTMLLinkElement::RemoveExpectRenderBlockingLink(const String& href) {
  if (!rel_attribute_.IsExpect()) {
    return;
  }

  if (auto* render_blocking_resource_manager =
          GetDocument().GetRenderBlockingResourceManager()) {
    render_blocking_resource_manager->RemovePendingParsingElementLink(
        ParseSameDocumentIdFromHref(href), this);
  }
}

AtomicString HTMLLinkElement::ParseSameDocumentIdFromHref(const String& href) {
  String actual_href =
      href.IsNull() ? FastGetAttribute(html_names::kHrefAttr) : href;
  if (actual_href.empty()) {
    return WTF::g_null_atom;
  }

  KURL url = GetDocument().CompleteURL(actual_href);
  if (!url.HasFragmentIdentifier()) {
    return WTF::g_null_atom;
  }

  return EqualIgnoringFragmentIdentifier(url, GetDocument().Url())
             ? AtomicString(url.FragmentIdentifier())
             : g_null_atom;
}

void HTMLLinkElement::AddExpectRenderBlockingLinkIfNeeded(
    const String& href,
    bool media_known_to_match) {
  if (!rel_attribute_.IsExpect()) {
    return;
  }

  bool media_matches = media_known_to_match || MediaQueryMatches();
  bool is_blocking_render = blocking_attribute_->HasRenderToken();
  if (!media_matches || !is_blocking_render || !isConnected()) {
    return;
  }

  if (auto* render_blocking_resource_manager =
          GetDocument().GetRenderBlockingResourceManager()) {
    render_blocking_resource_manager->AddPendingParsingElementLink(
        ParseSameDocumentIdFromHref(href), this);
  }
}

}  // namespace blink
