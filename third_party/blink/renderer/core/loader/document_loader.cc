/*
 * Copyright (C) 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/loader/document_loader.h"

#include <memory>
#include <utility>
#include "base/auto_reset.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/default_tick_clock.h"
#include "services/network/public/cpp/features.h"
#include "third_party/blink/public/common/origin_policy/origin_policy.h"
#include "third_party/blink/public/mojom/commit_result/commit_result.mojom-blink.h"
#include "third_party/blink/public/platform/modules/service_worker/web_service_worker_network_provider.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/document_init.h"
#include "third_party/blink/renderer/core/dom/document_parser.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/scriptable_document_parser.h"
#include "third_party/blink/renderer/core/dom/user_gesture_indicator.h"
#include "third_party/blink/renderer/core/dom/weak_identifier_map.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/deprecation.h"
#include "third_party/blink/renderer/core/frame/frame_console.h"
#include "third_party/blink/renderer/core/frame/intervention.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/inspector/inspector_trace_events.h"
#include "third_party/blink/renderer/core/inspector/main_thread_debugger.h"
#include "third_party/blink/renderer/core/loader/alternate_signed_exchange_resource_info.h"
#include "third_party/blink/renderer/core/loader/appcache/application_cache_host_for_frame.h"
#include "third_party/blink/renderer/core/loader/frame_fetch_context.h"
#include "third_party/blink/renderer/core/loader/frame_loader.h"
#include "third_party/blink/renderer/core/loader/idleness_detector.h"
#include "third_party/blink/renderer/core/loader/interactive_detector.h"
#include "third_party/blink/renderer/core/loader/mixed_content_checker.h"
#include "third_party/blink/renderer/core/loader/prefetched_signed_exchange_manager.h"
#include "third_party/blink/renderer/core/loader/preload_helper.h"
#include "third_party/blink/renderer/core/loader/private/frame_client_hints_preferences_context.h"
#include "third_party/blink/renderer/core/loader/progress_tracker.h"
#include "third_party/blink/renderer/core/loader/subresource_filter.h"
#include "third_party/blink/renderer/core/origin_trials/origin_trial_context.h"
#include "third_party/blink/renderer/core/page/frame_tree.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/core/timing/window_performance.h"
#include "third_party/blink/renderer/platform/bindings/microtask.h"
#include "third_party/blink/renderer/platform/bindings/script_forbidden_scope.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_isolate_data.h"
#include "third_party/blink/renderer/platform/loader/cors/cors.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_initiator_type_names.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_parameters.h"
#include "third_party/blink/renderer/platform/loader/fetch/memory_cache.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader_options.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_timing_info.h"
#include "third_party/blink/renderer/platform/loader/fetch/source_keyed_cached_metadata_handler.h"
#include "third_party/blink/renderer/platform/loader/fetch/unique_identifier.h"
#include "third_party/blink/renderer/platform/loader/ftp_directory_listing.h"
#include "third_party/blink/renderer/platform/loader/static_data_navigation_body_loader.h"
#include "third_party/blink/renderer/platform/mhtml/archive_resource.h"
#include "third_party/blink/renderer/platform/mhtml/mhtml_archive.h"
#include "third_party/blink/renderer/platform/network/content_security_policy_response_headers.h"
#include "third_party/blink/renderer/platform/network/encoded_form_data.h"
#include "third_party/blink/renderer/platform/network/http_names.h"
#include "third_party/blink/renderer/platform/network/http_parsers.h"
#include "third_party/blink/renderer/platform/network/network_utils.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_scheduler.h"
#include "third_party/blink/renderer/platform/weborigin/scheme_registry.h"
#include "third_party/blink/renderer/platform/weborigin/security_policy.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

DocumentLoader::DocumentLoader(
    LocalFrame* frame,
    WebNavigationType navigation_type,
    std::unique_ptr<WebNavigationParams> navigation_params)
    : params_(std::move(navigation_params)),
      frame_(frame),
      load_type_(params_->frame_load_type),
      is_client_redirect_(params_->is_client_redirect),
      replaces_current_history_item_(false),
      data_received_(false),
      navigation_type_(navigation_type),
      document_load_timing_(*this),
      service_worker_network_provider_(
          std::move(params_->service_worker_network_provider)),
      was_blocked_by_csp_(false),
      state_(kNotStarted),
      in_commit_data_(false),
      data_buffer_(SharedBuffer::Create()),
      devtools_navigation_token_(params_->devtools_navigation_token),
      had_sticky_activation_(params_->is_user_activated),
      is_browser_initiated_(params_->is_browser_initiated),
      was_discarded_(params_->was_discarded),
      use_counter_(),
      clock_(params_->tick_clock ? params_->tick_clock
                                 : base::DefaultTickClock::GetInstance()) {
  DCHECK(frame_);

  // TODO(nasko): How should this work with OOPIF?
  // The MHTMLArchive is parsed as a whole, but can be constructed from frames
  // in multiple processes. In that case, which process should parse it and how
  // should the output be spread back across multiple processes?
  if (!frame_->IsMainFrame()) {
    if (auto* parent = DynamicTo<LocalFrame>(frame_->Tree().Parent()))
      archive_ = parent->Loader().GetDocumentLoader()->archive_;
  }

  url_ = params_->url;
  original_url_ = url_;
  had_transient_activation_ = LocalFrame::HasTransientUserActivation(frame_) ||
                              params_->had_transient_activation;
  http_method_ = params_->http_method;
  if (params_->referrer.IsEmpty()) {
    referrer_ = Referrer(Referrer::NoReferrer(), params_->referrer_policy);
  } else {
    referrer_ = Referrer(params_->referrer, params_->referrer_policy);
  }

  original_referrer_ = referrer_;
  http_body_ = params_->http_body;
  http_content_type_ = params_->http_content_type;
  origin_policy_ = params_->origin_policy;
  requestor_origin_ = params_->requestor_origin;
  unreachable_url_ = params_->unreachable_url;
  if (frame_->IsMainFrame()) {
    previews_state_ = params_->previews_state;
  } else {
    // Subframes inherit previews state from the main frame.
    if (auto* parent = DynamicTo<LocalFrame>(frame_->Tree().Parent()))
      previews_state_ = parent->Loader().GetDocumentLoader()->previews_state_;
  }
  ip_address_space_ = params_->ip_address_space;
  grant_load_local_resources_ = params_->grant_load_local_resources;
  force_fetch_cache_mode_ = params_->force_fetch_cache_mode;
  frame_policy_ = params_->frame_policy;

  WebNavigationTimings& timings = params_->navigation_timings;
  if (!timings.input_start.is_null())
    document_load_timing_.SetInputStart(timings.input_start);
  if (timings.navigation_start.is_null()) {
    // If we don't have any navigation timings yet, it starts now.
    document_load_timing_.SetNavigationStart(clock_->NowTicks());
  } else {
    document_load_timing_.SetNavigationStart(timings.navigation_start);
    if (!timings.redirect_start.is_null()) {
      document_load_timing_.SetRedirectStart(timings.redirect_start);
      document_load_timing_.SetRedirectEnd(timings.redirect_end);
    }
    if (!timings.fetch_start.is_null()) {
      // If we started fetching, we should have started the navigation.
      DCHECK(!timings.navigation_start.is_null());
      document_load_timing_.SetFetchStart(timings.fetch_start);
    }
  }

  // TODO(japhet): This is needed because the browser process DCHECKs if the
  // first entry we commit in a new frame has replacement set. It's unclear
  // whether the DCHECK is right, investigate removing this special case.
  // TODO(dgozman): we should get rid of this boolean field, and make client
  // responsible for it's own view of "replaces current item", based on the
  // frame load type.
  replaces_current_history_item_ =
      load_type_ == WebFrameLoadType::kReplaceCurrentItem &&
      (!frame_->Loader().Opener() || !url_.IsEmpty());

  // The document URL needs to be added to the head of the list as that is
  // where the redirects originated.
  if (is_client_redirect_)
    redirect_chain_.push_back(frame_->GetDocument()->Url());

  if (!params_->origin_to_commit.IsNull())
    origin_to_commit_ = params_->origin_to_commit.Get()->IsolatedCopy();

  loading_url_as_empty_document_ =
      !params_->is_static_data && WillLoadUrlAsEmpty(url_);
  loading_srcdoc_ = url_.IsAboutSrcdocURL();

  if (loading_srcdoc_) {
    // about:srcdoc always inherits CSP from its parent.
    ContentSecurityPolicy* parent_csp = frame_->Tree()
                                            .Parent()
                                            ->GetSecurityContext()
                                            ->GetContentSecurityPolicy();
    content_security_policy_ = MakeGarbageCollected<ContentSecurityPolicy>();
    content_security_policy_->CopyStateFrom(parent_csp);
    content_security_policy_->CopyPluginTypesFrom(parent_csp);
  } else if (!loading_url_as_empty_document_) {
    content_security_policy_ =
        CreateCSP(params_->response.ToResourceResponse(), origin_policy_);
    if (!content_security_policy_) {
      // Loading the document was blocked by the CSP check. Pretend that
      // this was an empty document instead and don't reuse the
      // original URL (https://crbug.com/622385).
      // TODO(mkwst):  Remove this once XFO moves to the browser.
      // https://crbug.com/555418.

      // Update |origin_to_commit_| to contain an opaque origin with precursor
      // information that is consistent with the final request URL.
      // Note: We can't use |url_| for the origin calculation because
      // we need to take into account any redirects that may have occurred.
      const auto request_url_origin = blink::SecurityOrigin::Create(
          params_->response.ToResourceResponse().CurrentRequestUrl());
      origin_to_commit_ = request_url_origin->DeriveNewOpaqueOrigin();

      was_blocked_by_csp_ = true;
      KURL blocked_url = SecurityOrigin::UrlWithUniqueOpaqueOrigin();
      original_url_ = blocked_url;
      url_ = blocked_url;
      params_->url = blocked_url;
      WebNavigationParams::FillStaticResponse(params_.get(), "text/html",
                                              "UTF-8", "");
    }
  }

  if (!GetFrameLoader().StateMachine()->CreatingInitialEmptyDocument())
    redirect_chain_.push_back(url_);

  response_ = params_->response.ToResourceResponse();

  for (auto feature : params_->initiator_origin_trial_features) {
    // Convert from int to OriginTrialFeature. These values are passed between
    // blink navigations. OriginTrialFeature isn't visible outside of blink (and
    // doesn't need to be) so the values are transferred outside of blink as
    // ints and casted to OriginTrialFeature once being processed in blink.
    initiator_origin_trial_features_.push_back(
        static_cast<OriginTrialFeature>(feature));
  }

  // For back/forward navigations, the browser passed a history item to use at
  // commit time in |params_|. Set it as the current history item of this
  // DocumentLoader. For other navigations, |history_item_| will be created when
  // the FrameLoader calls SetHistoryItemStateForCommit.
  if (IsBackForwardLoadType(params_->frame_load_type)) {
    HistoryItem* history_item = params_->history_item;
    DCHECK(history_item);
    history_item_ = history_item;
  }

  base_url_override_for_bundled_exchanges_ =
      params_->base_url_override_for_bundled_exchanges;
}

FrameLoader& DocumentLoader::GetFrameLoader() const {
  DCHECK(frame_);
  return frame_->Loader();
}

LocalFrameClient& DocumentLoader::GetLocalFrameClient() const {
  DCHECK(frame_);
  LocalFrameClient* client = frame_->Client();
  // LocalFrame clears its |m_client| only after detaching all DocumentLoaders
  // (i.e. calls detachFromFrame() which clears |frame_|) owned by the
  // LocalFrame's FrameLoader. So, if |frame_| is non nullptr, |client| is
  // also non nullptr.
  DCHECK(client);
  return *client;
}

DocumentLoader::~DocumentLoader() {
  DCHECK(!frame_);
  DCHECK(!application_cache_host_);
  DCHECK_EQ(state_, kSentDidFinishLoad);
}

void DocumentLoader::Trace(blink::Visitor* visitor) {
  visitor->Trace(archive_);
  visitor->Trace(frame_);
  visitor->Trace(history_item_);
  visitor->Trace(parser_);
  visitor->Trace(subresource_filter_);
  visitor->Trace(resource_loading_hints_);
  visitor->Trace(document_load_timing_);
  visitor->Trace(application_cache_host_);
  visitor->Trace(content_security_policy_);
  visitor->Trace(cached_metadata_handler_);
  visitor->Trace(prefetched_signed_exchange_manager_);
  visitor->Trace(use_counter_);
}

uint64_t DocumentLoader::MainResourceIdentifier() const {
  return main_resource_identifier_;
}

ResourceTimingInfo* DocumentLoader::GetNavigationTimingInfo() const {
  return navigation_timing_info_.get();
}

const KURL& DocumentLoader::OriginalUrl() const {
  return original_url_;
}

const Referrer& DocumentLoader::OriginalReferrer() const {
  return original_referrer_;
}

void DocumentLoader::SetSubresourceFilter(
    SubresourceFilter* subresource_filter) {
  subresource_filter_ = subresource_filter;
}

const KURL& DocumentLoader::Url() const {
  return url_;
}

const AtomicString& DocumentLoader::HttpMethod() const {
  return http_method_;
}

const Referrer& DocumentLoader::GetReferrer() const {
  return referrer_;
}

void DocumentLoader::SetServiceWorkerNetworkProvider(
    std::unique_ptr<WebServiceWorkerNetworkProvider> provider) {
  service_worker_network_provider_ = std::move(provider);
}

void DocumentLoader::DispatchLinkHeaderPreloads(
    const base::Optional<ViewportDescription>& viewport,
    PreloadHelper::MediaPreloadPolicy media_policy) {
  DCHECK_GE(state_, kCommitted);
  PreloadHelper::LoadLinksFromHeader(
      GetResponse().HttpHeaderField(http_names::kLink),
      GetResponse().CurrentRequestUrl(), *frame_, frame_->GetDocument(),
      PreloadHelper::kOnlyLoadResources, media_policy, viewport,
      nullptr /* alternate_resource_info */,
      base::nullopt /* recursive_prefetch_token */);
}

