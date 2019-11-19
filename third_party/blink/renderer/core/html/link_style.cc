// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/link_style.h"

#include "services/network/public/mojom/referrer_policy.mojom-blink.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/html/cross_origin_attribute.h"
#include "third_party/blink/renderer/core/html/html_link_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/loader/importance_attribute.h"
#include "third_party/blink/renderer/core/loader/link_load_parameters.h"
#include "third_party/blink/renderer/core/loader/resource/css_style_sheet_resource.h"
#include "third_party/blink/renderer/core/loader/subresource_integrity_helper.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/instrumentation/histogram.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_parameters.h"
#include "third_party/blink/renderer/platform/loader/subresource_integrity.h"
#include "third_party/blink/renderer/platform/network/mime/content_type.h"
#include "third_party/blink/renderer/platform/network/mime/mime_type_registry.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_policy.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

static bool StyleSheetTypeIsSupported(const String& type) {
  String trimmed_type = ContentType(type).GetType();
  return trimmed_type.IsEmpty() ||
         MIMETypeRegistry::IsSupportedStyleSheetMIMEType(trimmed_type);
}

LinkStyle::LinkStyle(HTMLLinkElement* owner)
    : LinkResource(owner),
      disabled_state_(kUnset),
      pending_sheet_type_(kNone),
      loading_(false),
      fired_load_(false),
      loaded_sheet_(false) {}

LinkStyle::~LinkStyle() = default;

enum StyleSheetCacheStatus {
  kStyleSheetNewEntry,
  kStyleSheetInDiskCache,
  kStyleSheetInMemoryCache,
  kStyleSheetCacheStatusCount,
};

void LinkStyle::NotifyFinished(Resource* resource) {
  if (!owner_->isConnected()) {
    // While the stylesheet is asynchronously loading, the owner can be
    // disconnected from a document.
    // In that case, cancel any processing on the loaded content.
    loading_ = false;
    RemovePendingSheet();
    if (sheet_)
      ClearSheet();
    return;
  }

  CSSStyleSheetResource* cached_style_sheet = ToCSSStyleSheetResource(resource);
  // See the comment in pending_script.cc about why this check is necessary
  // here, instead of in the resource fetcher. https://crbug.com/500701.
  if ((!cached_style_sheet->ErrorOccurred() &&
       !owner_->FastGetAttribute(html_names::kIntegrityAttr).IsEmpty() &&
       !cached_style_sheet->IntegrityMetadata().IsEmpty()) ||
      resource->IsLinkPreload()) {
    ResourceIntegrityDisposition disposition =
        cached_style_sheet->IntegrityDisposition();

    SubresourceIntegrityHelper::DoReport(
        GetDocument(), cached_style_sheet->IntegrityReportInfo());

    if (disposition == ResourceIntegrityDisposition::kFailed) {
      loading_ = false;
      RemovePendingSheet();
      NotifyLoadedSheetAndAllCriticalSubresources(
          Node::kErrorOccurredLoadingSubresource);
      return;
    }
  }

  auto* parser_context = MakeGarbageCollected<CSSParserContext>(
      GetDocument(), cached_style_sheet->GetResponse().ResponseUrl(),
      cached_style_sheet->GetResponse().IsCorsSameOrigin(),
      cached_style_sheet->GetReferrerPolicy(), cached_style_sheet->Encoding());

  if (StyleSheetContents* parsed_sheet =
          cached_style_sheet->CreateParsedStyleSheetFromCache(parser_context)) {
    if (sheet_)
      ClearSheet();
    sheet_ = MakeGarbageCollected<CSSStyleSheet>(parsed_sheet, *owner_);
    sheet_->SetMediaQueries(MediaQuerySet::Create(owner_->Media()));
    if (owner_->IsInDocumentTree())
      SetSheetTitle(owner_->title());

    loading_ = false;
    parsed_sheet->CheckLoaded();

    return;
  }

  auto* style_sheet = MakeGarbageCollected<StyleSheetContents>(
      parser_context, cached_style_sheet->Url());

  if (sheet_)
    ClearSheet();

  sheet_ = MakeGarbageCollected<CSSStyleSheet>(style_sheet, *owner_);
  sheet_->SetMediaQueries(MediaQuerySet::Create(owner_->Media()));
  if (owner_->IsInDocumentTree())
    SetSheetTitle(owner_->title());

  style_sheet->ParseAuthorStyleSheet(cached_style_sheet,
                                     GetDocument().GetSecurityOrigin());

  loading_ = false;
  style_sheet->NotifyLoadedSheet(cached_style_sheet);
  style_sheet->CheckLoaded();

  if (style_sheet->IsCacheableForResource()) {
    const_cast<CSSStyleSheetResource*>(cached_style_sheet)
        ->SaveParsedStyleSheet(style_sheet);
  }
  ClearResource();
}

