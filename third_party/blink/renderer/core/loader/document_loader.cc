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
#include "build/chromeos_buildflags.h"
#include "services/network/public/cpp/web_sandbox_flags.h"
#include "services/network/public/mojom/web_sandbox_flags.mojom-blink.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/commit_result/commit_result.mojom-blink.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/platform/modules/service_worker/web_service_worker_network_provider.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/document_init.h"
#include "third_party/blink/renderer/core/dom/document_parser.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/scriptable_document_parser.h"
#include "third_party/blink/renderer/core/dom/weak_identifier_map.h"
#include "third_party/blink/renderer/core/execution_context/security_context_init.h"
#include "third_party/blink/renderer/core/execution_context/window_agent.h"
#include "third_party/blink/renderer/core/execution_context/window_agent_factory.h"
#include "third_party/blink/renderer/core/feature_policy/document_policy_parser.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/deprecation.h"
#include "third_party/blink/renderer/core/frame/frame_console.h"
#include "third_party/blink/renderer/core/frame/intervention.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/html_document.h"
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
#include "third_party/blink/renderer/core/page/scrolling/text_fragment_anchor.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/core/timing/window_performance.h"
#include "third_party/blink/renderer/core/xml/document_xslt.h"
#include "third_party/blink/renderer/platform/bindings/microtask.h"
#include "third_party/blink/renderer/platform/bindings/script_forbidden_scope.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_isolate_data.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
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
#include "third_party/blink/renderer/platform/network/encoded_form_data.h"
#include "third_party/blink/renderer/platform/network/http_names.h"
#include "third_party/blink/renderer/platform/network/http_parsers.h"
#include "third_party/blink/renderer/platform/network/network_utils.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_scheduler.h"
#include "third_party/blink/renderer/platform/web_test_support.h"
#include "third_party/blink/renderer/platform/weborigin/scheme_registry.h"
#include "third_party/blink/renderer/platform/weborigin/security_policy.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

namespace {
Vector<OriginTrialFeature> CopyInitiatorOriginTrials(
    const WebVector<int>& initiator_origin_trial_features) {
  Vector<OriginTrialFeature> result;
  for (auto feature : initiator_origin_trial_features) {
    // Convert from int to OriginTrialFeature. These values are passed between
    // blink navigations. OriginTrialFeature isn't visible outside of blink (and
    // doesn't need to be) so the values are transferred outside of blink as
    // ints and casted to OriginTrialFeature once being processed in blink.
    result.push_back(static_cast<OriginTrialFeature>(feature));
  }
  return result;
}

Vector<String> CopyForceEnabledOriginTrials(
    const WebVector<WebString>& force_enabled_origin_trials) {
  Vector<String> result;
  result.ReserveInitialCapacity(
      SafeCast<wtf_size_t>(force_enabled_origin_trials.size()));
  for (const auto& trial : force_enabled_origin_trials)
    result.push_back(trial);
  return result;
}

bool IsPagePopupRunningInWebTest(LocalFrame* frame) {
  return frame && frame->GetPage()->GetChromeClient().IsPopup() &&
         WebTestSupport::IsRunningWebTest();
}

}  // namespace

