// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/link_style.h"

#include "base/metrics/histogram_functions.h"
#include "services/network/public/mojom/referrer_policy.mojom-blink.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/html/cross_origin_attribute.h"
#include "third_party/blink/renderer/core/html/html_link_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/inspector/inspector_audits_issue.h"
#include "third_party/blink/renderer/core/loader/fetch_priority_attribute.h"
#include "third_party/blink/renderer/core/loader/link_load_parameters.h"
#include "third_party/blink/renderer/core/loader/resource/css_style_sheet_resource.h"
#include "third_party/blink/renderer/core/loader/subresource_integrity_helper.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
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
  return trimmed_type.empty() ||
         MIMETypeRegistry::IsSupportedStyleSheetMIMEType(trimmed_type);
}

LinkStyle::LinkStyle(HTMLLinkElement* owner)
    : LinkResource(owner),
      disabled_state_(kUnset),
      pending_sheet_type_(PendingSheetType::kNone),
      render_blocking_behavior_(RenderBlockingBehavior::kUnset),
      loading_(false),
      fired_load_(false),
      loaded_sheet_(false) {}

LinkStyle::~LinkStyle() = default;

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

  if (resource->LoadFailedOrCanceled()) {
    AuditsIssue::ReportStylesheetLoadingRequestFailedIssue(
        &GetDocument(), resource->Url(),
        resource->LastResourceRequest().GetDevToolsId(), GetDocument().Url(),
        resource->Options().initiator_info.position.line_,
        resource->Options().initiator_info.position.column_,
        resource->GetResourceError().LocalizedDescription());
  }

  auto* cached_style_sheet = To<CSSStyleSheetResource>(resource);
  // See the comment in pending_script.cc about why this check is necessary
  // here, instead of in the resource fetcher. https://crbug.com/500701.
  if ((!cached_style_sheet->ErrorOccurred() &&
       !owner_->FastGetAttribute(html_names::kIntegrityAttr).empty() &&
       !cached_style_sheet->IntegrityMetadata().empty()) ||
      resource->IsLinkPreload()) {
    ResourceIntegrityDisposition disposition =
        cached_style_sheet->IntegrityDisposition();

    SubresourceIntegrityHelper::DoReport(
        *GetExecutionContext(), cached_style_sheet->IntegrityReportInfo());

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
      Referrer(cached_style_sheet->GetResponse().ResponseUrl(),
               cached_style_sheet->GetReferrerPolicy()),
      cached_style_sheet->Encoding());
  if (cached_style_sheet->GetResourceRequest().IsAdResource()) {
    parser_context->SetIsAdRelated();
  }

  if (StyleSheetContents* parsed_sheet =
          cached_style_sheet->CreateParsedStyleSheetFromCache(parser_context)) {
    if (sheet_)
      ClearSheet();
    sheet_ = MakeGarbageCollected<CSSStyleSheet>(parsed_sheet, *owner_);
    sheet_->SetMediaQueries(
        MediaQuerySet::Create(owner_->Media(), GetExecutionContext()));
    if (owner_->IsInDocumentTree())
      SetSheetTitle(owner_->title());

    loading_ = false;
    parsed_sheet->CheckLoaded();
    parsed_sheet->SetRenderBlocking(render_blocking_behavior_);

    return;
  }

  auto parser_start_time = base::TimeTicks::Now();
  auto* style_sheet = MakeGarbageCollected<StyleSheetContents>(
      parser_context, cached_style_sheet->Url());

  if (sheet_)
    ClearSheet();

  sheet_ = MakeGarbageCollected<CSSStyleSheet>(style_sheet, *owner_);
  sheet_->SetMediaQueries(
      MediaQuerySet::Create(owner_->Media(), GetExecutionContext()));
  if (owner_->IsInDocumentTree())
    SetSheetTitle(owner_->title());

  style_sheet->SetRenderBlocking(render_blocking_behavior_);
  style_sheet->ParseAuthorStyleSheet(cached_style_sheet);

  loading_ = false;
  style_sheet->NotifyLoadedSheet(cached_style_sheet);
  style_sheet->CheckLoaded();

  if (style_sheet->IsCacheableForResource()) {
    const_cast<CSSStyleSheetResource*>(cached_style_sheet)
        ->SaveParsedStyleSheet(style_sheet);
  }
  base::UmaHistogramMicrosecondsTimes(
      "Blink.CSSStyleSheetResource.ParseTime",
      base::TimeTicks::Now() - parser_start_time);
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

void LinkStyle::SetToPendingState() {
  DCHECK_LT(pending_sheet_type_, PendingSheetType::kBlocking);
  AddPendingSheet(PendingSheetType::kBlocking);
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

  if (pending_sheet_type_ == PendingSheetType::kNonBlocking)
    return;
  GetDocument().GetStyleEngine().AddPendingBlockingSheet(*owner_,
                                                         pending_sheet_type_);
}

void LinkStyle::RemovePendingSheet() {
  DCHECK(owner_);
  PendingSheetType type = pending_sheet_type_;
  pending_sheet_type_ = PendingSheetType::kNone;

  if (type == PendingSheetType::kNone)
    return;
  if (type == PendingSheetType::kNonBlocking) {
    // Tell StyleEngine to re-compute styleSheets of this owner_'s treescope.
    GetDocument().GetStyleEngine().ModifiedStyleSheetCandidateNode(*owner_);
    return;
  }

  GetDocument().GetStyleEngine().RemovePendingBlockingSheet(*owner_, type);
}