void DocumentLoader::DidChangePerformanceTiming() {
  if (frame_ && state_ >= kCommitted) {
    GetLocalFrameClient().DidChangePerformanceTiming();
  }
}

void DocumentLoader::DidObserveLoadingBehavior(LoadingBehaviorFlag behavior) {
  if (frame_) {
    DCHECK_GE(state_, kCommitted);
    GetLocalFrameClient().DidObserveLoadingBehavior(behavior);
  }
}

void DocumentLoader::MarkAsCommitted() {
  DCHECK_LT(state_, kCommitted);
  state_ = kCommitted;
}

// static
WebHistoryCommitType DocumentLoader::LoadTypeToCommitType(
    WebFrameLoadType type) {
  switch (type) {
    case WebFrameLoadType::kStandard:
      return kWebStandardCommit;
    case WebFrameLoadType::kBackForward:
      return kWebBackForwardCommit;
    case WebFrameLoadType::kReload:
    case WebFrameLoadType::kReplaceCurrentItem:
    case WebFrameLoadType::kReloadBypassingCache:
      return kWebHistoryInertCommit;
  }
  NOTREACHED();
  return kWebHistoryInertCommit;
}

static SinglePageAppNavigationType CategorizeSinglePageAppNavigation(
    SameDocumentNavigationSource same_document_navigation_source,
    WebFrameLoadType frame_load_type) {
  // |SinglePageAppNavigationType| falls into this grid according to different
  // combinations of |WebFrameLoadType| and |SameDocumentNavigationSource|:
  //
  //                 HistoryApi           Default
  //  kBackForward   illegal              otherFragmentNav
  // !kBackForward   sameDocBack/Forward  historyPushOrReplace
  switch (same_document_navigation_source) {
    case kSameDocumentNavigationDefault:
      if (frame_load_type == WebFrameLoadType::kBackForward) {
        return kSPANavTypeSameDocumentBackwardOrForward;
      }
      return kSPANavTypeOtherFragmentNavigation;
    case kSameDocumentNavigationHistoryApi:
      // It's illegal to have both kSameDocumentNavigationHistoryApi and
      // WebFrameLoadType::kBackForward.
      DCHECK(frame_load_type != WebFrameLoadType::kBackForward);
      return kSPANavTypeHistoryPushStateOrReplaceState;
  }
  NOTREACHED();
  return kSPANavTypeSameDocumentBackwardOrForward;
}

void DocumentLoader::UpdateForSameDocumentNavigation(
    const KURL& new_url,
    SameDocumentNavigationSource same_document_navigation_source,
    scoped_refptr<SerializedScriptValue> data,
    HistoryScrollRestorationType scroll_restoration_type,
    WebFrameLoadType type,
    Document* initiating_document) {
  SinglePageAppNavigationType single_page_app_navigation_type =
      CategorizeSinglePageAppNavigation(same_document_navigation_source, type);
  UMA_HISTOGRAM_ENUMERATION(
      "RendererScheduler.UpdateForSameDocumentNavigationCount",
      single_page_app_navigation_type, kSPANavTypeCount);

  TRACE_EVENT1("blink", "FrameLoader::updateForSameDocumentNavigation", "url",
               new_url.GetString().Ascii());

  // Generate start and stop notifications only when loader is completed so that
  // we don't fire them for fragment redirection that happens in window.onload
  // handler. See https://bugs.webkit.org/show_bug.cgi?id=31838
  // Do not fire the notifications if the frame is concurrently navigating away
  // from the document, since a new document is already loading.
  bool was_loading = frame_->IsLoading();
  if (!was_loading)
    GetLocalFrameClient().DidStartLoading();

  // Update the data source's request with the new URL to fake the URL change
  frame_->GetDocument()->SetURL(new_url);

  KURL old_url = url_;
  original_url_ = new_url;
  url_ = new_url;
  replaces_current_history_item_ = type != WebFrameLoadType::kStandard;
  if (same_document_navigation_source == kSameDocumentNavigationHistoryApi) {
    http_method_ = http_names::kGET;
    http_body_ = nullptr;
  }
  redirect_chain_.clear();
  if (is_client_redirect_)
    redirect_chain_.push_back(old_url);
  redirect_chain_.push_back(new_url);

  SetHistoryItemStateForCommit(
      history_item_.Get(), type,
      same_document_navigation_source == kSameDocumentNavigationHistoryApi
          ? HistoryNavigationType::kHistoryApi
          : HistoryNavigationType::kFragment);
  history_item_->SetDocumentState(frame_->GetDocument()->GetDocumentState());
  if (same_document_navigation_source == kSameDocumentNavigationHistoryApi) {
    history_item_->SetStateObject(std::move(data));
    history_item_->SetScrollRestorationType(scroll_restoration_type);
  }
  WebHistoryCommitType commit_type = LoadTypeToCommitType(type);
  frame_->GetFrameScheduler()->DidCommitProvisionalLoad(
      commit_type == kWebHistoryInertCommit,
      FrameScheduler::NavigationType::kSameDocument);

  GetLocalFrameClient().DidFinishSameDocumentNavigation(
      history_item_.Get(), commit_type, initiating_document);
  probe::DidNavigateWithinDocument(frame_);
  if (!was_loading)
    GetLocalFrameClient().DidStopLoading();
}