DocumentLoader::DocumentLoader(
    LocalFrame* frame,
    WebNavigationType navigation_type,
    ContentSecurityPolicy* content_security_policy,
    std::unique_ptr<WebNavigationParams> navigation_params)
    : params_(std::move(navigation_params)),
      url_(params_->url),
      http_method_(static_cast<String>(params_->http_method)),
      referrer_(Referrer(params_->referrer.IsEmpty()
                             ? Referrer::NoReferrer()
                             : static_cast<String>(params_->referrer),
                         params_->referrer_policy)),
      http_body_(params_->http_body),
      http_content_type_(static_cast<String>(params_->http_content_type)),
      origin_policy_(params_->origin_policy),
      requestor_origin_(params_->requestor_origin),
      unreachable_url_(params_->unreachable_url),
      ip_address_space_(params_->ip_address_space),
      grant_load_local_resources_(params_->grant_load_local_resources),
      force_fetch_cache_mode_(params_->force_fetch_cache_mode),
      frame_policy_(params_->frame_policy.value_or(FramePolicy())),
      frame_(frame),
      // For back/forward navigations, the browser passed a history item to use
      // at commit time in |params_|. Set it as the current history item of this
      // DocumentLoader. For other navigations, |history_item_| will be created
      // when the FrameLoader calls SetHistoryItemStateForCommit.
      history_item_(IsBackForwardLoadType(params_->frame_load_type)
                        ? params_->history_item
                        : nullptr),
      original_url_(params_->url),
      original_referrer_(referrer_),
      response_(params_->response.ToResourceResponse()),
      load_type_(params_->frame_load_type),
      is_client_redirect_(params_->is_client_redirect),
      // TODO(japhet): This is needed because the browser process DCHECKs if the
      // first entry we commit in a new frame has replacement set. It's unclear
      // whether the DCHECK is right, investigate removing this special case.
      // TODO(dgozman): we should get rid of this boolean field, and make client
      // responsible for it's own view of "replaces current item", based on the
      // frame load type.
      replaces_current_history_item_(
          load_type_ == WebFrameLoadType::kReplaceCurrentItem &&
          (!frame_->Loader().Opener() || !url_.IsEmpty())),
      data_received_(false),
      // The input CSP is null when the CSP check done in the FrameLoader failed
      content_security_policy_(
          content_security_policy
              ? content_security_policy
              : MakeGarbageCollected<ContentSecurityPolicy>()),
      was_blocked_by_csp_(!content_security_policy),
      // Loading the document was blocked by the CSP check. Pretend that this
      // was an empty document instead and don't reuse the original URL. More
      // details in: https://crbug.com/622385.
      // TODO(https://crbug.com/555418) Remove this once XFO moves to the
      // browser.

      // Update |origin_to_commit_| to contain an opaque origin with precursor
      // information that is consistent with the final request URL.
      // Note: this doesn't use |url_| for the origin calculation, because
      // redirects are not yet accounted for (this happens later in
      // StartLoadingInternal).
      origin_to_commit_(
          was_blocked_by_csp_
              ? blink::SecurityOrigin::Create(response_.CurrentRequestUrl())
                    ->DeriveNewOpaqueOrigin()
              : params_->origin_to_commit.IsNull()
                    ? nullptr
                    : params_->origin_to_commit.Get()->IsolatedCopy()),
      navigation_type_(navigation_type),
      document_load_timing_(*this),
      service_worker_network_provider_(
          std::move(params_->service_worker_network_provider)),
      was_blocked_by_document_policy_(false),
      state_(kNotStarted),
      in_commit_data_(false),
      data_buffer_(SharedBuffer::Create()),
      devtools_navigation_token_(params_->devtools_navigation_token),
      had_sticky_activation_(params_->is_user_activated),
      had_transient_activation_(
          LocalFrame::HasTransientUserActivation(frame_) ||
          params_->had_transient_activation),
      is_browser_initiated_(params_->is_browser_initiated),
      was_discarded_(params_->was_discarded),
      loading_srcdoc_(url_.IsAboutSrcdocURL()),
      loading_url_as_empty_document_(!params_->is_static_data &&
                                     WillLoadUrlAsEmpty(url_)),
      web_bundle_physical_url_(params_->web_bundle_physical_url),
      web_bundle_claimed_url_(params_->web_bundle_claimed_url),
      ukm_source_id_(params_->document_ukm_source_id),
      clock_(params_->tick_clock ? params_->tick_clock
                                 : base::DefaultTickClock::GetInstance()),
      initiator_origin_trial_features_(
          CopyInitiatorOriginTrials(params_->initiator_origin_trial_features)),
      force_enabled_origin_trials_(
          CopyForceEnabledOriginTrials(params_->force_enabled_origin_trials)),
      origin_isolated_(params_->origin_isolated),
      is_cross_browsing_context_group_navigation_(
          params_->is_cross_browsing_context_group_navigation) {
  DCHECK(frame_);

  // See `archive_` attribute documentation.
  if (!frame_->IsMainFrame()) {
    if (auto* parent = DynamicTo<LocalFrame>(frame_->Tree().Parent()))
      archive_ = parent->Loader().GetDocumentLoader()->archive_;
  }

  // Determine if this document should have a text fragment permission token.
  // We can either generate a new one from this navigation, if it's user
  // activated, or receive one propagated from the prior navigation that didn't
  // consume its token.
  has_text_fragment_token_ = TextFragmentAnchor::GenerateNewToken(*this) ||
                             params_->has_text_fragment_token;

  if (frame_->IsMainFrame()) {
    previews_state_ = params_->previews_state;
  } else {
    // Subframes inherit previews state from the main frame.
    if (auto* parent = DynamicTo<LocalFrame>(frame_->Tree().Parent()))
      previews_state_ = parent->Loader().GetDocumentLoader()->previews_state_;
  }

  document_policy_ = CreateDocumentPolicy();

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

  // The document URL needs to be added to the head of the list as that is
  // where the redirects originated.
  if (is_client_redirect_)
    redirect_chain_.push_back(frame_->GetDocument()->Url());

  if (was_blocked_by_csp_ || was_blocked_by_document_policy_)
    ReplaceWithEmptyDocument();

  if (commit_reason_ != CommitReason::kInitialization)
    redirect_chain_.push_back(url_);

  if (IsBackForwardLoadType(params_->frame_load_type))
    DCHECK(history_item_);
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

void DocumentLoader::Trace(Visitor* visitor) const {
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
    const ViewportDescription* viewport,
    PreloadHelper::MediaPreloadPolicy media_policy) {
  DCHECK_GE(state_, kCommitted);
  PreloadHelper::LoadLinksFromHeader(
      GetResponse().HttpHeaderField(http_names::kLink),
      GetResponse().CurrentRequestUrl(), *frame_, frame_->GetDocument(),
      PreloadHelper::kOnlyLoadResources, media_policy, viewport,
      nullptr /* alternate_resource_info */,
      nullptr /* recursive_prefetch_token */);
}

void DocumentLoader::DidChangePerformanceTiming() {
  if (frame_ && state_ >= kCommitted) {
    GetLocalFrameClient().DidChangePerformanceTiming();
  }
}

void DocumentLoader::DidObserveInputDelay(base::TimeDelta input_delay) {
  if (frame_ && state_ >= kCommitted) {
    GetLocalFrameClient().DidObserveInputDelay(input_delay);
  }
}
void DocumentLoader::DidObserveLoadingBehavior(LoadingBehaviorFlag behavior) {
  if (frame_) {
    DCHECK_GE(state_, kCommitted);
    GetLocalFrameClient().DidObserveLoadingBehavior(behavior);
  }
}

// static
WebHistoryCommitType LoadTypeToCommitType(WebFrameLoadType type) {
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
    mojom::blink::ScrollRestorationType scroll_restoration_type,
    WebFrameLoadType type,
    bool is_content_initiated) {
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

  // We want to allow same-document text fragment navigations if they're coming
  // from the browser.
  has_text_fragment_token_ =
      TextFragmentAnchor::GenerateNewTokenForSameDocument(
          new_url.FragmentIdentifier(), type, is_content_initiated,
          same_document_navigation_source);

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
      history_item_.Get(), commit_type, is_content_initiated);
  probe::DidNavigateWithinDocument(frame_);
  if (!was_loading) {
    GetLocalFrameClient().DidStopLoading();
    frame_->UpdateFaviconURL();
  }
}

const KURL& DocumentLoader::UrlForHistory() const {
  return UnreachableURL().IsEmpty() ? Url() : UnreachableURL();
}