bool LinkStyle::SheetLoaded() {
  if (!StyleSheetIsLoading()) {
    RemovePendingSheet();
    return true;
  }
  return false;
}

void LinkStyle::NotifyLoadedSheetAndAllCriticalSubresources(
    Node::LoadedSheetErrorStatus error_status) {
  if (fired_load_)
    return;
  loaded_sheet_ = (error_status == Node::kNoErrorLoadingSubresource);
  if (owner_)
    owner_->ScheduleEvent();
  fired_load_ = true;
}

void LinkStyle::StartLoadingDynamicSheet() {
  DCHECK_LT(pending_sheet_type_, kBlocking);
  AddPendingSheet(kBlocking);
}

void LinkStyle::ClearSheet() {
  DCHECK(sheet_);
  DCHECK_EQ(sheet_->ownerNode(), owner_);
  sheet_.Release()->ClearOwnerNode();
}

bool LinkStyle::StyleSheetIsLoading() const {
  if (loading_)
    return true;
  if (!sheet_)
    return false;
  return sheet_->Contents()->IsLoading();
}

void LinkStyle::AddPendingSheet(PendingSheetType type) {
  if (type <= pending_sheet_type_)
    return;
  pending_sheet_type_ = type;

  if (pending_sheet_type_ == kNonBlocking)
    return;
  GetDocument().GetStyleEngine().AddPendingSheet(style_engine_context_);
}

void LinkStyle::RemovePendingSheet() {
  DCHECK(owner_);
  PendingSheetType type = pending_sheet_type_;
  pending_sheet_type_ = kNone;

  if (type == kNone)
    return;
  if (type == kNonBlocking) {
    // Tell StyleEngine to re-compute styleSheets of this owner_'s treescope.
    GetDocument().GetStyleEngine().ModifiedStyleSheetCandidateNode(*owner_);
    return;
  }

  GetDocument().GetStyleEngine().RemovePendingSheet(*owner_,
                                                    style_engine_context_);
}

void LinkStyle::SetDisabledState(bool disabled) {
  LinkStyle::DisabledState old_disabled_state = disabled_state_;
  disabled_state_ = disabled ? kDisabled : kEnabledViaScript;
  if (old_disabled_state == disabled_state_)
    return;

  // If we change the disabled state while the sheet is still loading, then we
  // have to perform three checks:
  if (StyleSheetIsLoading()) {
    // Check #1: The sheet becomes disabled while loading.
    if (disabled_state_ == kDisabled)
      RemovePendingSheet();

    // Check #2: An alternate sheet becomes enabled while it is still loading.
    if (owner_->RelAttribute().IsAlternate() &&
        disabled_state_ == kEnabledViaScript)
      AddPendingSheet(kBlocking);

    // Check #3: A main sheet becomes enabled while it was still loading and
    // after it was disabled via script. It takes really terrible code to make
    // this happen (a double toggle for no reason essentially). This happens
    // on virtualplastic.net, which manages to do about 12 enable/disables on
    // only 3 sheets. :)
    if (!owner_->RelAttribute().IsAlternate() &&
        disabled_state_ == kEnabledViaScript && old_disabled_state == kDisabled)
      AddPendingSheet(kBlocking);

    // If the sheet is already loading just bail.
    return;
  }

  if (sheet_) {
    sheet_->setDisabled(disabled);
    return;
  }

  if (disabled_state_ == kEnabledViaScript && owner_->ShouldProcessStyle())
    Process();
}