const KURL& DocumentLoader::UrlForHistory() const {
  return UnreachableURL().IsEmpty() ? Url() : UnreachableURL();
}

void DocumentLoader::FillNavigationParamsForErrorPage(
    WebNavigationParams* params) {
  params->http_method = http_method_;
  params->referrer = referrer_.referrer;
  params->referrer_policy = referrer_.referrer_policy;
  params->http_body = WebHTTPBody(http_body_.get());
  params->http_content_type = http_content_type_;
  params->previews_state = previews_state_;
  params->requestor_origin = requestor_origin_;
  params->origin_policy = origin_policy_;
  // Force the commit origin for the error page to be opaque and
  // contain precursor information from the document's origin.
  // This is only used for renderer-side enforcement of CSP and the
  // document origin used here is the origin of the document that was blocked.
  params->origin_to_commit =
      frame_->GetDocument()->GetSecurityOrigin()->DeriveNewOpaqueOrigin();
}

void DocumentLoader::SetHistoryItemStateForCommit(
    HistoryItem* old_item,
    WebFrameLoadType load_type,
    HistoryNavigationType navigation_type) {
  if (!history_item_ || !IsBackForwardLoadType(load_type))
    history_item_ = MakeGarbageCollected<HistoryItem>();

  history_item_->SetURL(UrlForHistory());
  scoped_refptr<const SecurityOrigin> origin =
      requestor_origin_ ? requestor_origin_
                        : SecurityOrigin::CreateUniqueOpaque();
  history_item_->SetReferrer(SecurityPolicy::GenerateReferrer(
      referrer_.referrer_policy, origin, history_item_->Url(),
      referrer_.referrer));
  if (DeprecatedEqualIgnoringCase(http_method_, "POST")) {
    // FIXME: Eventually we have to make this smart enough to handle the case
    // where we have a stream for the body to handle the "data interspersed with
    // files" feature.
    history_item_->SetFormData(http_body_);
    history_item_->SetFormContentType(http_content_type_);
  } else {
    history_item_->SetFormData(nullptr);
    history_item_->SetFormContentType(g_null_atom);
  }

  // Don't propagate state from the old item to the new item if there isn't an
  // old item (obviously), or if this is a back/forward navigation, since we
  // explicitly want to restore the state we just committed.
  if (!old_item || IsBackForwardLoadType(load_type))
    return;
  // Don't propagate state from the old item if this is a different-document
  // navigation, unless the before and after pages are logically related. This
  // means they have the same url (ignoring fragment) and the new item was
  // loaded via reload or client redirect.
  WebHistoryCommitType history_commit_type = LoadTypeToCommitType(load_type);
  if (navigation_type == HistoryNavigationType::kDifferentDocument &&
      (history_commit_type != kWebHistoryInertCommit ||
       !EqualIgnoringFragmentIdentifier(old_item->Url(), history_item_->Url())))
    return;
  history_item_->SetDocumentSequenceNumber(old_item->DocumentSequenceNumber());

  history_item_->CopyViewStateFrom(old_item);
  history_item_->SetScrollRestorationType(old_item->ScrollRestorationType());

  // The item sequence number determines whether items are "the same", such
  // back/forward navigation between items with the same item sequence number is
  // a no-op. Only treat this as identical if the navigation did not create a
  // back/forward entry and the url is identical or it was loaded via
  // history.replaceState().
  if (history_commit_type == kWebHistoryInertCommit &&
      (navigation_type == HistoryNavigationType::kHistoryApi ||
       old_item->Url() == history_item_->Url())) {
    history_item_->SetStateObject(old_item->StateObject());
    history_item_->SetItemSequenceNumber(old_item->ItemSequenceNumber());
  }
}

void DocumentLoader::BodyCodeCacheReceived(mojo_base::BigBuffer data) {
  if (cached_metadata_handler_) {
    cached_metadata_handler_->SetSerializedCachedMetadata(std::move(data));
  }
}

void DocumentLoader::BodyDataReceived(base::span<const char> data) {
  TRACE_EVENT0("loading", "DocumentLoader::BodyDataReceived");
  GetFrameLoader().Progress().IncrementProgress(main_resource_identifier_,
                                                data.size());
  probe::DidReceiveData(probe::ToCoreProbeSink(GetFrame()),
                        main_resource_identifier_, this, data.data(),
                        data.size());

  TRACE_EVENT1("loading", "DocumentLoader::HandleData", "length", data.size());

  DCHECK(data.data());
  DCHECK(data.size());
  DCHECK(!frame_->GetPage()->Paused());
  time_of_last_data_received_ = clock_->NowTicks();

  if (listing_ftp_directory_ || loading_mhtml_archive_) {
    // 1) Ftp directory listings accumulate data buffer and transform it later
    //    to the actual document content.
    // 2) Mhtml archives accumulate data buffer and parse it as mhtml later
    //    to retrieve the actual document content.
    data_buffer_->Append(data.data(), data.size());
    return;
  }

  ProcessDataBuffer(data.data(), data.size());
}

void DocumentLoader::BodyLoadingFinished(
    base::TimeTicks completion_time,
    int64_t total_encoded_data_length,
    int64_t total_encoded_body_length,
    int64_t total_decoded_body_length,
    bool should_report_corb_blocking,
    const base::Optional<WebURLError>& error) {
  TRACE_EVENT0("loading", "DocumentLoader::BodyLoadingFinished");
  response_.SetEncodedDataLength(total_encoded_data_length);
  response_.SetEncodedBodyLength(total_encoded_body_length);
  response_.SetDecodedBodyLength(total_decoded_body_length);

  if (!error) {
    GetFrameLoader().Progress().CompleteProgress(main_resource_identifier_);
    probe::DidFinishLoading(
        probe::ToCoreProbeSink(GetFrame()), main_resource_identifier_, this,
        completion_time, total_encoded_data_length, total_decoded_body_length,
        should_report_corb_blocking);
    if (response_.IsHTTP()) {
      navigation_timing_info_->SetFinalResponse(response_);
      navigation_timing_info_->AddFinalTransferSize(
          total_encoded_data_length == -1 ? 0 : total_encoded_data_length);
      if (response_.HttpStatusCode() < 400 && report_timing_info_to_parent_) {
        navigation_timing_info_->SetLoadResponseEnd(completion_time);
        if (state_ >= kCommitted) {
          // Note that we currently lose timing info for empty documents,
          // which will be fixed with synchronous commit.
          // Main resource timing information is reported through the owner
          // to be passed to the parent frame, if appropriate.
          frame_->Owner()->AddResourceTiming(*navigation_timing_info_);
        }
        frame_->SetShouldSendResourceTimingInfoToParent(false);
      }
    }
    FinishedLoading(completion_time);
    return;
  }

  ResourceError resource_error = error.value();
  if (network_utils::IsCertificateTransparencyRequiredError(
          resource_error.ErrorCode())) {
    GetUseCounterHelper().Count(
        WebFeature::kCertificateTransparencyRequiredErrorOnResourceLoad,
        GetFrame());
  }
  GetFrameLoader().Progress().CompleteProgress(main_resource_identifier_);
  probe::DidFailLoading(probe::ToCoreProbeSink(GetFrame()),
                        main_resource_identifier_, this, resource_error);
  GetFrame()->Console().DidFailLoading(this, main_resource_identifier_,
                                       resource_error);
  LoadFailed(resource_error);
}

void DocumentLoader::LoadFailed(const ResourceError& error) {
  body_loader_.reset();
  virtual_time_pauser_.UnpauseVirtualTime();

  if (!error.IsCancellation() && frame_->Owner())
    frame_->Owner()->RenderFallbackContent(frame_);

  WebHistoryCommitType history_commit_type = LoadTypeToCommitType(load_type_);
  DCHECK_EQ(kCommitted, state_);
  if (frame_->GetDocument()->Parser())
    frame_->GetDocument()->Parser()->StopParsing();
  state_ = kSentDidFinishLoad;
  GetLocalFrameClient().DispatchDidFailLoad(error, history_commit_type);
  GetFrameLoader().DidFinishNavigation(
      FrameLoader::NavigationFinishState::kFailure);
  DCHECK_EQ(kSentDidFinishLoad, state_);
  params_ = nullptr;
}

void DocumentLoader::FinishedLoading(base::TimeTicks finish_time) {
  body_loader_.reset();
  virtual_time_pauser_.UnpauseVirtualTime();

  DCHECK(frame_->Loader().StateMachine()->CreatingInitialEmptyDocument() ||
         !frame_->GetPage()->Paused() ||
         MainThreadDebugger::Instance()->IsPaused());

  if (listing_ftp_directory_) {
    data_buffer_ = GenerateFtpDirectoryListingHtml(
        response_.CurrentRequestUrl(), data_buffer_.get());
    ProcessDataBuffer();
  }

  if (loading_mhtml_archive_ && state_ < kCommitted) {
    FinalizeMHTMLArchiveLoad();
  }

  // We should not call FinishedLoading before committing navigation,
  // except for the mhtml case. When loading an MHTML archive, the whole archive
  // has to be validated before committing the navigation. The validation
  // process loads the entire body of the archive, which will move the state to
  // FinishedLoading.
  if (!loading_mhtml_archive_)
    DCHECK_GE(state_, kCommitted);

  base::TimeTicks response_end_time = finish_time;
  if (response_end_time.is_null())
    response_end_time = time_of_last_data_received_;
  if (response_end_time.is_null())
    response_end_time = clock_->NowTicks();
  GetTiming().SetResponseEnd(response_end_time);

  if (!frame_)
    return;

  if (parser_) {
    if (parser_blocked_count_) {
      finish_loading_when_parser_resumed_ = true;
    } else {
      parser_->Finish();
      parser_.Clear();
    }
  }
}