void DocumentLoader::SetHistoryItemStateForCommit(
    HistoryItem* old_item,
    WebFrameLoadType load_type,
    HistoryNavigationType navigation_type) {
  if (!history_item_ || !IsBackForwardLoadType(load_type))
    history_item_ = MakeGarbageCollected<HistoryItem>();

  history_item_->SetURL(UrlForHistory());
  history_item_->SetReferrer(SecurityPolicy::GenerateReferrer(
      referrer_.referrer_policy, history_item_->Url(), referrer_.referrer));
  if (EqualIgnoringASCIICase(http_method_, "POST")) {
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

mojo::PendingReceiver<mojom::blink::WorkerTimingContainer>
DocumentLoader::TakePendingWorkerTimingReceiver(int request_id) {
  if (!GetServiceWorkerNetworkProvider())
    return mojo::NullReceiver();
  return GetServiceWorkerNetworkProvider()->TakePendingWorkerTimingReceiver(
      request_id);
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

  if (listing_ftp_directory_ || loading_main_document_from_mhtml_archive_) {
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
      // The response is being copied here to pass the Encoded and Decoded
      // sizes.
      // TODO(yoav): copy the sizes info directly.
      navigation_timing_info_->SetFinalResponse(response_);
      navigation_timing_info_->AddFinalTransferSize(
          total_encoded_data_length == -1 ? 0 : total_encoded_data_length);
      if (report_timing_info_to_parent_) {
        navigation_timing_info_->SetLoadResponseEnd(completion_time);
        if (state_ >= kCommitted) {
          // Note that we currently lose timing info for empty documents,
          // which will be fixed with synchronous commit.
          // Main resource timing information is reported through the owner
          // to be passed to the parent frame, if appropriate.

          // TODO(https://crbug.com/900700): Set a Mojo pending receiver for
          // WorkerTimingContainer in |navigation_timing_info|.
          frame_->Owner()->AddResourceTiming(*navigation_timing_info_);
        }
        frame_->SetShouldSendResourceTimingInfoToParent(false);
      }
    }
    FinishedLoading(completion_time);
    return;
  }

  ResourceError resource_error(*error);
  if (network_utils::IsCertificateTransparencyRequiredError(
          resource_error.ErrorCode())) {
    CountUse(WebFeature::kCertificateTransparencyRequiredErrorOnResourceLoad);
  }
  GetFrameLoader().Progress().CompleteProgress(main_resource_identifier_);
  probe::DidFailLoading(probe::ToCoreProbeSink(GetFrame()),
                        main_resource_identifier_, this, resource_error,
                        frame_->GetDevToolsFrameToken());
  GetFrame()->Console().DidFailLoading(this, main_resource_identifier_,
                                       resource_error);
  LoadFailed(resource_error);
}

void DocumentLoader::LoadFailed(const ResourceError& error) {
  TRACE_EVENT1("navigation,rail", "DocumentLoader::LoadFailed", "error",
               error.ErrorCode());
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

  DCHECK(commit_reason_ == CommitReason::kInitialization ||
         !frame_->GetPage()->Paused() ||
         MainThreadDebugger::Instance()->IsPaused());

  if (listing_ftp_directory_) {
    data_buffer_ = GenerateFtpDirectoryListingHtml(
        response_.CurrentRequestUrl(), data_buffer_.get());
    ProcessDataBuffer();
  }

  if (loading_main_document_from_mhtml_archive_ && state_ < kCommitted) {
    // The browser process should block any navigation to an MHTML archive
    // inside iframes. See NavigationRequest::OnResponseStarted().
    CHECK(frame_->IsMainFrame());

    archive_ = MHTMLArchive::Create(url_, std::move(data_buffer_));
  }

  // We should not call FinishedLoading before committing navigation,
  // except for the mhtml case. When loading an MHTML archive, the whole archive
  // has to be validated before committing the navigation. The validation
  // process loads the entire body of the archive, which will move the state to
  // FinishedLoading.
  if (!loading_main_document_from_mhtml_archive_)
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

void DocumentLoader::HandleRedirect(const KURL& current_request_url) {
  // Browser process should have already checked that redirecting url is
  // allowed to display content from the target origin.
  // When the referrer page is in an unsigned Web Bundle file in local
  // (eg: file:///tmp/a.wbn), Chrome internally redirects the navigation to the
  // page (eg: https://example.com/page.html) inside the Web Bundle file
  // to the file's URL (file:///tmp/a.wbn?https://example.com/page.html). In
  // this case, CanDisplay() returns false, and web_bundle_claimed_url must not
  // be null.
  CHECK(SecurityOrigin::Create(current_request_url)->CanDisplay(url_) ||
        !params_->web_bundle_claimed_url.IsNull());

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

void DocumentLoader::ConsoleError(const String& message) {
  auto* console_message = MakeGarbageCollected<ConsoleMessage>(
      mojom::ConsoleMessageSource::kSecurity,
      mojom::ConsoleMessageLevel::kError, message,
      response_.CurrentRequestUrl(), this, MainResourceIdentifier());
  frame_->DomWindow()->AddConsoleMessage(console_message);
}

void DocumentLoader::ReplaceWithEmptyDocument() {
  DCHECK(params_);
  KURL blocked_url = SecurityOrigin::UrlWithUniqueOpaqueOrigin();
  original_url_ = blocked_url;
  url_ = blocked_url;
  params_->url = blocked_url;
  WebNavigationParams::FillStaticResponse(params_.get(), "text/html", "UTF-8",
                                          "");
}

DocumentPolicy::ParsedDocumentPolicy DocumentLoader::CreateDocumentPolicy() {
  // For URLs referring to local content to parent frame, they have no way to
  // specify the document policy they use. If the parent frame requires a
  // document policy on them, use the required policy as effective policy.
  if (url_.IsEmpty() || url_.ProtocolIsAbout() || url_.ProtocolIsData() ||
      url_.ProtocolIs("blob") || url_.ProtocolIs("filesystem"))
    return {frame_policy_.required_document_policy, {} /* endpoint_map */};

  PolicyParserMessageBuffer header_logger("Document-Policy HTTP header: ");
  PolicyParserMessageBuffer require_header_logger(
      "Require-Document-Policy HTTP header: ");

  // Filtering out features that are disabled by origin trial is done
  // in SecurityContextInit when origin trial context is available.
  auto parsed_policy =
      DocumentPolicyParser::Parse(
          response_.HttpHeaderField(http_names::kDocumentPolicy), header_logger)
          .value_or(DocumentPolicy::ParsedDocumentPolicy{});

  // |parsed_policy| can have policies that are disabled by origin trial,
  // but |frame_policy_.required_document_policy| cannot.
  // It is safe to call |IsPolicyCompatible| as long as required policy is
  // checked against origin trial.
  if (!DocumentPolicy::IsPolicyCompatible(
          frame_policy_.required_document_policy,
          parsed_policy.feature_state)) {
    was_blocked_by_document_policy_ = true;
    // When header policy is less strict than required policy, use required
    // policy to initialize document policy for the document.
    parsed_policy = {frame_policy_.required_document_policy,
                     {} /* endpoint_map */};
  }

  // Initialize required document policy for subtree.
  //
  // If the document is blocked by document policy, there won't be content
  // in the sub-frametree, thus no need to initialize required_policy for
  // subtree.
  if (!was_blocked_by_document_policy_) {
    // Require-Document-Policy header only affects subtree of current document,
    // but not the current document.
    const DocumentPolicyFeatureState header_required_policy =
        DocumentPolicyParser::Parse(
            response_.HttpHeaderField(http_names::kRequireDocumentPolicy),
            require_header_logger)
            .value_or(DocumentPolicy::ParsedDocumentPolicy{})
            .feature_state;
    frame_->SetRequiredDocumentPolicy(DocumentPolicy::MergeFeatureState(
        header_required_policy, frame_policy_.required_document_policy));
  }

  document_policy_parsing_messages_.AppendVector(header_logger.GetMessages());
  document_policy_parsing_messages_.AppendVector(
      require_header_logger.GetMessages());

  return parsed_policy;
}

void DocumentLoader::HandleResponse() {
  DCHECK(frame_);
  application_cache_host_->DidReceiveResponseForMainResource(response_);

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
    LocalDOMWindow* origin_window,
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
  if (origin_window && !origin_window->GetSecurityOrigin()->CanAccess(
                           frame_->DomWindow()->GetSecurityOrigin())) {
    frame_->GetTaskRunner(TaskType::kInternalLoading)
        ->PostTask(
            FROM_HERE,
            WTF::Bind(&DocumentLoader::CommitSameDocumentNavigationInternal,
                      WrapWeakPersistent(this), url, frame_load_type,
                      WrapPersistent(history_item), client_redirect_policy,
                      !!origin_window, has_event, std::move(extra_data)));
  } else {
    CommitSameDocumentNavigationInternal(url, frame_load_type, history_item,
                                         client_redirect_policy, origin_window,
                                         has_event, std::move(extra_data));
  }
  return mojom::CommitResult::Ok;
}

void DocumentLoader::CommitSameDocumentNavigationInternal(
    const KURL& url,
    WebFrameLoadType frame_load_type,
    HistoryItem* history_item,
    ClientRedirectPolicy client_redirect,
    bool is_content_initiated,
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

  // If we have a client navigation for a different document, a fragment
  // scroll should cancel it.
  // Note: see fragment-change-does-not-cancel-pending-navigation, where
  // this does not actually happen.
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
  bool same_item_sequence_number =
      history_item_ && history_item &&
      history_item_->ItemSequenceNumber() == history_item->ItemSequenceNumber();
  if (history_item)
    history_item_ = history_item;
  if (extra_data)
    GetLocalFrameClient().UpdateDocumentLoader(this, std::move(extra_data));
  UpdateForSameDocumentNavigation(url, kSameDocumentNavigationDefault, nullptr,
                                  mojom::blink::ScrollRestorationType::kAuto,
                                  frame_load_type, is_content_initiated);

  initial_scroll_state_.was_scrolled_by_user = false;

  frame_->GetDocument()->CheckCompleted();

  // If the item sequence number didn't change, there's no need to trigger
  // popstate, restore scroll positions, or scroll to fragments for this
  // same-document navigation.  It's possible to get a same-document navigation
  // to a same ISN when a history navigation targets a frame that no longer
  // exists (https://crbug.com/705550).
  if (!same_item_sequence_number) {
    GetFrameLoader().DidFinishSameDocumentNavigation(url, frame_load_type,
                                                     history_item);
  }
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
  if (body_loader_)
    body_loader_->SetDefersLoading(defers);
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

  if (url_.IsEmpty() && commit_reason_ != CommitReason::kInitialization)
    url_ = BlankURL();

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
      fetch_initiator_type_names::kDocument, GetTiming().NavigationStart(),
      mojom::blink::RequestContextType::IFRAME,
      network::mojom::RequestDestination::kIframe);
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
      http_content_type_ = g_null_atom;
      http_method_ = new_http_method;
    }
    if (redirect.new_referrer.IsEmpty()) {
      referrer_ =
          Referrer(Referrer::NoReferrer(), redirect.new_referrer_policy);
    } else {
      referrer_ = Referrer(redirect.new_referrer, redirect.new_referrer_policy);
    }

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

  if (!frame_->IsMainFrame()) {
    // We only care about detecting embedded private subresources.
    //
    // TODO(crbug.com/1129326): Revisit this when we have a coherent story for
    // top-level navigations.
    MixedContentChecker::CheckMixedPrivatePublic(frame_, response_);
  }

  ApplyClientHintsConfig(params_->enabled_client_hints);
  PreloadHelper::LoadLinksFromHeader(
      response_.HttpHeaderField(http_names::kLink),
      response_.CurrentRequestUrl(), *GetFrame(), nullptr,
      PreloadHelper::kDoNotLoadResources, PreloadHelper::kLoadAll,
      nullptr /* viewport_description */, nullptr /* alternate_resource_info */,
      nullptr /* recursive_prefetch_token */);
  if (!frame_->IsMainFrame() && response_.HasMajorCertificateErrors()) {
    MixedContentChecker::HandleCertificateError(
        response_, mojom::blink::RequestContextType::HYPERLINK,
        MixedContentChecker::DecideCheckModeForPlugin(
            GetFrame()->GetSettings()),
        GetContentSecurityNotifier());
  }
  GetFrameLoader().Progress().IncrementProgress(main_resource_identifier_,
                                                response_);
  probe::DidReceiveResourceResponse(probe::ToCoreProbeSink(GetFrame()),
                                    main_resource_identifier_, this, response_,
                                    nullptr /* resource */);

  HandleResponse();

  loading_main_document_from_mhtml_archive_ =
      EqualIgnoringASCIICase("multipart/related", response_.MimeType()) ||
      EqualIgnoringASCIICase("message/rfc822", response_.MimeType());
  if (loading_main_document_from_mhtml_archive_) {
    // The browser process should block any navigation to an MHTML archive
    // inside iframes. See NavigationRequest::OnResponseStarted().
    CHECK(frame_->IsMainFrame());

    // To commit an mhtml archive synchronously we have to load the whole body
    // synchronously and parse it, and it's already loaded in a buffer usually.
    // This means we should not defer, and we'll finish loading synchronously
    // from StartLoadingBody().
    body_loader_->StartLoadingBody(this, false /* use_isolated_code_cache */);
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

  // Let the browser process know about all the CSP applied to the document.
  // The browser process is enforcing several directives. It needs to know about
  // 'frame-src', 'child-src', 'navigate-to', 'upgrade-insecure-request', etc.
  //
  // It is important to forward all the CSP data before loading the response
  // body, otherwise some loaded content might not be blocked.
  frame_->DomWindow()->GetContentSecurityPolicy()->ReportAccumulatedHeaders();

  CreateParserPostCommit();

  // The main document from an MHTML archive is not loaded from its HTTP
  // response, but from the main resource within the archive (in the response).
  if (loading_main_document_from_mhtml_archive_) {
    // If the `archive_` contains a main resource, load the main document from
    // the archive, else it will remain empty.
    if (ArchiveResource* resource = archive_->MainResource()) {
      DCHECK_EQ(archive_->LoadResult(),
                mojom::blink::MHTMLLoadResult::kSuccess);

      data_buffer_ = resource->Data();
      ProcessDataBuffer();
      FinishedLoading(base::TimeTicks::Now());
      return;
    }

    // Log attempts loading a malformed archive.
    DCHECK_NE(archive_->LoadResult(), mojom::blink::MHTMLLoadResult::kSuccess);
    frame_->Console().AddMessage(MakeGarbageCollected<ConsoleMessage>(
        mojom::blink::ConsoleMessageSource::kJavaScript,
        mojom::blink::ConsoleMessageLevel::kError,
        "Malformed multipart archive: " + url_.GetString()));
    FinishedLoading(base::TimeTicks::Now());
    return;
  }

  // Empty documents are empty by definition. Nothing to load.
  if (loading_url_as_empty_document_) {
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
      ShouldUseIsolatedCodeCache(mojom::blink::RequestContextType::HYPERLINK,
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
  frame_->DomWindow()->BindContentSecurityPolicy();

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

  for (const auto& message : document_policy_parsing_messages_) {
    document->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
        mojom::blink::ConsoleMessageSource::kOther, message.level,
        message.content));
  }
  document_policy_parsing_messages_.clear();
}

void DocumentLoader::WillCommitNavigation() {
  if (commit_reason_ != CommitReason::kRegular)
    return;
  probe::WillCommitLoad(frame_, this);
  frame_->GetIdlenessDetector()->WillCommitLoad();
}

void DocumentLoader::DidCommitNavigation() {
  if (commit_reason_ != CommitReason::kRegular)
    return;

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

  // Report legacy TLS versions after Page::DidCommitLoad, because the latter
  // clears the console.
  if (response_.IsLegacyTLSVersion()) {
    GetFrameLoader().ReportLegacyTLSVersion(
        response_.CurrentRequestUrl(), false /* is_subresource */,
        frame_->IsAdSubframe() || frame_->IsAdRoot());
  }
}

network::mojom::blink::WebSandboxFlags DocumentLoader::CalculateSandboxFlags() {
  auto sandbox_flags = GetFrameLoader().GetForcedSandboxFlags() |
                       content_security_policy_->GetSandboxMask() |
                       frame_policy_.sandbox_flags;
  if (archive_) {
    // The URL of a Document loaded from a MHTML archive is controlled by
    // the Content-Location header. This would allow UXSS, since
    // Content-Location can be arbitrarily controlled to control the
    // Document's URL and origin. Instead, force a Document loaded from a
    // MHTML archive to be sandboxed, providing exceptions only for creating
    // new windows.
    DCHECK(commit_reason_ == CommitReason::kRegular ||
           commit_reason_ == CommitReason::kInitialization);
    sandbox_flags |= (network::mojom::blink::WebSandboxFlags::kAll &
                      ~(network::mojom::blink::WebSandboxFlags::kPopups |
                        network::mojom::blink::WebSandboxFlags::
                            kPropagatesToAuxiliaryBrowsingContexts));
  } else if (commit_reason_ == CommitReason::kXSLT) {
    // An XSLT document inherits sandbox flags from the document that create it.
    sandbox_flags |= frame_->DomWindow()->GetSandboxFlags();
  }
  return sandbox_flags;
}

scoped_refptr<SecurityOrigin> DocumentLoader::CalculateOrigin(
    Document* owner_document,
    network::mojom::blink::WebSandboxFlags sandbox_flags) {
  scoped_refptr<SecurityOrigin> origin;
  if (origin_to_commit_) {
    // Origin to commit is specified by the browser process, it must be taken
    // and used directly. It is currently supplied only for session history
    // navigations, where the origin was already calcuated previously and
    // stored on the session history entry.
    origin = origin_to_commit_;
  } else if (IsPagePopupRunningInWebTest(frame_)) {
    // If we are a page popup in LayoutTests ensure we use the popup
    // owner's security origin so the tests can possibly access the
    // document via internals API.
    auto* owner_context = frame_->PagePopupOwner()->GetExecutionContext();
    origin = owner_context->GetSecurityOrigin()->IsolatedCopy();
  } else if (owner_document && owner_document->domWindow()) {
    origin = owner_document->domWindow()->GetMutableSecurityOrigin();
  } else {
    // Otherwise, create an origin that propagates precursor information
    // as needed. For non-opaque origins, this creates a standard tuple
    // origin, but for opaque origins, it creates an origin with the
    // initiator origin as the precursor.
    origin = SecurityOrigin::CreateWithReferenceOrigin(url_,
                                                       requestor_origin_.get());
  }

  if ((sandbox_flags & network::mojom::blink::WebSandboxFlags::kOrigin) !=
      network::mojom::blink::WebSandboxFlags::kNone) {
    auto sandbox_origin = origin->DeriveNewOpaqueOrigin();

    // If we're supposed to inherit our security origin from our
    // owner, but we're also sandboxed, the only things we inherit are
    // the origin's potential trustworthiness and the ability to
    // load local resources. The latter lets about:blank iframes in
    // file:// URL documents load images and other resources from
    // the file system.
    //
    // Note: Sandboxed about:srcdoc iframe without "allow-same-origin" aren't
    // allowed to load user's file, even if its parent can.
    if (owner_document) {
      if (origin->IsPotentiallyTrustworthy())
        sandbox_origin->SetOpaqueOriginIsPotentiallyTrustworthy(true);
      if (origin->CanLoadLocalResources() && !loading_srcdoc_)
        sandbox_origin->GrantLoadLocalResources();
    }
    origin = sandbox_origin;
  }

  if (!frame_->GetSettings()->GetWebSecurityEnabled()) {
    // Web security is turned off. We should let this document access
    // every other document. This is used primary by testing harnesses for
    // web sites.
    origin->GrantUniversalAccess();
  } else if (origin->IsLocal()) {
    if (frame_->GetSettings()->GetAllowUniversalAccessFromFileURLs()) {
      // Some clients want local URLs to have universal access, but that
      // setting is dangerous for other clients.
      origin->GrantUniversalAccess();
    } else if (!frame_->GetSettings()->GetAllowFileAccessFromFileURLs()) {
      // Some clients do not want local URLs to have access to other local
      // URLs.
      origin->BlockLocalAccessFromLocalOrigin();
    }
  }

  if (grant_load_local_resources_)
    origin->GrantLoadLocalResources();

  if (origin->IsOpaque()) {
    KURL url = url_.IsEmpty() ? BlankURL() : url_;
    if (SecurityOrigin::Create(url)->IsPotentiallyTrustworthy())
      origin->SetOpaqueOriginIsPotentiallyTrustworthy(true);
  }
  return origin;
}

bool ShouldReuseDOMWindow(LocalDOMWindow* window,
                          SecurityOrigin* security_origin) {
  // Secure transitions can only happen when navigating from the initial empty
  // document.
  return window && window->document()->IsInitialEmptyDocument() &&
         window->GetSecurityOrigin()->CanAccess(security_origin);
}

WindowAgent* GetWindowAgentForOrigin(LocalFrame* frame,
                                     SecurityOrigin* origin) {
  // TODO(keishi): Also check if AllowUniversalAccessFromFileURLs might
  // dynamically change.
  bool has_potential_universal_access_privilege =
      !frame->GetSettings()->GetWebSecurityEnabled() ||
      frame->GetSettings()->GetAllowUniversalAccessFromFileURLs();
  return frame->window_agent_factory().GetAgentForOrigin(
      has_potential_universal_access_privilege,
      V8PerIsolateData::MainThreadIsolate(), origin);
}

void DocumentLoader::InitializeWindow(Document* owner_document) {
  auto sandbox_flags = CalculateSandboxFlags();
  auto security_origin = CalculateOrigin(owner_document, sandbox_flags);

  // In some rare cases, we'll re-use a LocalDOMWindow for a new Document. For
  // example, when a script calls window.open("..."), the browser gives
  // JavaScript a window synchronously but kicks off the load in the window
  // asynchronously. Web sites expect that modifications that they make to the
  // window object synchronously won't be blown away when the network load
  // commits. To make that happen, we "securely transition" the existing
  // LocalDOMWindow to the Document that results from the network load. See also
  // Document::IsSecureTransitionTo.
  if (!ShouldReuseDOMWindow(frame_->DomWindow(), security_origin.get())) {
    auto* agent = GetWindowAgentForOrigin(frame_.Get(), security_origin.get());
    frame_->SetDOMWindow(MakeGarbageCollected<LocalDOMWindow>(*frame_, agent));

    if (origin_policy_.has_value()) {
      // Convert from WebVector<WebString> to WTF::Vector<WTF::String>
      Vector<String> ids;
      for (const auto& id : origin_policy_->ids) {
        ids.push_back(id);
      }

      frame_->DomWindow()->SetOriginPolicyIds(ids);
    }

    // Inheriting cases use their agent's origin-isolated value, which is set by
    // whatever they're inheriting from.
    //
    // javascript: URLs use the calling page as their Url() value, so we need to
    // exclude them explicitly.
    //
    // TODO(https://crbug.com/1111897): This call is likely to happen happen
    // multiple times per agent, since navigations can happen multiple times per
    // agent. This is subpar. Currently a DCHECK guards against it happening
    // multiple times *with different values*, but ideally we would use a better
    // architecture.
    if (!Document::ShouldInheritSecurityOriginFromOwner(Url()) &&
        commit_reason_ != CommitReason::kJavascriptUrl) {
      agent->SetIsOriginIsolated(origin_isolated_);
    }
  } else {
    if (frame_->GetSettings()->GetShouldReuseGlobalForUnownedMainFrame() &&
        frame_->IsMainFrame()) {
      // When GetShouldReuseGlobalForUnownedMainFrame() causes a main frame's
      // window to be reused, we should not inherit the initial empty document's
      // Agent, which was a universal access Agent.
      // This happens only in android webview.
      frame_->DomWindow()->ResetWindowAgent(
          GetWindowAgentForOrigin(frame_.Get(), security_origin.get()));
    }
    frame_->DomWindow()->ClearForReuse();
  }

  // Now that we have the final window and Agent, ensure the security origin has
  // the appropriate agent cluster id. This may derive a new security origin.
  security_origin = security_origin->GetOriginForAgentCluster(
      frame_->DomWindow()->GetAgent()->cluster_id());

  SecurityContext& security_context = frame_->DomWindow()->GetSecurityContext();
  security_context.SetContentSecurityPolicy(content_security_policy_.Get());
  security_context.ApplySandboxFlags(sandbox_flags);
  // Conceptually, SecurityOrigin doesn't have to be initialized after sandbox
  // flags are applied, but there's a UseCounter in SetSecurityOrigin() that
  // wants to inspect sandbox flags.
  security_context.SetSecurityOrigin(std::move(security_origin));
  // Requires SecurityOrigin to be initialized.
  OriginTrialContext::AddTokensFromHeader(
      frame_->DomWindow(), response_.HttpHeaderField(http_names::kOriginTrial));

  if (auto* parent = frame_->Tree().Parent()) {
    const SecurityContext* parent_context = parent->GetSecurityContext();
    security_context.SetInsecureRequestPolicy(
        parent_context->GetInsecureRequestPolicy());
    for (auto to_upgrade : parent_context->InsecureNavigationsToUpgrade())
      security_context.AddInsecureNavigationUpgrade(to_upgrade);
  }
  frame_->DomWindow()->SetAddressSpace(ip_address_space_);

  if (base::FeatureList::IsEnabled(blink::features::kPolicyContainer)) {
    // SVG image documents go throught this but don't have a PolicyContainer, so
    // ignore them.
    if (frame_->GetPolicyContainer()) {
      frame_->DomWindow()->SetReferrerPolicy(
          frame_->GetPolicyContainer()->GetReferrerPolicy(), false);
    }
  }
  String referrer_policy_header =
      response_.HttpHeaderField(http_names::kReferrerPolicy);
  if (!referrer_policy_header.IsNull()) {
    CountUse(WebFeature::kReferrerPolicyHeader);
    frame_->DomWindow()->ParseAndSetReferrerPolicy(referrer_policy_header);
    if (base::FeatureList::IsEnabled(blink::features::kPolicyContainer)) {
      if (frame_->GetPolicyContainer()) {
        frame_->GetPolicyContainer()->UpdateReferrerPolicy(
            frame_->DomWindow()->GetReferrerPolicy());
      }
    }
  }
}

void DocumentLoader::CommitNavigation() {
  DCHECK_LT(state_, kCommitted);
  DCHECK(frame_->GetPage());
  DCHECK(!frame_->GetDocument() || !frame_->GetDocument()->IsActive());
  DCHECK_EQ(frame_->Tree().ChildCount(), 0u);
  state_ = kCommitted;

  // Prepare a DocumentInit before clearing the frame, because it may need to
  // inherit an aliased security context.
  Document* owner_document = nullptr;

  // TODO(dcheng): This differs from the behavior of both IE and Firefox: the
  // origin is inherited from the document that loaded the URL.
  if (Document::ShouldInheritSecurityOriginFromOwner(Url())) {
    Frame* owner_frame = frame_->Tree().Parent();
    if (!owner_frame)
      owner_frame = frame_->Loader().Opener();
    if (auto* owner_local_frame = DynamicTo<LocalFrame>(owner_frame))
      owner_document = owner_local_frame->GetDocument();
  }

  // Re-validate Document Policy feature before installing the new document.
  if (!RuntimeEnabledFeatures::DocumentPolicyEnabled(
          owner_document ? owner_document->GetExecutionContext() : nullptr)) {
    document_policy_ = DocumentPolicy::ParsedDocumentPolicy{};
  }

  if (document_policy_.feature_state.contains(
          mojom::blink::DocumentPolicyFeature::kForceLoadAtTop)) {
    navigation_scroll_allowed_ = !(
        document_policy_
            .feature_state[mojom::blink::DocumentPolicyFeature::kForceLoadAtTop]
            .BoolValue());
  }

  LocalDOMWindow* previous_window = frame_->DomWindow();
  InitializeWindow(owner_document);

  SecurityContextInit security_init(frame_->DomWindow());
  // FeaturePolicy and DocumentPolicy require SecurityOrigin and origin trials
  // to be initialized.
  // TODO(iclelland): Add Feature-Policy-Report-Only to Origin Policy.
  security_init.ApplyFeaturePolicy(frame_.Get(), response_, origin_policy_,
                                   frame_policy_);
  // |document_policy_| is parsed in document loader because it is
  // compared with |frame_policy.required_document_policy| to decide
  // whether to block the document load or not.
  // |report_only_document_policy| does not block the page load. Its
  // initialization is delayed to
  // SecurityContextInit::InitializeDocumentPolicy(), similar to
  // |report_only_feature_policy|.
  security_init.ApplyDocumentPolicy(
      document_policy_,
      response_.HttpHeaderField(http_names::kDocumentPolicyReportOnly));

  WillCommitNavigation();

  Document* document = frame_->DomWindow()->InstallNewDocument(
      DocumentInit::Create()
          .WithWindow(frame_->DomWindow(), owner_document)
          .ForInitialEmptyDocument(commit_reason_ ==
                                   CommitReason::kInitialization)
          .WithURL(Url())
          .WithTypeFrom(MimeType())
          .WithSrcdocDocument(loading_srcdoc_)
          .WithNewRegistrationContext()
          .WithWebBundleClaimedUrl(web_bundle_claimed_url_)
          .WithUkmSourceId(ukm_source_id_));

  RecordUseCountersForCommit();
  RecordConsoleMessagesForCommit();

  // Clear the user activation state.
  // TODO(crbug.com/736415): Clear this bit unconditionally for all frames.
  if (frame_->IsMainFrame())
    frame_->ClearUserActivation();

  // The DocumentLoader was flagged as activated if it needs to notify the frame
  // that it was activated before navigation. Update the frame state based on
  // the new value.
  if (frame_->HadStickyUserActivationBeforeNavigation() !=
      had_sticky_activation_) {
    frame_->SetHadStickyUserActivationBeforeNavigation(had_sticky_activation_);
    frame_->GetLocalFrameHostRemote()
        .HadStickyUserActivationBeforeNavigationChanged(had_sticky_activation_);
  }

  bool should_clear_window_name =
      previous_window && frame_->IsMainFrame() && !frame_->Loader().Opener() &&
      !frame_->DomWindow()->GetSecurityOrigin()->IsSameOriginWith(
          previous_window->GetSecurityOrigin());
  if (should_clear_window_name) {
    // TODO(andypaicu): experimentalSetNullName will just record the fact
    // that the name would be nulled and if the name is accessed after we will
    // fire a UseCounter. If we decide to move forward with this change, we'd
    // actually clean the name here.
    // frame_->tree().setName(g_null_atom);
    frame_->Tree().ExperimentalSetNulledName();
  }

  bool should_clear_cross_browsing_context_group_window_name =
      previous_window && frame_->IsMainFrame() && !frame_->Loader().Opener() &&
      is_cross_browsing_context_group_navigation_;
  if (should_clear_cross_browsing_context_group_window_name) {
    // TODO(shuuran): CrossBrowsingContextGroupSetNulledName will just
    // record the fact that the name would be nulled and if the name is accessed
    // after we will fire a UseCounter. If we decide to move forward with
    // this change, we'd actually clean the name here.
    // frame_->tree().setName(g_null_atom);
    frame_->Tree().CrossBrowsingContextGroupSetNulledName();
  }

  // MHTML archive's URL is usually a local file. However the main resource
  // within the archive has a public URL and must be used to resolve all the
  // relative links.
  if (loading_main_document_from_mhtml_archive_) {
    ArchiveResource* main_resource = archive_->MainResource();
    KURL main_resource_url = main_resource ? main_resource->Url() : KURL();
    if (!main_resource_url.IsEmpty())
      document->SetBaseURLOverride(main_resource_url);
  }

  if (commit_reason_ == CommitReason::kXSLT)
    DocumentXSLT::SetHasTransformSource(*document);

  DidInstallNewDocument(document);

  // This must be called before the document is opened, otherwise HTML parser
  // will use stale values from HTMLParserOption.
  DidCommitNavigation();

  if (requestor_origin_) {
    const scoped_refptr<const SecurityOrigin> url_origin =
        SecurityOrigin::Create(Url());

    is_same_origin_navigation_ =
        requestor_origin_->IsSameOriginWith(url_origin.get()) &&
        Url().ProtocolIsInHTTPFamily();
  }

  // The PaintHolding feature defers compositor commits until content has
  // been painted or 500ms have passed, whichever comes first. The additional
  // PaintHoldingCrossOrigin feature allows PaintHolding even for cross-origin
  // navigations, otherwise only same-origin navigations have deferred commits.
  // We also require that this be an html document served via http.
  if (base::FeatureList::IsEnabled(blink::features::kPaintHolding) &&
      IsA<HTMLDocument>(document) && Url().ProtocolIsInHTTPFamily() &&
      (is_same_origin_navigation_ ||
       base::FeatureList::IsEnabled(
           blink::features::kPaintHoldingCrossOrigin))) {
    document->SetDeferredCompositorCommitIsAllowed(true);
  } else {
    document->SetDeferredCompositorCommitIsAllowed(false);
  }

  if (response_.IsHTTP() && navigation_timing_info_) {
    // The response is being copied here to pass the ServerTiming info.
    // TODO(yoav): copy the ServerTiming info directly.
    navigation_timing_info_->SetFinalResponse(response_);
  }

  {
    // Notify the browser process about the commit.
    FrameNavigationDisabler navigation_disabler(*frame_);
    if (commit_reason_ == CommitReason::kInitialization) {
      GetLocalFrameClient().DidCreateInitialEmptyDocument();
    } else if (IsJavaScriptURLOrXSLTCommit()) {
      GetLocalFrameClient().DidCommitDocumentReplacementNavigation(this);
    } else {
      GetLocalFrameClient().DispatchDidCommitLoad(
          history_item_.Get(), LoadTypeToCommitType(load_type_),
          previous_window != frame_->DomWindow());
    }
    // TODO(dgozman): make DidCreateScriptContext notification call currently
    // triggered by installing new document happen here, after commit.
  }
  // Note: this must be called after DispatchDidCommitLoad() for
  // metrics to be correctly sent to the browser process.
  if (commit_reason_ != CommitReason::kInitialization)
    use_counter_.DidCommitLoad(frame_);
  if (load_type_ == WebFrameLoadType::kBackForward) {
    if (Page* page = frame_->GetPage())
      page->HistoryNavigationVirtualTimePauser().UnpauseVirtualTime();
  }

  // FeaturePolicy is reset in the browser process on commit, so this needs to
  // be initialized and replicated to the browser process after commit messages
  // are sent.
  GetLocalFrameClient().DidSetFramePolicyHeaders(
      frame_->DomWindow()->GetSandboxFlags(),
      security_init.FeaturePolicyHeader(), document_policy_.feature_state);

  // Load the document if needed.
  StartLoadingResponse();
}

void DocumentLoader::CreateParserPostCommit() {
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
  DispatchLinkHeaderPreloads(nullptr /* viewport */,
                             PreloadHelper::kOnlyLoadNonMedia);

  // Initializing origin trials might force window proxy initialization,
  // which later triggers CHECK when swapping in via WebFrame::Swap().
  // We can safely omit installing original trials on initial empty document
  // and wait for the real load.
  if (commit_reason_ != CommitReason::kInitialization) {
    LocalDOMWindow* window = frame_->DomWindow();
    if (frame_->GetSettings()
            ->GetForceTouchEventFeatureDetectionForInspector()) {
      window->GetOriginTrialContext()->AddFeature(
          OriginTrialFeature::kTouchEventFeatureDetection);
    }

    // Enable any origin trials that have been force enabled for this commit.
    window->GetOriginTrialContext()->AddForceEnabledTrials(
        force_enabled_origin_trials_);

#if BUILDFLAG(IS_ASH)
    // Enable Auto Picture-in-Picture feature for the built-in Chrome OS Video
    // Player app.
    const url::Origin origin = window->GetSecurityOrigin()->ToUrlOrigin();
    if (origin.scheme() == "chrome-extension" &&
        origin.DomainIs("jcgeabjmjgoblfofpppfkcoakmfobdko") &&
        origin.port() == 0) {
      window->GetOriginTrialContext()->AddFeature(
          OriginTrialFeature::kAutoPictureInPicture);
    }
#endif

    OriginTrialContext::ActivateNavigationFeaturesFromInitiator(
        window, &initiator_origin_trial_features_);
  }

  ParserSynchronizationPolicy parsing_policy =
      RuntimeEnabledFeatures::ForceSynchronousHTMLParsingEnabled()
          ? kAllowDeferredParsing
          : kAllowAsynchronousParsing;
  if (IsJavaScriptURLOrXSLTCommit() ||
      !Document::ThreadedParsingEnabledForTesting()) {
    parsing_policy = kForceSynchronousParsing;
  }
  const AtomicString& encoding = commit_reason_ == CommitReason::kXSLT
                                     ? "UTF-8"
                                     : response_.TextEncodingName();

  Document* document = frame_->GetDocument();
  parser_ = document->OpenForNavigation(parsing_policy, MimeType(), encoding);

  // XSLT processing converts the response into UTF-8 before sending it through
  // the DocumentParser, but we should still report the original encoding when
  // script queries it via document.characterSet.
  if (commit_reason_ == CommitReason::kXSLT) {
    DocumentEncodingData data;
    data.SetEncoding(WTF::TextEncoding(response_.TextEncodingName()));
    document->SetEncodingData(data);
  }

  // If this is a scriptable parser and there is a resource, register the
  // resource's cache handler with the parser.
  ScriptableDocumentParser* scriptable_parser =
      parser_->AsScriptableDocumentParser();
  if (scriptable_parser && cached_metadata_handler_)
    scriptable_parser->SetInlineScriptCacheHandler(cached_metadata_handler_);

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
  if (loading_main_document_from_mhtml_archive_) {
    if (ArchiveResource* main_resource = archive_->MainResource())
      return main_resource->MimeType();
  }

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

void DocumentLoader::RecordUseCountersForCommit() {
  // Pre-commit state, count usage the use counter associated with "this"
  // (provisional document loader) instead of frame_'s document loader.
  if (response_.DidServiceWorkerNavigationPreload())
    CountUse(WebFeature::kServiceWorkerNavigationPreload);
  if (!frame_->IsMainFrame() && response_.GetCTPolicyCompliance() ==
                                    ResourceResponse::kCTPolicyDoesNotComply) {
    // Exclude main-frame navigations; those are tracked elsewhere.
    CountUse(
        WebFeature::kCertificateTransparencyNonCompliantResourceInSubframe);
  }
  if (RuntimeEnabledFeatures::ForceLoadAtTopEnabled(frame_->DomWindow()))
    CountUse(WebFeature::kForceLoadAtTop);

  if (response_.IsSignedExchangeInnerResponse()) {
    CountUse(WebFeature::kSignedExchangeInnerResponse);
    CountUse(frame_->IsMainFrame()
                 ? WebFeature::kSignedExchangeInnerResponseInMainFrame
                 : WebFeature::kSignedExchangeInnerResponseInSubFrame);
  }

  if (!response_.HttpHeaderField(http_names::kRequireDocumentPolicy).IsNull())
    CountUse(WebFeature::kRequireDocumentPolicyHeader);

  if (was_blocked_by_document_policy_)
    CountUse(WebFeature::kDocumentPolicyCausedPageUnload);

  // Required document policy can either come from iframe attribute or HTTP
  // header 'Require-Document-Policy'.
  if (!frame_policy_.required_document_policy.empty())
    CountUse(WebFeature::kRequiredDocumentPolicy);
}

void DocumentLoader::RecordConsoleMessagesForCommit() {
  // Log if the document was blocked by CSP checks now that the new Document has
  // been created and console messages will be properly displayed.
  if (was_blocked_by_csp_) {
    ConsoleError("Refused to display '" +
                 response_.CurrentRequestUrl().ElidedString() +
                 "' because it has not opted into the following policy "
                 "required by its embedder: '" +
                 GetFrameLoader().RequiredCSP() + "'.");
  }

  if (was_blocked_by_document_policy_) {
    // TODO(chenleihu): Add which document policy violated in error string,
    // instead of just displaying serialized required document policy.
    ConsoleError(
        "Refused to display '" + response_.CurrentRequestUrl().ElidedString() +
        "' because it violates the following document policy "
        "required by its embedder: '" +
        DocumentPolicy::Serialize(frame_policy_.required_document_policy)
            .value_or("[Serialization Error]")
            .c_str() +
        "'.");
  }

  // Report the ResourceResponse now that the new Document has been created and
  // console messages will be properly displayed.
  frame_->Console().ReportResourceResponseReceived(
      this, main_resource_identifier_, response_);
}

void DocumentLoader::ReportPreviewsIntervention() const {
  // Only send reports for main frames.
  if (!frame_->IsMainFrame())
    return;

  // Verify that certain types are not on main frame requests.
  DCHECK_NE(PreviewsTypes::kClientLoFiAutoReload, previews_state_);
  DCHECK_NE(PreviewsTypes::kSubresourceRedirectOn, previews_state_);

  static_assert(PreviewsTypes::kPreviewsStateLast ==
                    PreviewsTypes::kSubresourceRedirectOn,
                "If a new Preview type is added, verify that the Intervention "
                "Report should be sent (or not sent) for that type.");

  // If the preview type is not unspecified, off, or no transform, it is a
  // preview that needs to be reported.
  if (previews_state_ == PreviewsTypes::kPreviewsUnspecified ||
      previews_state_ & PreviewsTypes::kPreviewsOff ||
      previews_state_ & PreviewsTypes::kPreviewsNoTransform) {
    return;
  }

  Intervention::GenerateReport(
      frame_, "LitePageServed",
      "Modified page load behavior on the page because the page was expected "
      "to take a long amount of time to load. "
      "https://www.chromestatus.com/feature/5148050062311424");
}

void DocumentLoader::ApplyClientHintsConfig(
    const WebVector<network::mojom::WebClientHintsType>& enabled_client_hints) {
  for (auto ch : enabled_client_hints)
    client_hints_preferences_.SetShouldSend(ch);
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

base::TimeDelta DocumentLoader::RemainingTimeToLCPLimit() const {
  // We shouldn't call this function before navigation start
  DCHECK(!document_load_timing_.NavigationStart().is_null());
  base::TimeTicks lcp_limit =
      document_load_timing_.NavigationStart() +
      base::TimeDelta::FromMilliseconds(
          features::kAlignFontDisplayAutoTimeoutWithLCPGoalTimeoutParam.Get());
  base::TimeTicks now = clock_->NowTicks();
  if (now < lcp_limit)
    return lcp_limit - now;
  return base::TimeDelta();
}

mojom::blink::ContentSecurityNotifier&
DocumentLoader::GetContentSecurityNotifier() {
  if (!content_security_notifier_.is_bound()) {
    GetFrame()->Client()->GetBrowserInterfaceBroker().GetInterface(
        content_security_notifier_.BindNewPipeAndPassReceiver());
  }
  return *content_security_notifier_;
}

bool DocumentLoader::ConsumeTextFragmentToken() {
  bool token_value = has_text_fragment_token_;
  has_text_fragment_token_ = false;
  return token_value;
}

DEFINE_WEAK_IDENTIFIER_MAP(DocumentLoader)

}  // namespace blink