LinkStyle::LoadReturnValue LinkStyle::LoadStylesheetIfNeeded(
    const LinkLoadParameters& params,
    const WTF::TextEncoding& charset) {
  if (disabled_state_ == kDisabled || !owner_->RelAttribute().IsStyleSheet() ||
      !StyleSheetTypeIsSupported(params.type) || !ShouldLoadResource() ||
      !params.href.IsValid())
    return kNotNeeded;

  if (GetResource()) {
    RemovePendingSheet();
    ClearResource();
  }

  if (!owner_->ShouldLoadLink())
    return kBail;

  loading_ = true;

  String title = owner_->title();
  if (!title.IsEmpty() && !owner_->IsAlternate() &&
      disabled_state_ != kEnabledViaScript && owner_->IsInDocumentTree()) {
    GetDocument().GetStyleEngine().SetPreferredStylesheetSetNameIfNotSet(title);
  }

  bool media_query_matches = true;
  LocalFrame* frame = LoadingFrame();
  if (!owner_->Media().IsEmpty() && frame) {
    scoped_refptr<MediaQuerySet> media = MediaQuerySet::Create(owner_->Media());
    MediaQueryEvaluator evaluator(frame);
    media_query_matches = evaluator.Eval(*media);
  }

  // Don't hold up layout tree construction and script execution on
  // stylesheets that are not needed for the layout at the moment.
  bool blocking = media_query_matches && !owner_->IsAlternate() &&
                  owner_->IsCreatedByParser();
  AddPendingSheet(blocking ? kBlocking : kNonBlocking);

  // Load stylesheets that are not needed for the layout immediately with low
  // priority.  When the link element is created by scripts, load the
  // stylesheets asynchronously but in high priority.
  FetchParameters::DeferOption defer_option =
      !media_query_matches || owner_->IsAlternate() ? FetchParameters::kLazyLoad
                                                    : FetchParameters::kNoDefer;

  owner_->LoadStylesheet(params, charset, defer_option, this);

  if (loading_ && !GetResource()) {
    // Fetch() synchronous failure case.
    // The request may have been denied if (for example) the stylesheet is
    // local and the document is remote, or if there was a Content Security
    // Policy Failure.
    loading_ = false;
    RemovePendingSheet();
    NotifyLoadedSheetAndAllCriticalSubresources(
        Node::kErrorOccurredLoadingSubresource);
  }
  return kLoaded;
}

void LinkStyle::Process() {
  DCHECK(owner_->ShouldProcessStyle());
  const LinkLoadParameters params(
      owner_->RelAttribute(),
      GetCrossOriginAttributeValue(
          owner_->FastGetAttribute(html_names::kCrossoriginAttr)),
      owner_->TypeValue().DeprecatedLower(),
      owner_->AsValue().DeprecatedLower(), owner_->Media().DeprecatedLower(),
      owner_->nonce(), owner_->IntegrityValue(),
      owner_->ImportanceValue().LowerASCII(), owner_->GetReferrerPolicy(),
      owner_->GetNonEmptyURLAttribute(html_names::kHrefAttr),
      owner_->FastGetAttribute(html_names::kImagesrcsetAttr),
      owner_->FastGetAttribute(html_names::kImagesizesAttr));

  WTF::TextEncoding charset = GetCharset();

  if (owner_->RelAttribute().GetIconType() != kInvalidIcon &&
      params.href.IsValid() && !params.href.IsEmpty()) {
    if (!owner_->ShouldLoadLink())
      return;
    if (!GetDocument().GetSecurityOrigin()->CanDisplay(params.href))
      return;
    if (!GetDocument().GetContentSecurityPolicy()->AllowImageFromSource(
            params.href))
      return;
    if (GetDocument().GetFrame() && GetDocument().GetFrame()->Client()) {
      GetDocument().GetFrame()->Client()->DispatchDidChangeIcons(
          owner_->RelAttribute().GetIconType());
    }
  }

  if (!sheet_ && !owner_->LoadLink(params))
    return;

  if (LoadStylesheetIfNeeded(params, charset) == kNotNeeded && sheet_) {
    // we no longer contain a stylesheet, e.g. perhaps rel or type was changed
    ClearSheet();
    GetDocument().GetStyleEngine().SetNeedsActiveStyleUpdate(
        owner_->GetTreeScope());
  }
}

void LinkStyle::SetSheetTitle(const String& title) {
  if (!owner_->IsInDocumentTree() || !owner_->RelAttribute().IsStyleSheet())
    return;

  if (sheet_)
    sheet_->SetTitle(title);

  if (title.IsEmpty() || !IsUnset() || owner_->IsAlternate())
    return;

  const KURL& href = owner_->GetNonEmptyURLAttribute(html_names::kHrefAttr);
  if (href.IsValid() && !href.IsEmpty())
    GetDocument().GetStyleEngine().SetPreferredStylesheetSetNameIfNotSet(title);
}

void LinkStyle::OwnerRemoved() {
  if (StyleSheetIsLoading())
    RemovePendingSheet();

  if (sheet_)
    ClearSheet();
}

void LinkStyle::Trace(Visitor* visitor) {
  visitor->Trace(sheet_);
  LinkResource::Trace(visitor);
  ResourceClient::Trace(visitor);
}

}  // namespace blink