void DocumentLoader::FinalizeMHTMLArchiveLoad() {
  if (!frame_->IsMainFrame()) {
    // Only the top-frame can load MHTML.
    frame_->Console().AddMessage(ConsoleMessage::Create(
        mojom::ConsoleMessageSource::kJavaScript,
        mojom::ConsoleMessageLevel::kError,
        "Attempted to load a multipart archive into an subframe: " +
            url_.GetString()));
  } else {
    archive_ = MHTMLArchive::Create(url_, data_buffer_);
    archive_load_result_ = archive_->LoadResult();
    if (archive_load_result_ != mojom::MHTMLLoadResult::kSuccess) {
      archive_.Clear();
      // Log if attempting to load an invalid archive resource.
      frame_->Console().AddMessage(ConsoleMessage::Create(
          mojom::ConsoleMessageSource::kJavaScript,
          mojom::ConsoleMessageLevel::kError,
          "Malformed multipart archive: " + url_.GetString()));
    }
  }
  data_buffer_ = nullptr;
}

void DocumentLoader::HandleRedirect(const KURL& current_request_url) {
  // Browser process should have already checked that redirecting url is
  // allowed to display content from the target origin.
  // When the referrer page is in an unsigned bundled exchanges file in local
  // (eg: file:///tmp/a.wbn), Chrome internally redirects the navigation to the
  // page (eg: https://example.com/page.html) inside the bundled exchanges file
  // to the file's URL (file:///tmp/a.wbn?https://example.com/page.html). In
  // this case, CanDisplay() returns false, and
  // base_url_override_for_bundled_exchanges must not be null.
  CHECK(SecurityOrigin::Create(current_request_url)->CanDisplay(url_) ||
        !params_->base_url_override_for_bundled_exchanges.IsNull());

  DCHECK(!GetTiming().FetchStart().is_null());
  redirect_chain_.push_back(url_);
  GetTiming().AddRedirect(current_request_url, url_);
}

bool DocumentLoader::ShouldReportTimingInfoToParent() {
  DCHECK(frame_);
  // <iframe>s should report the initial navigation requested by the parent
  // document, but not subsequent navigations.
  if (!frame_->Owner())
    return false;
  // Note that this can be racy since this information is forwarded over IPC
  // when crossing process boundaries.
  if (!frame_->should_send_resource_timing_info_to_parent())
    return false;
  // Do not report iframe navigation that restored from history, since its
  // location may have been changed after initial navigation,
  if (load_type_ == WebFrameLoadType::kBackForward) {
    // ...and do not report subsequent navigations in the iframe too.
    frame_->SetShouldSendResourceTimingInfoToParent(false);
    return false;
  }
  return true;
}

ContentSecurityPolicy* DocumentLoader::CreateCSP(
    const ResourceResponse& response,
    const base::Optional<WebOriginPolicy>& origin_policy) {
  auto* csp = MakeGarbageCollected<ContentSecurityPolicy>();
  csp->SetOverrideURLForSelf(response.CurrentRequestUrl());

  if (!frame_->GetSettings()->BypassCSP()) {
    csp->DidReceiveHeaders(ContentSecurityPolicyResponseHeaders(response));

    // Handle OriginPolicy. We can skip the entire block if the OP policies have
    // already been passed down.
    if (origin_policy.has_value() &&
        !csp->HasPolicyFromSource(
            kContentSecurityPolicyHeaderSourceOriginPolicy)) {
      for (const auto& policy : origin_policy->content_security_policies) {
        csp->DidReceiveHeader(policy, kContentSecurityPolicyHeaderTypeEnforce,
                              kContentSecurityPolicyHeaderSourceOriginPolicy);
      }

      for (const auto& policy :
           origin_policy->content_security_policies_report_only) {
        csp->DidReceiveHeader(policy, kContentSecurityPolicyHeaderTypeReport,
                              kContentSecurityPolicyHeaderSourceOriginPolicy);
      }
    }
  }
  if (!base::FeatureList::IsEnabled(
          network::features::kOutOfBlinkFrameAncestors)) {
    if (!csp->AllowAncestors(frame_, response.CurrentRequestUrl()))
      return nullptr;
  }

  if (!frame_->GetSettings()->BypassCSP() &&
      !GetFrameLoader().RequiredCSP().IsEmpty()) {
    const SecurityOrigin* parent_security_origin =
        frame_->Tree().Parent()->GetSecurityContext()->GetSecurityOrigin();
    if (ContentSecurityPolicy::ShouldEnforceEmbeddersPolicy(
            response, parent_security_origin)) {
      csp->AddPolicyFromHeaderValue(GetFrameLoader().RequiredCSP(),
                                    kContentSecurityPolicyHeaderTypeEnforce,
                                    kContentSecurityPolicyHeaderSourceHTTP);
    } else {
      auto* required_csp = MakeGarbageCollected<ContentSecurityPolicy>();
      required_csp->AddPolicyFromHeaderValue(
          GetFrameLoader().RequiredCSP(),
          kContentSecurityPolicyHeaderTypeEnforce,
          kContentSecurityPolicyHeaderSourceHTTP);
      if (!required_csp->Subsumes(*csp)) {
        String message = "Refused to display '" +
                         response.CurrentRequestUrl().ElidedString() +
                         "' because it has not opted-into the following policy "
                         "required by its embedder: '" +
                         GetFrameLoader().RequiredCSP() + "'.";
        ConsoleMessage* console_message = ConsoleMessage::CreateForRequest(
            mojom::ConsoleMessageSource::kSecurity,
            mojom::ConsoleMessageLevel::kError, message,
            response.CurrentRequestUrl(), this, MainResourceIdentifier());
        frame_->GetDocument()->AddConsoleMessage(console_message);
        return nullptr;
      }
    }
  }
  return csp;
}

void DocumentLoader::HandleResponse() {
  DCHECK(frame_);
  application_cache_host_->DidReceiveResponseForMainResource(response_);

  // Pre-commit state, count usage the use counter associated with "this"
  // (provisional document loader) instead of frame_'s document loader.
  if (response_.DidServiceWorkerNavigationPreload())
    CountUse(WebFeature::kServiceWorkerNavigationPreload);

  if (response_.CurrentRequestUrl().ProtocolIs("ftp") &&
      response_.MimeType() == "text/vnd.chromium.ftp-dir") {
    if (response_.CurrentRequestUrl().Query() == "raw") {
      // Interpret the FTP LIST command result as text.
      response_.SetMimeType("text/plain");
    } else {
      // FTP directory listing: Make up an HTML for the entries.
      listing_ftp_directory_ = true;
      response_.SetMimeType("text/html");
    }
  }

  if (frame_->Owner() && response_.IsHTTP() &&
      !cors::IsOkStatus(response_.HttpStatusCode()))
    frame_->Owner()->RenderFallbackContent(frame_);
}

void DocumentLoader::CommitNavigation() {
  CHECK_GE(state_, kCommitted);

  KURL overriding_url = base_url_override_for_bundled_exchanges_;
  if (loading_mhtml_archive_ && archive_)
    overriding_url = archive_->MainResource()->Url();

  // Prepare a DocumentInit before clearing the frame, because it may need to
  // inherit an aliased security context.
  Document* owner_document = nullptr;
  scoped_refptr<const SecurityOrigin> initiator_origin = requestor_origin_;

  // TODO(dcheng): This differs from the behavior of both IE and Firefox: the
  // origin is inherited from the document that loaded the URL.
  if (loading_url_as_javascript_) {
    owner_document = frame_->GetDocument();
  } else if (Document::ShouldInheritSecurityOriginFromOwner(Url())) {
    Frame* owner_frame = frame_->Tree().Parent();
    if (!owner_frame)
      owner_frame = frame_->Loader().Opener();
    if (auto* owner_local_frame = DynamicTo<LocalFrame>(owner_frame)) {
      owner_document = owner_local_frame->GetDocument();
      initiator_origin = owner_document->GetSecurityOrigin();
    }
  }
  DCHECK(frame_->GetPage());

  InstallNewDocument(Url(), initiator_origin, owner_document, MimeType(),
                     overriding_url);
}

void DocumentLoader::CommitData(const char* bytes, size_t length) {
  TRACE_EVENT1("loading", "DocumentLoader::CommitData", "length", length);

  // This can happen if document.close() is called by an event handler while
  // there's still pending incoming data.
  // TODO(dgozman): we should stop body loader when stopping the parser to
  // avoid unnecessary work. This may happen, for example, when we abort current
  // committed document which is still loading when initiating a new navigation.
  if (!frame_ || !frame_->GetDocument()->Parsing())
    return;

  base::AutoReset<bool> reentrancy_protector(&in_commit_data_, true);
  if (length)
    data_received_ = true;
  parser_->AppendBytes(bytes, length);
}