void LinkStyle::SetDisabledState(bool disabled) {
  LinkStyle::DisabledState old_disabled_state = disabled_state_;
  disabled_state_ = disabled ? kDisabled : kEnabledViaScript;
  // Whenever the disabled attribute is removed, set the link element's
  // explicitly enabled attribute to true.
  if (!disabled)
    explicitly_enabled_ = true;
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
      AddPendingSheet(PendingSheetType::kBlocking);

    // Check #3: A main sheet becomes enabled while it was still loading and
    // after it was disabled via script. It takes really terrible code to make
    // this happen (a double toggle for no reason essentially). This happens
    // on virtualplastic.net, which manages to do about 12 enable/disables on
    // only 3 sheets. :)
    if (!owner_->RelAttribute().IsAlternate() &&
        disabled_state_ == kEnabledViaScript && old_disabled_state == kDisabled)
      AddPendingSheet(PendingSheetType::kBlocking);

    // If the sheet is already loading just bail.
    return;
  }

  if (sheet_) {
    DCHECK(disabled) << "If link is being enabled, sheet_ shouldn't exist yet";
    ClearSheet();
    GetDocument().GetStyleEngine().SetNeedsActiveStyleUpdate(
        owner_->GetTreeScope());
    return;
  }

  if (disabled_state_ == kEnabledViaScript && owner_->ShouldProcessStyle())
    Process(LinkLoadParameters::Reason::kDefault);
}

LinkStyle::LoadReturnValue LinkStyle::LoadStylesheetIfNeeded(
    const LinkLoadParameters& params,
    const WTF::TextEncoding& charset) {
  if (GetDocument().StatePreservingAtomicMoveInProgress()) {
    return kNotNeeded;
  }

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
  if (!title.empty() && !owner_->IsAlternate() &&
      disabled_state_ != kEnabledViaScript && owner_->IsInDocumentTree()) {
    GetDocument().GetStyleEngine().SetPreferredStylesheetSetNameIfNotSet(title);
  }

  bool media_query_matches = true;
  LocalFrame* frame = LoadingFrame();
  if (!owner_->Media().empty() && frame) {
    MediaQuerySet* media =
        MediaQuerySet::Create(owner_->Media(), GetExecutionContext());
    MediaQueryEvaluator* evaluator =
        MakeGarbageCollected<MediaQueryEvaluator>(frame);
    media_query_matches = evaluator->Eval(*media);
  }

  // Don't hold up layout tree construction and script execution on
  // stylesheets that are not needed for the layout at the moment.
  bool critical_style = media_query_matches && !owner_->IsAlternate();
  auto type_and_behavior = ComputePendingSheetTypeAndRenderBlockingBehavior(
      *owner_, critical_style, owner_->IsCreatedByParser());
  PendingSheetType type = type_and_behavior.first;

  AddPendingSheet(type);

  // Load stylesheets that are not needed for the layout immediately with low
  // priority.  When the link element is created by scripts, load the
  // stylesheets asynchronously but in high priority.
  FetchParameters::DeferOption defer_option =
      !critical_style ? FetchParameters::kLazyLoad : FetchParameters::kNoDefer;

  render_blocking_behavior_ = type_and_behavior.second;
  owner_->LoadStylesheet(params, charset, defer_option, this,
                         render_blocking_behavior_);

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

void LinkStyle::Process(LinkLoadParameters::Reason reason) {
  DCHECK(owner_->ShouldProcessStyle());
  const LinkLoadParameters params(
      owner_->RelAttribute(),
      GetCrossOriginAttributeValue(
          owner_->FastGetAttribute(html_names::kCrossoriginAttr)),
      owner_->TypeValue().DeprecatedLower(),
      owner_->AsValue().DeprecatedLower(), owner_->Media().DeprecatedLower(),
      owner_->nonce(), owner_->IntegrityValue(),
      owner_->FetchPriorityHintValue().LowerASCII(),
      owner_->GetReferrerPolicy(),
      owner_->GetNonEmptyURLAttribute(html_names::kHrefAttr),
      owner_->FastGetAttribute(html_names::kImagesrcsetAttr),
      owner_->FastGetAttribute(html_names::kImagesizesAttr),
      owner_->FastGetAttribute(html_names::kBlockingAttr), reason);

  WTF::TextEncoding charset = GetCharset();

  if (owner_->RelAttribute().GetIconType() !=
          mojom::blink::FaviconIconType::kInvalid &&
      params.href.IsValid() && !params.href.IsEmpty()) {
    if (!owner_->ShouldLoadLink())
      return;
    if (!GetExecutionContext())
      return;
    if (!GetExecutionContext()->GetSecurityOrigin()->CanDisplay(params.href))
      return;
    if (!GetExecutionContext()
             ->GetContentSecurityPolicy()
             ->AllowImageFromSource(params.href, params.href,
                                    RedirectStatus::kNoRedirect)) {
      return;
    }
    if (GetDocument().GetFrame())
      GetDocument().GetFrame()->UpdateFaviconURL();
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

  if (title.empty() || !IsUnset() || owner_->IsAlternate())
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

void LinkStyle::UnblockRenderingForPendingSheet() {
  DCHECK(StyleSheetIsLoading());
  if (pending_sheet_type_ == PendingSheetType::kDynamicRenderBlocking) {
    GetDocument().GetStyleEngine().RemovePendingBlockingSheet(
        *owner_, pending_sheet_type_);
    pending_sheet_type_ = PendingSheetType::kNonBlocking;
  }
}

void LinkStyle::Trace(Visitor* visitor) const {
  visitor->Trace(sheet_);
  LinkResource::Trace(visitor);
  ResourceClient::Trace(visitor);
}

}  // namespace blink
