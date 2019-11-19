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

#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/platform/web_icon_sizes_parser.h"
#include "third_party/blink/public/platform/web_prescient_networking.h"
#include "third_party/blink/public/platform/web_size.h"
#include "third_party/blink/renderer/bindings/core/v8/script_event_listener.h"
#include "third_party/blink/renderer/core/core_initializer.h"
#include "third_party/blink/renderer/core/dom/attribute.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/frame/deprecation.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/html/cross_origin_attribute.h"
#include "third_party/blink/renderer/core/html/imports/link_import.h"
#include "third_party/blink/renderer/core/html/link_manifest.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/loader/link_loader.h"
#include "third_party/blink/renderer/core/origin_trials/origin_trial_context.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/weborigin/security_policy.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

HTMLLinkElement::HTMLLinkElement(Document& document,
                                 const CreateElementFlags flags)
    : HTMLElement(html_names::kLinkTag, document),
      link_loader_(LinkLoader::Create(this)),
      referrer_policy_(network::mojom::ReferrerPolicy::kDefault),
      sizes_(MakeGarbageCollected<DOMTokenList>(*this, html_names::kSizesAttr)),
      rel_list_(MakeGarbageCollected<RelList>(this)),
      created_by_parser_(flags.IsCreatedByParser()) {}

HTMLLinkElement::~HTMLLinkElement() = default;