mojom::CommitResult DocumentLoader::CommitSameDocumentNavigation(
    const KURL& url,
    WebFrameLoadType frame_load_type,
    HistoryItem* history_item,
    ClientRedirectPolicy client_redirect_policy,
    Document* origin_document,
    bool has_event,
    std::unique_ptr<WebDocumentLoader::ExtraData> extra_data) {
  DCHECK(!IsReloadLoadType(frame_load_type));
  DCHECK(frame_->GetDocument());

  if (Page* page = frame_->GetPage())
    page->HistoryNavigationVirtualTimePauser().UnpauseVirtualTime();

  if (!frame_->IsNavigationAllowed())
    return mojom::CommitResult::Aborted;

  if (!IsBackForwardLoadType(frame_load_type)) {
    // In the case of non-history navigations, check that this is a
    // same-document navigation. If not, the navigation should restart as a
    // cross-document navigation.
    if (!url.HasFragmentIdentifier() ||
        !EqualIgnoringFragmentIdentifier(frame_->GetDocument()->Url(), url) ||
        frame_->GetDocument()->IsFrameSet()) {
      return mojom::CommitResult::RestartCrossDocument;
    }
  }

  // If the requesting document is cross-origin, perform the navigation
  // asynchronously to minimize the navigator's ability to execute timing
  // attacks.
  if (origin_document && !origin_document->GetSecurityOrigin()->CanAccess(
                             frame_->GetDocument()->GetSecurityOrigin())) {
    frame_->GetTaskRunner(TaskType::kInternalLoading)
        ->PostTask(
            FROM_HERE,
            WTF::Bind(&DocumentLoader::CommitSameDocumentNavigationInternal,
                      WrapWeakPersistent(this), url, frame_load_type,
                      WrapPersistent(history_item), client_redirect_policy,
                      WrapPersistent(origin_document), has_event,
                      std::move(extra_data)));
  } else {
    CommitSameDocumentNavigationInternal(
        url, frame_load_type, history_item, client_redirect_policy,
        origin_document, has_event, std::move(extra_data));
  }
  return mojom::CommitResult::Ok;
}

void DocumentLoader::CommitSameDocumentNavigationInternal(
    const KURL& url,
    WebFrameLoadType frame_load_type,
    HistoryItem* history_item,
    ClientRedirectPolicy client_redirect,
    Document* initiating_document,
    bool has_event,
    std::unique_ptr<WebDocumentLoader::ExtraData> extra_data) {
  // If this function was scheduled to run asynchronously, this DocumentLoader
  // might have been detached before the task ran.
  if (!frame_)
    return;

  if (!IsBackForwardLoadType(frame_load_type)) {
    SetNavigationType(has_event ? kWebNavigationTypeLinkClicked
                                : kWebNavigationTypeOther);
    if (history_item_ && url == history_item_->Url())
      frame_load_type = WebFrameLoadType::kReplaceCurrentItem;
  }

  // If we have a provisional request for a different document, a fragment
  // scroll should cancel it.
  // Note: see fragment-change-does-not-cancel-pending-navigation, where
  // this does not actually happen.
  GetFrameLoader().DetachProvisionalDocumentLoader();
  GetFrameLoader().DidFinishNavigation(
      FrameLoader::NavigationFinishState::kSuccess);

  // GetFrameLoader().DidFinishNavigation can lead to DetachFromFrame so need
  // to check again if frame_ is null.
  if (!frame_ || !frame_->GetPage())
    return;
  GetFrameLoader().SaveScrollState();

  KURL old_url = frame_->GetDocument()->Url();
  bool hash_change = EqualIgnoringFragmentIdentifier(url, old_url) &&
                     url.FragmentIdentifier() != old_url.FragmentIdentifier();
  if (hash_change) {
    // If we were in the autoscroll/middleClickAutoscroll mode we want to stop
    // it before following the link to the anchor
    frame_->GetEventHandler().StopAutoscroll();
    frame_->DomWindow()->EnqueueHashchangeEvent(old_url, url);
  }
  is_client_redirect_ =
      client_redirect == ClientRedirectPolicy::kClientRedirect;
  if (history_item)
    history_item_ = history_item;
  if (extra_data)
    GetLocalFrameClient().UpdateDocumentLoader(this, std::move(extra_data));
  UpdateForSameDocumentNavigation(url, kSameDocumentNavigationDefault, nullptr,
                                  kScrollRestorationAuto, frame_load_type,
                                  initiating_document);

  initial_scroll_state_.was_scrolled_by_user = false;

  frame_->GetDocument()->CheckCompleted();
  GetFrameLoader().DidFinishSameDocumentNavigation(url, frame_load_type,
                                                   history_item);
}

void DocumentLoader::ProcessDataBuffer(const char* bytes, size_t length) {
  DCHECK_GE(state_, kCommitted);
  if (parser_blocked_count_ || in_commit_data_) {
    // 1) If parser is blocked, we buffer data and process it upon resume.
    // 2) If this function is reentered, we defer processing of the additional
    //    data to the top-level invocation. Reentrant calls can occur because
    //    of web platform (mis-)features that require running a nested run loop:
    //    - alert(), confirm(), prompt()
    //    - Detach of plugin elements.
    //    - Synchronous XMLHTTPRequest
    if (bytes)
      data_buffer_->Append(bytes, length);
    return;
  }

  if (bytes)
    CommitData(bytes, length);
  // Process data received in reentrant invocations. Note that the invocations
  // of CommitData() may queue more data in reentrant invocations, so iterate
  // until it's empty.
  for (const auto& span : *data_buffer_)
    CommitData(span.data(), span.size());
  // All data has been consumed, so flush the buffer.
  data_buffer_->Clear();
}

void DocumentLoader::StopLoading() {
  if (frame_ && GetFrameLoader().GetDocumentLoader() == this)
    frame_->GetDocument()->Fetcher()->StopFetching();
  body_loader_.reset();
  virtual_time_pauser_.UnpauseVirtualTime();
  if (!SentDidFinishLoad())
    LoadFailed(ResourceError::CancelledError(Url()));
}

void DocumentLoader::SetDefersLoading(bool defers) {
  defers_loading_ = defers;
  if (body_loader_) {
    body_loader_->SetDefersLoading(defers);
    if (defers_loading_)
      virtual_time_pauser_.UnpauseVirtualTime();
    else
      virtual_time_pauser_.PauseVirtualTime();
  }
}

void DocumentLoader::DetachFromFrame(bool flush_microtask_queue) {
  DCHECK(frame_);
  StopLoading();
  if (flush_microtask_queue) {
    // Flush microtask queue so that they all run on pre-navigation context.
    // TODO(dcheng): This is a temporary hack that should be removed. This is
    // only here because it's currently not possible to drop the microtasks
    // queued for a Document when the Document is navigated away; instead, the
    // entire microtask queue needs to be flushed. Unfortunately, running the
    // microtasks any later results in violating internal invariants, since
    // Blink does not expect the DocumentLoader for a not-yet-detached Document
    // to be null. It is also not possible to flush microtasks any earlier,
    // since flushing microtasks can only be done after any other JS (which can
    // queue additional microtasks) has run. Once it is possible to associate
    // microtasks with a v8::Context, remove this hack.
    Microtask::PerformCheckpoint(V8PerIsolateData::MainThreadIsolate());
  }
  ScriptForbiddenScope forbid_scripts;

  // If that load cancellation triggered another detach, leave.
  // (fast/frames/detach-frame-nested-no-crash.html is an example of this.)
  if (!frame_)
    return;

  if (application_cache_host_) {
    application_cache_host_->Detach();
    application_cache_host_.Clear();
  }
  service_worker_network_provider_ = nullptr;
  WeakIdentifierMap<DocumentLoader>::NotifyObjectDestroyed(this);
  frame_ = nullptr;
}

const KURL& DocumentLoader::UnreachableURL() const {
  return unreachable_url_;
}

const base::Optional<blink::mojom::FetchCacheMode>&
DocumentLoader::ForceFetchCacheMode() const {
  return force_fetch_cache_mode_;
}

bool DocumentLoader::WillLoadUrlAsEmpty(const KURL& url) {
  if (url.IsEmpty())
    return true;
  // Usually, we load urls with about: scheme as empty.
  // However, about:srcdoc is only used as a marker for non-existent
  // url of iframes with srcdoc attribute, which have possibly non-empty
  // content of the srcdoc attribute used as document's html.
  if (url.IsAboutSrcdocURL())
    return false;
  return SchemeRegistry::ShouldLoadURLSchemeAsEmptyDocument(url.Protocol());
}

void DocumentLoader::InitializeEmptyResponse() {
  response_ = ResourceResponse(url_);
  response_.SetMimeType("text/html");
  response_.SetTextEncodingName("utf-8");
}

void DocumentLoader::StartLoading() {
  probe::LifecycleEvent(frame_, this, "init",
                        base::TimeTicks::Now().since_origin().InSecondsF());
  StartLoadingInternal();
  params_ = nullptr;
}

void DocumentLoader::StartLoadingInternal() {
  GetTiming().MarkNavigationStart();
  DCHECK_EQ(state_, kNotStarted);
  DCHECK(params_);
  state_ = kProvisional;
  application_cache_host_ = MakeGarbageCollected<ApplicationCacheHostForFrame>(
      this, GetFrame()->Client()->GetBrowserInterfaceBroker(),
      GetFrame()->GetTaskRunner(TaskType::kNetworking),
      params_->appcache_host_id);

  if (url_.IsEmpty() &&
      !GetFrameLoader().StateMachine()->CreatingInitialEmptyDocument()) {
    url_ = BlankURL();
  }

  if (loading_url_as_empty_document_) {
    InitializeEmptyResponse();
    return;
  }

  body_loader_ = std::move(params_->body_loader);
  DCHECK(body_loader_);
  DCHECK(!GetTiming().NavigationStart().is_null());
  // The fetch has already started in the browser,
  // so we don't MarkFetchStart here.
  main_resource_identifier_ = CreateUniqueIdentifier();

  navigation_timing_info_ = ResourceTimingInfo::Create(
      fetch_initiator_type_names::kDocument, GetTiming().NavigationStart());
  navigation_timing_info_->SetInitialURL(url_);
  report_timing_info_to_parent_ = ShouldReportTimingInfoToParent();

  virtual_time_pauser_ =
      frame_->GetFrameScheduler()->CreateWebScopedVirtualTimePauser(
          url_.GetString(),
          WebScopedVirtualTimePauser::VirtualTaskDuration::kNonInstant);
  virtual_time_pauser_.PauseVirtualTime();

  if (!archive_) {
    application_cache_host_->WillStartLoadingMainResource(this, url_,
                                                          http_method_);
  }

  // Many parties are interested in resource loading, so we will notify
  // them through various DispatchXXX methods on FrameFetchContext.

  GetFrameLoader().Progress().WillStartLoading(main_resource_identifier_,
                                               ResourceLoadPriority::kVeryHigh);
  probe::WillSendNavigationRequest(probe::ToCoreProbeSink(GetFrame()),
                                   main_resource_identifier_, this, url_,
                                   http_method_, http_body_.get());

  for (size_t i = 0; i < params_->redirects.size(); ++i) {
    WebNavigationParams::RedirectInfo& redirect = params_->redirects[i];
    url_ = redirect.new_url;
    AtomicString new_http_method = redirect.new_http_method;
    if (http_method_ != new_http_method) {
      http_body_ = nullptr;
      http_method_ = new_http_method;
    }
    if (redirect.new_referrer.IsEmpty()) {
      referrer_ =
          Referrer(Referrer::NoReferrer(), redirect.new_referrer_policy);
    } else {
      referrer_ = Referrer(redirect.new_referrer, redirect.new_referrer_policy);
    }
    http_content_type_ = g_null_atom;
    // TODO(dgozman): check whether clearing origin policy is intended behavior.
    origin_policy_ = base::nullopt;
    probe::WillSendNavigationRequest(probe::ToCoreProbeSink(GetFrame()),
                                     main_resource_identifier_, this, url_,
                                     http_method_, http_body_.get());
    ResourceResponse redirect_response =
        redirect.redirect_response.ToResourceResponse();
    navigation_timing_info_->AddRedirect(redirect_response, url_);
    HandleRedirect(redirect_response.CurrentRequestUrl());
  }

  if (!frame_->IsMainFrame() && response_.GetCTPolicyCompliance() ==
                                    ResourceResponse::kCTPolicyDoesNotComply) {
    // Exclude main-frame navigations; those are tracked elsewhere.
    GetUseCounterHelper().Count(
        WebFeature::kCertificateTransparencyNonCompliantResourceInSubframe,
        GetFrame());
  }
  MixedContentChecker::CheckMixedPrivatePublic(GetFrame(),
                                               response_.RemoteIPAddress());
  ParseAndPersistClientHints(response_);
  PreloadHelper::LoadLinksFromHeader(
      response_.HttpHeaderField(http_names::kLink),
      response_.CurrentRequestUrl(), *GetFrame(), nullptr,
      PreloadHelper::kDoNotLoadResources, PreloadHelper::kLoadAll,
      base::nullopt /* viewport_description */,
      nullptr /* alternate_resource_info */,
      base::nullopt /* recursive_prefetch_token */);
  if (!frame_->IsMainFrame() && response_.HasMajorCertificateErrors()) {
    MixedContentChecker::HandleCertificateError(
        GetFrame(), response_, mojom::RequestContextType::HYPERLINK);
  }
  GetFrameLoader().Progress().IncrementProgress(main_resource_identifier_,
                                                response_);
  probe::DidReceiveResourceResponse(probe::ToCoreProbeSink(GetFrame()),
                                    main_resource_identifier_, this, response_,
                                    nullptr /* resource */);
  frame_->Console().ReportResourceResponseReceived(
      this, main_resource_identifier_, response_);

  HandleResponse();

  loading_mhtml_archive_ =
      DeprecatedEqualIgnoringCase("multipart/related", response_.MimeType()) ||
      DeprecatedEqualIgnoringCase("message/rfc822", response_.MimeType());
  if (loading_mhtml_archive_) {
    // To commit an mhtml archive synchronously we have to load the whole body
    // synchronously and parse it, and it's already loaded in a buffer usually.
    // This means we should not defer, and we'll finish loading synchronously
    // from StartLoadingBody().
    body_loader_->StartLoadingBody(this, false /* use_isolated_code_cache */);
    if (body_loader_) {
      // Finalize the load of the MHTML archive. If the load fail (ie. did not
      // finish synchronously), |body_loader_| will be null amd the load will
      // not be finalized. When StartLoadingResponse is called later, an empty
      // document will be loaded instead of the MHTML archive.
      // TODO(clamy): Simplify this code path.
      FinalizeMHTMLArchiveLoad();
    }
    return;
  }

  InitializePrefetchedSignedExchangeManager();

  if (defers_loading_)
    body_loader_->SetDefersLoading(true);
}

void DocumentLoader::StartLoadingResponse() {
  if (!frame_)
    return;

  CHECK_GE(state_, kCommitted);
  CreateParserPostCommit();

  // Finish load of MHTML archives and empty documents.
  ArchiveResource* main_resource =
      loading_mhtml_archive_ && archive_ ? archive_->MainResource() : nullptr;
  if (main_resource) {
    data_buffer_ = main_resource->Data();
    ProcessDataBuffer();
  }

  if (loading_mhtml_archive_ || loading_url_as_empty_document_) {
    // Finish the load of an empty document if the URL was meant to load as an
    // empty document or the load of the MHTML archive failed.
    FinishedLoading(base::TimeTicks::Now());
    return;
  }

  // TODO(dgozman): why do we stop loading for media documents?
  // This seems like a hack.
  if (frame_ && frame_->GetDocument()->IsMediaDocument()) {
    parser_->Finish();
    StopLoading();
    return;
  }

  // Committing can run unload handlers, which can detach this frame or
  // stop this loader.
  if (!frame_ || !body_loader_)
    return;

  if (!url_.ProtocolIsInHTTPFamily()) {
    // We only support code cache for http family, and browser insists on not
    // event asking for code cache with other schemes.
    body_loader_->StartLoadingBody(this, false /* use_isolated_code_cache */);
    return;
  }

  bool use_isolated_code_cache =
      RuntimeEnabledFeatures::CacheInlineScriptCodeEnabled() &&
      ShouldUseIsolatedCodeCache(mojom::RequestContextType::HYPERLINK,
                                 response_);

  // The |cached_metadata_handler_| is created, even when
  // |use_isolated_code_cache| is false to support the parts that don't
  // go throught the site-isolated-code-cache.
  auto cached_metadata_sender = CachedMetadataSender::Create(
      response_, blink::mojom::CodeCacheType::kJavascript, requestor_origin_);
  cached_metadata_handler_ =
      MakeGarbageCollected<SourceKeyedCachedMetadataHandler>(
          WTF::TextEncoding(), std::move(cached_metadata_sender));

  body_loader_->StartLoadingBody(this, use_isolated_code_cache);
}

void DocumentLoader::DidInstallNewDocument(Document* document) {
  document->SetReadyState(Document::kLoading);
  if (content_security_policy_)
    document->BindContentSecurityPolicy();

  if (history_item_ && IsBackForwardLoadType(load_type_))
    document->SetStateForNewControls(history_item_->GetDocumentState());

  DCHECK(document->GetFrame());
  // TODO(dgozman): modify frame's client hints directly once we commit
  // synchronously.
  document->GetFrame()->GetClientHintsPreferences().UpdateFrom(
      client_hints_preferences_);

  const AtomicString& dns_prefetch_control =
      response_.HttpHeaderField(http_names::kXDNSPrefetchControl);
  if (!dns_prefetch_control.IsEmpty())
    document->ParseDNSPrefetchControlHeader(dns_prefetch_control);

  String header_content_language =
      response_.HttpHeaderField(http_names::kContentLanguage);
  if (!header_content_language.IsEmpty()) {
    wtf_size_t comma_index = header_content_language.find(',');
    // kNotFound == -1 == don't truncate
    header_content_language.Truncate(comma_index);
    header_content_language =
        header_content_language.StripWhiteSpace(IsHTMLSpace<UChar>);
    if (!header_content_language.IsEmpty())
      document->SetContentLanguage(AtomicString(header_content_language));
  }

  String referrer_policy_header =
      response_.HttpHeaderField(http_names::kReferrerPolicy);
  if (!referrer_policy_header.IsNull()) {
    UseCounter::Count(*document, WebFeature::kReferrerPolicyHeader);
    document->ParseAndSetReferrerPolicy(referrer_policy_header);
  }

  if (response_.IsSignedExchangeInnerResponse()) {
    UseCounter::Count(*document, WebFeature::kSignedExchangeInnerResponse);
    UseCounter::Count(*document,
                      document->GetFrame()->IsMainFrame()
                          ? WebFeature::kSignedExchangeInnerResponseInMainFrame
                          : WebFeature::kSignedExchangeInnerResponseInSubFrame);
  }
}