void HTMLLinkElement::ParseAttribute(
    const AttributeModificationParams& params) {
  const QualifiedName& name = params.name;
  const AtomicString& value = params.new_value;
  if (name == html_names::kRelAttr) {
    rel_attribute_ = LinkRelAttribute(value);
    if (rel_attribute_.IsImport() &&
        RuntimeEnabledFeatures::HTMLImportsEnabled(&GetDocument())) {
      Deprecation::CountDeprecation(&GetDocument(), WebFeature::kHTMLImports);
    }
    rel_list_->DidUpdateAttributeValue(params.old_value, value);
    Process();
  } else if (name == html_names::kHrefAttr) {
    // Log href attribute before logging resource fetching in process().
    LogUpdateAttributeIfIsolatedWorldAndInDocument("link", params);
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
    WebVector<WebSize> web_icon_sizes =
        WebIconSizesParser::ParseIconSizes(value);
    icon_sizes_.resize(SafeCast<wtf_size_t>(web_icon_sizes.size()));
    for (wtf_size_t i = 0; i < icon_sizes_.size(); ++i)
      icon_sizes_[i] = web_icon_sizes[i];
    Process();
  } else if (name == html_names::kMediaAttr) {
    media_ = value.LowerASCII();
    Process();
  } else if (name == html_names::kScopeAttr) {
    scope_ = value;
    Process();
  } else if (name == html_names::kIntegrityAttr) {
    integrity_ = value;
  } else if (name == html_names::kImportanceAttr &&
             RuntimeEnabledFeatures::PriorityHintsEnabled(&GetDocument())) {
    UseCounter::Count(GetDocument(), WebFeature::kPriorityHints);
    importance_ = value;
  } else if (name == html_names::kDisabledAttr) {
    UseCounter::Count(GetDocument(), WebFeature::kHTMLLinkElementDisabled);
    if (params.reason == AttributeModificationReason::kByParser)
      UseCounter::Count(GetDocument(), WebFeature::kHTMLLinkElementDisabledByParser);
    if (LinkStyle* link = GetLinkStyle())
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
    // - <link> tags on html import documents.
    if (!rel_attribute_.IsStyleSheet() && !GetDocument().IsHTMLImport())
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
                                     ResourceClient* link_client) {
  return link_loader_->LoadStylesheet(params, localName(), charset,
                                      defer_option, GetDocument(), link_client);
}

LinkResource* HTMLLinkElement::LinkResourceToProcess() {
  if (!ShouldLoadLink()) {
    // If we shouldn't load the link, but the link is already of type
    // LinkType::kStyle and has a stylesheet loaded, it is because the
    // rel attribute is modified and we need to process it to remove
    // the sheet from the style engine and do style recalculation.
    if (GetLinkStyle() && GetLinkStyle()->HasSheet())
      return GetLinkStyle();
    // TODO(yoav): Ideally, the element's error event would be fired here.
    return nullptr;
  }

  if (!link_) {
    if (rel_attribute_.IsImport()) {
      // Only create an import link when HTML imports are enabled.
      if (!RuntimeEnabledFeatures::HTMLImportsEnabled(&GetDocument()))
        return nullptr;
      link_ = MakeGarbageCollected<LinkImport>(this);
    } else if (rel_attribute_.IsManifest()) {
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

LinkImport* HTMLLinkElement::GetLinkImport() const {
  if (!link_ || link_->GetType() != LinkResource::kImport)
    return nullptr;
  return static_cast<LinkImport*>(link_.Get());
}

Document* HTMLLinkElement::import() const {
  if (LinkImport* link = GetLinkImport())
    return link->ImportedDocument();
  return nullptr;
}

void HTMLLinkElement::Process() {
  if (LinkResource* link = LinkResourceToProcess())
    link->Process();
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
    GetDocument().AddConsoleMessage(
        ConsoleMessage::Create(mojom::ConsoleMessageSource::kJavaScript,
                               mojom::ConsoleMessageLevel::kWarning, message));
    return kInsertionDone;
  }

  Process();

  if (link_)
    link_->OwnerInserted();

  return kInsertionDone;
}

void HTMLLinkElement::RemovedFrom(ContainerNode& insertion_point) {
  // Store the result of isConnected() here before Node::removedFrom(..) clears
  // the flags.
  bool was_connected = isConnected();
  HTMLElement::RemovedFrom(insertion_point);
  if (!insertion_point.isConnected())
    return;

  link_loader_->Abort();

  if (!was_connected) {
    DCHECK(!GetLinkStyle() || !GetLinkStyle()->HasSheet());
    return;
  }
  GetDocument().GetStyleEngine().RemoveStyleSheetCandidateNode(*this,
                                                               insertion_point);
  if (link_)
    link_->OwnerRemoved();
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

void HTMLLinkElement::DidStartLinkPrerender() {
  DispatchEvent(*Event::Create(event_type_names::kWebkitprerenderstart));
}

void HTMLLinkElement::DidStopLinkPrerender() {
  DispatchEvent(*Event::Create(event_type_names::kWebkitprerenderstop));
}

void HTMLLinkElement::DidSendLoadForLinkPrerender() {
  DispatchEvent(*Event::Create(event_type_names::kWebkitprerenderload));
}

void HTMLLinkElement::DidSendDOMContentLoadedForLinkPrerender() {
  DispatchEvent(
      *Event::Create(event_type_names::kWebkitprerenderdomcontentloaded));
}

scoped_refptr<base::SingleThreadTaskRunner>
HTMLLinkElement::GetLoadingTaskRunner() {
  return GetDocument().GetTaskRunner(TaskType::kNetworking);
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
          WTF::Bind(&HTMLLinkElement::DispatchPendingEvent,
                    WrapPersistent(this),
                    WTF::Passed(std::make_unique<IncrementLoadEventDelayCount>(
                        GetDocument()))));
}

void HTMLLinkElement::StartLoadingDynamicSheet() {
  DCHECK(GetLinkStyle());
  GetLinkStyle()->StartLoadingDynamicSheet();
}

bool HTMLLinkElement::IsURLAttribute(const Attribute& attribute) const {
  return attribute.GetName().LocalName() == html_names::kHrefAttr ||
         HTMLElement::IsURLAttribute(attribute);
}

bool HTMLLinkElement::HasLegalLinkAttribute(const QualifiedName& name) const {
  return name == html_names::kHrefAttr ||
         HTMLElement::HasLegalLinkAttribute(name);
}

const QualifiedName& HTMLLinkElement::SubResourceAttributeName() const {
  // If the link element is not css, ignore it.
  if (DeprecatedEqualIgnoringCase(FastGetAttribute(html_names::kTypeAttr),
                                  "text/css")) {
    // FIXME: Add support for extracting links of sub-resources which
    // are inside style-sheet such as @import, @font-face, url(), etc.
    return html_names::kHrefAttr;
  }
  return HTMLElement::SubResourceAttributeName();
}

KURL HTMLLinkElement::Href() const {
  const String& url = FastGetAttribute(html_names::kHrefAttr);
  if (url.IsEmpty())
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

IconType HTMLLinkElement::GetIconType() const {
  return rel_attribute_.GetIconType();
}

const Vector<IntSize>& HTMLLinkElement::IconSizes() const {
  return icon_sizes_;
}

DOMTokenList* HTMLLinkElement::sizes() const {
  return sizes_.Get();
}

void HTMLLinkElement::Trace(Visitor* visitor) {
  visitor->Trace(link_);
  visitor->Trace(sizes_);
  visitor->Trace(link_loader_);
  visitor->Trace(rel_list_);
  HTMLElement::Trace(visitor);
  LinkLoaderClient::Trace(visitor);
}

}  // namespace blink