void DocumentLoader::WillCommitNavigation() {
  if (GetFrameLoader().StateMachine()->CreatingInitialEmptyDocument())
    return;
  probe::WillCommitLoad(frame_, this);
  frame_->GetIdlenessDetector()->WillCommitLoad();
}

void DocumentLoader::DidCommitNavigation() {
  if (GetFrameLoader().StateMachine()->CreatingInitialEmptyDocument())
    return;

  if (!frame_->Loader().StateMachine()->CommittedMultipleRealLoads() &&
      load_type_ == WebFrameLoadType::kStandard) {
    frame_->Loader().StateMachine()->AdvanceTo(
        FrameLoaderStateMachine::kCommittedMultipleRealLoads);
  }

  WebHistoryCommitType commit_type = LoadTypeToCommitType(load_type_);
  frame_->GetFrameScheduler()->DidCommitProvisionalLoad(
      commit_type == kWebHistoryInertCommit,
      load_type_ == WebFrameLoadType::kReload
          ? FrameScheduler::NavigationType::kReload
          : FrameScheduler::NavigationType::kOther);
  if (response_.CacheControlContainsNoCache()) {
    GetFrame()->GetFrameScheduler()->RegisterStickyFeature(
        SchedulingPolicy::Feature::kMainResourceHasCacheControlNoCache,
        {SchedulingPolicy::RecordMetricsForBackForwardCache()});
  }
  if (response_.CacheControlContainsNoStore()) {
    GetFrame()->GetFrameScheduler()->RegisterStickyFeature(
        SchedulingPolicy::Feature::kMainResourceHasCacheControlNoStore,
        {SchedulingPolicy::RecordMetricsForBackForwardCache()});
  }

  // When a new navigation commits in the frame, subresource loading should be
  // resumed.
  frame_->ResumeSubresourceLoading();

  Document* document = frame_->GetDocument();
  InteractiveDetector* interactive_detector =
      InteractiveDetector::From(*document);
  if (interactive_detector)
    interactive_detector->SetNavigationStartTime(GetTiming().NavigationStart());

  TRACE_EVENT1("devtools.timeline", "CommitLoad", "data",
               inspector_commit_load_event::Data(frame_));

  // Needs to run before dispatching preloads, as it may evict the memory cache.
  probe::DidCommitLoad(frame_, this);

  frame_->GetPage()->DidCommitLoad(frame_);
  GetUseCounterHelper().DidCommitLoad(frame_);

  // Report legacy TLS versions after Page::DidCommitLoad, because the latter
  // clears the console.
  if (response_.IsLegacyTLSVersion()) {
    GetFrameLoader().ReportLegacyTLSVersion(
        response_.CurrentRequestUrl(), false /* is_subresource */,
        frame_->IsAdSubframe() || frame_->IsAdRoot());
  }
}

// Helper function: Merge the feature policy strings from HTTP headers and the
// origin policy (if any).
// Headers go first, which means that the per-page headers override the
// origin policy features.
void MergeFeaturesFromOriginPolicy(WTF::StringBuilder& feature_policy,
                                   const WebOriginPolicy& origin_policy) {
  for (const auto& policy : origin_policy.features) {
    if (!feature_policy.IsEmpty()) {
      feature_policy.Append(',');
    }
    feature_policy.Append(policy);
  }
}

void DocumentLoader::InstallNewDocument(
    const KURL& url,
    const scoped_refptr<const SecurityOrigin> initiator_origin,
    Document* owner_document,
    const AtomicString& mime_type,
    const KURL& overriding_url) {
  DCHECK(!frame_->GetDocument() || !frame_->GetDocument()->IsActive());
  DCHECK_EQ(frame_->Tree().ChildCount(), 0u);

  // FeaturePolicy is reset in the browser process on commit, so this needs to
  // be initialized and replicated to the browser process after commit messages
  // are sent in didCommitNavigation().
  WTF::StringBuilder feature_policy;
  feature_policy.Append(response_.HttpHeaderField(http_names::kFeaturePolicy));
  if (origin_policy_.has_value()) {
    MergeFeaturesFromOriginPolicy(feature_policy, origin_policy_.value());
  }

  DocumentInit init =
      DocumentInit::Create()
          .WithDocumentLoader(this)
          .WithURL(url)
          .WithOwnerDocument(owner_document)
          .WithInitiatorOrigin(initiator_origin)
          .WithOriginToCommit(origin_to_commit_)
          .WithIPAddressSpace(ip_address_space_)
          .WithSrcdocDocument(loading_srcdoc_)
          .WithBlockedByCSP(was_blocked_by_csp_)
          .WithGrantLoadLocalResources(grant_load_local_resources_)
          .WithFramePolicy(frame_policy_)
          .WithNewRegistrationContext()
          .WithFeaturePolicyHeader(feature_policy.ToString())
          .WithOriginTrialsHeader(
              response_.HttpHeaderField(http_names::kOriginTrial))
          .WithContentSecurityPolicy(content_security_policy_.Get());

  // A javascript: url inherits CSP from the document in which it was
  // executed.
  ContentSecurityPolicy* csp =
      loading_url_as_javascript_
          ? frame_->GetDocument()->GetContentSecurityPolicy()
          : content_security_policy_.Get();
  global_object_reuse_policy_ =
      GetFrameLoader().ShouldReuseDefaultView(init.GetDocumentOrigin(), csp)
          ? GlobalObjectReusePolicy::kUseExisting
          : GlobalObjectReusePolicy::kCreateNew;

  if (GetFrameLoader().StateMachine()->IsDisplayingInitialEmptyDocument()) {
    GetFrameLoader().StateMachine()->AdvanceTo(
        FrameLoaderStateMachine::kCommittedFirstRealLoad);
  }

  const SecurityOrigin* previous_security_origin = nullptr;
  if (frame_->GetDocument()) {
    previous_security_origin = frame_->GetDocument()->GetSecurityOrigin();
  }

  // In some rare cases, we'll re-use a LocalDOMWindow for a new Document. For
  // example, when a script calls window.open("..."), the browser gives
  // JavaScript a window synchronously but kicks off the load in the window
  // asynchronously. Web sites expect that modifications that they make to the
  // window object synchronously won't be blown away when the network load
  // commits. To make that happen, we "securely transition" the existing
  // LocalDOMWindow to the Document that results from the network load. See also
  // Document::IsSecureTransitionTo.
  if (global_object_reuse_policy_ != GlobalObjectReusePolicy::kUseExisting) {
    if (frame_->GetDocument())
      frame_->GetDocument()->RemoveAllEventListenersRecursively();
    frame_->SetDOMWindow(MakeGarbageCollected<LocalDOMWindow>(*frame_));
  }

  if (!loading_url_as_javascript_)
    WillCommitNavigation();

  Document* document =
      frame_->DomWindow()->InstallNewDocument(mime_type, init, false);

  // Clear the user activation state.
  // TODO(crbug.com/736415): Clear this bit unconditionally for all frames.
  if (frame_->IsMainFrame())
    frame_->ClearActivation();

  // The DocumentLoader was flagged as activated if it needs to notify the frame
  // that it was activated before navigation. Update the frame state based on
  // the new value.
  if (frame_->HasReceivedUserGestureBeforeNavigation() !=
      had_sticky_activation_) {
    frame_->SetDocumentHasReceivedUserGestureBeforeNavigation(
        had_sticky_activation_);
    GetLocalFrameClient().SetHasReceivedUserGestureBeforeNavigation(
        had_sticky_activation_);
  }

  bool should_clear_window_name =
      previous_security_origin && frame_->IsMainFrame() &&
      !frame_->Loader().Opener() &&
      !document->GetSecurityOrigin()->IsSameSchemeHostPort(
          previous_security_origin);
  if (should_clear_window_name) {
    // TODO(andypaicu): experimentalSetNullName will just record the fact
    // that the name would be nulled and if the name is accessed after we will
    // fire a UseCounter. If we decide to move forward with this change, we'd
    // actually clean the name here.
    // frame_->tree().setName(g_null_atom);
    frame_->Tree().ExperimentalSetNulledName();
  }

  if (!overriding_url.IsEmpty())
    document->SetBaseURLOverride(overriding_url);
  DidInstallNewDocument(document);

  // This must be called before the document is opened, otherwise HTML parser
  // will use stale values from HTMLParserOption.
  if (!loading_url_as_javascript_)
    DidCommitNavigation();

  // Determine if the load is from a document from the same origin to enable
  // deferred commits to avoid white flash on load. We only want to delay
  // commits on same origin loads to avoid confusing users. We also require
  // that this be an html document served via http.
  if (initiator_origin) {
    const scoped_refptr<const SecurityOrigin> url_origin =
        SecurityOrigin::Create(Url());
    document->SetDeferredCompositorCommitIsAllowed(
        initiator_origin->IsSameSchemeHostPort(url_origin.get()) &&
        Url().ProtocolIsInHTTPFamily() && document->IsHTMLDocument());
  } else {
    document->SetDeferredCompositorCommitIsAllowed(false);
  }
}

void DocumentLoader::CreateParserPostCommit() {
  Document* document = frame_->GetDocument();

  // DidObserveLoadingBehavior() must be called after DispatchDidCommitLoad() is
  // called for the metrics tracking logic to handle it properly.
  if (service_worker_network_provider_ &&
      service_worker_network_provider_->GetControllerServiceWorkerMode() ==
          blink::mojom::ControllerServiceWorkerMode::kControlled) {
    GetLocalFrameClient().DidObserveLoadingBehavior(
        kLoadingBehaviorServiceWorkerControlled);
  }

  // Links with media values need more information (like viewport information).
  // This happens after the first chunk is parsed in HTMLDocumentParser.
  DispatchLinkHeaderPreloads(base::nullopt /* viewport */,
                             PreloadHelper::kOnlyLoadNonMedia);

  if (!loading_url_as_javascript_ &&
      !GetFrameLoader().StateMachine()->CreatingInitialEmptyDocument()) {
    // When the embedder gets notified (above) that the new navigation has
    // committed, the embedder will drop the old Content Security Policy and
    // therefore now is a good time to report to the embedder the Content
    // Security Policies that have accumulated so far for the new navigation.
    frame_->GetSecurityContext()
        ->GetContentSecurityPolicy()
        ->ReportAccumulatedHeaders(&GetLocalFrameClient());
  }

  // Initializing origin trials might force window proxy initialization,
  // which later triggers CHECK when swapping in via WebFrame::Swap().
  // We can safely omit installing original trials on initial empty document
  // and wait for the real load.
  if (GetFrameLoader().StateMachine()->CommittedFirstRealDocumentLoad()) {
    if (document->GetSettings()
            ->GetForceTouchEventFeatureDetectionForInspector()) {
      document->GetOriginTrialContext()->AddFeature(
          OriginTrialFeature::kTouchEventFeatureDetection);
    }

#if defined(OS_CHROMEOS)
    // Enable Auto Picture-in-Picture feature for the built-in Chrome OS Video
    // Player app.
    const url::Origin origin = document->GetSecurityOrigin()->ToUrlOrigin();
    if (origin.scheme() == "chrome-extension" &&
        origin.DomainIs("jcgeabjmjgoblfofpppfkcoakmfobdko") &&
        origin.port() == 0) {
      document->GetOriginTrialContext()->AddFeature(
          OriginTrialFeature::kAutoPictureInPicture);
    }
#endif

    OriginTrialContext::ActivateNavigationFeaturesFromInitiator(
        document, &initiator_origin_trial_features_);
  }

  bool opted_out_mixed_autoupgrade = EqualIgnoringASCIICase(
      response_.HttpHeaderField("mixed-content"), "noupgrade");

  if (opted_out_mixed_autoupgrade) {
    document->SetMixedAutoupgradeOptOut(true);
  }
  UMA_HISTOGRAM_BOOLEAN("MixedAutoupgrade.Navigation.OptedOut",
                        opted_out_mixed_autoupgrade);

  ParserSynchronizationPolicy parsing_policy = kAllowAsynchronousParsing;
  if (loading_url_as_javascript_ ||
      !Document::ThreadedParsingEnabledForTesting()) {
    parsing_policy = kForceSynchronousParsing;
  }
  parser_ = document->OpenForNavigation(parsing_policy, MimeType(),
                                        response_.TextEncodingName());

  // If this is a scriptable parser and there is a resource, register the
  // resource's cache handler with the parser.
  ScriptableDocumentParser* scriptable_parser =
      parser_->AsScriptableDocumentParser();
  if (scriptable_parser && cached_metadata_handler_)
    scriptable_parser->SetInlineScriptCacheHandler(cached_metadata_handler_);

  // FeaturePolicy is reset in the browser process on commit, so this needs to
  // be initialized and replicated to the browser process after commit messages
  // are sent in didCommitNavigation().
  document->ApplyPendingFeaturePolicyHeaders();

  WTF::String report_only_feature_policy(
      response_.HttpHeaderField(http_names::kFeaturePolicyReportOnly));
  // TODO(iclelland): Add Feature-Policy-Report-Only to Origin Policy.
  document->ApplyReportOnlyFeaturePolicyFromHeader(report_only_feature_policy);

  GetFrameLoader().DispatchDidClearDocumentOfWindowObject();

  parser_->SetDocumentWasLoadedAsPartOfNavigation();
  if (was_discarded_)
    document->SetWasDiscarded(true);
  document->MaybeHandleHttpRefresh(
      response_.HttpHeaderField(http_names::kRefresh),
      Document::kHttpRefreshFromHeader);
  ReportPreviewsIntervention();
}

const AtomicString& DocumentLoader::MimeType() const {
  // In the case of mhtml archive, |response_| has an archive mime type,
  // while the document has a different mime type.
  if (archive_ && loading_mhtml_archive_)
    return archive_->MainResource()->MimeType();
  return response_.MimeType();
}

void DocumentLoader::BlockParser() {
  parser_blocked_count_++;
}

void DocumentLoader::ResumeParser() {
  parser_blocked_count_--;
  DCHECK_GE(parser_blocked_count_, 0);

  if (parser_blocked_count_ != 0)
    return;

  ProcessDataBuffer();

  if (finish_loading_when_parser_resumed_) {
    finish_loading_when_parser_resumed_ = false;
    parser_->Finish();
    parser_.Clear();
  }
}

void DocumentLoader::CountUse(mojom::WebFeature feature) {
  return use_counter_.Count(feature, GetFrame());
}

void DocumentLoader::CountDeprecation(mojom::WebFeature feature) {
  return Deprecation::CountDeprecation(this, feature);
}

void DocumentLoader::ReportPreviewsIntervention() const {
  // Only send reports for main frames.
  if (!frame_->IsMainFrame())
    return;

  // Verify that certain types are not on main frame requests.
  DCHECK_NE(WebURLRequest::kClientLoFiAutoReload, previews_state_);
  DCHECK_NE(WebURLRequest::kLazyImageLoadDeferred, previews_state_);
  DCHECK_NE(WebURLRequest::kSubresourceRedirectOn, previews_state_);

  static_assert(WebURLRequest::kPreviewsStateLast ==
                    WebURLRequest::kSubresourceRedirectOn,
                "If a new Preview type is added, verify that the Intervention "
                "Report should be sent (or not sent) for that type.");

  // If the preview type is not unspecified, off, or no transform, it is a
  // preview that needs to be reported.
  if (previews_state_ == WebURLRequest::kPreviewsUnspecified ||
      previews_state_ & WebURLRequest::kPreviewsOff ||
      previews_state_ & WebURLRequest::kPreviewsNoTransform) {
    return;
  }

  Intervention::GenerateReport(
      frame_, "LitePageServed",
      "Modified page load behavior on the page because the page was expected "
      "to take a long amount of time to load. "
      "https://www.chromestatus.com/feature/5148050062311424");
}

void DocumentLoader::ParseAndPersistClientHints(
    const ResourceResponse& response) {
  const KURL& url = response.CurrentRequestUrl();

  // The accept-ch-lifetime header is honored only on the navigation responses
  // from a top level frame or with an origin matching the origin of the top
  // level frame.
  if (!frame_->IsMainFrame()) {
    bool is_first_party_origin =
        frame_->Tree()
            .Top()
            .GetSecurityContext()
            ->GetSecurityOrigin()
            ->IsSameSchemeHostPort(SecurityOrigin::Create(url).get());
    if (!is_first_party_origin)
      return;
  }

  FrameClientHintsPreferencesContext hints_context(GetFrame());
  client_hints_preferences_.UpdateFromAcceptClientHintsLifetimeHeader(
      response.HttpHeaderField(http_names::kAcceptCHLifetime), url,
      &hints_context);
  client_hints_preferences_.UpdateFromAcceptClientHintsHeader(
      response.HttpHeaderField(http_names::kAcceptCH), url, &hints_context);

  // Notify content settings client of persistent client hints.
  if (client_hints_preferences_.GetPersistDuration().InSeconds() <= 0)
    return;

  auto* settings_client = frame_->GetContentSettingsClient();
  if (!settings_client)
    return;

  // Do not persist client hint preferences if the JavaScript is disabled.
  bool allow_script = frame_->GetSettings()->GetScriptEnabled();
  if (!settings_client->AllowScriptFromSource(allow_script, url))
    return;
  settings_client->PersistClientHints(
      client_hints_preferences_.GetWebEnabledClientHints(),
      client_hints_preferences_.GetPersistDuration(), url);
}

void DocumentLoader::InitializePrefetchedSignedExchangeManager() {
  if (params_->prefetched_signed_exchanges.empty())
    return;
  // |prefetched_signed_exchanges| is set only when the page is loaded from a
  // signed exchange.
  DCHECK(GetResponse().IsSignedExchangeInnerResponse());
  // When the page is loaded from a signed exchange, |last_redirect| must be the
  // synthesized redirect for the signed exchange.
  DCHECK(params_->redirects.size());
  const WebNavigationParams::RedirectInfo& last_redirect =
      params_->redirects[params_->redirects.size() - 1];
  prefetched_signed_exchange_manager_ =
      PrefetchedSignedExchangeManager::MaybeCreate(
          GetFrame(),
          last_redirect.redirect_response.HttpHeaderField(http_names::kLink),
          GetResponse().HttpHeaderField(http_names::kLink),
          std::move(params_->prefetched_signed_exchanges));
}

PrefetchedSignedExchangeManager*
DocumentLoader::GetPrefetchedSignedExchangeManager() const {
  return prefetched_signed_exchange_manager_;
}

DEFINE_WEAK_IDENTIFIER_MAP(DocumentLoader)

}  // namespace blink
