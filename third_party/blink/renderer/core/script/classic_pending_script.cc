// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/script/classic_pending_script.h"

#include "base/feature_list.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/loader/lcp_critical_path_predictor_util.h"
#include "third_party/blink/public/mojom/script/script_type.mojom-blink-forward.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/referrer_script_info.h"
#include "third_party/blink/renderer/bindings/core/v8/script_streamer.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/scriptable_document_parser.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/html_document.h"
#include "third_party/blink/renderer/core/inspector/inspector_trace_events.h"
#include "third_party/blink/renderer/core/lcp_critical_path_predictor/lcp_critical_path_predictor.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/resource/script_resource.h"
#include "third_party/blink/renderer/core/loader/subresource_integrity_helper.h"
#include "third_party/blink/renderer/core/loader/url_matcher.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/script/document_write_intervention.h"
#include "third_party/blink/renderer/core/script/script_loader.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/instrumentation/histogram.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/loader/allowed_by_nosniff.h"
#include "third_party/blink/renderer/platform/loader/fetch/cached_metadata.h"
#include "third_party/blink/renderer/platform/loader/fetch/detachable_use_counter.h"
#include "third_party/blink/renderer/platform/loader/fetch/memory_cache.h"
#include "third_party/blink/renderer/platform/loader/fetch/raw_resource.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_client.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {
namespace {

InlineScriptStreamer* GetInlineScriptStreamer(const String& source,
                                              Document& document) {
  ScriptableDocumentParser* scriptable_parser =
      document.GetScriptableDocumentParser();
  if (!scriptable_parser)
    return nullptr;

  // The inline script streamers are keyed by the full source text to make sure
  // the script that was parsed in the background scanner exactly matches the
  // script we want to compile here.
  return scriptable_parser->TakeInlineScriptStreamer(source);
}

}  // namespace

// <specdef href="https://html.spec.whatwg.org/C/#fetch-a-classic-script">
ClassicPendingScript* ClassicPendingScript::Fetch(
    const KURL& url,
    Document& element_document,
    const ScriptFetchOptions& options,
    CrossOriginAttributeValue cross_origin,
    const WTF::TextEncoding& encoding,
    ScriptElementBase* element,
    FetchParameters::DeferOption defer,
    scheduler::TaskAttributionInfo* parent_task) {
  ExecutionContext* context = element_document.GetExecutionContext();
  FetchParameters params(options.CreateFetchParameters(
      url, context->GetSecurityOrigin(), context->GetCurrentWorld(),
      cross_origin, encoding, defer));

  ClassicPendingScript* pending_script =
      MakeGarbageCollected<ClassicPendingScript>(
          element, TextPosition::MinimumPosition(), KURL(), KURL(), String(),
          ScriptSourceLocationType::kExternalFile, options,
          /*is_external=*/true, parent_task);

  // [Intervention]
  // For users on slow connections, we want to avoid blocking the parser in
  // the main frame on script loads inserted via document.write, since it
  // can add significant delays before page content is displayed on the
  // screen.
  pending_script->intervened_ =
      MaybeDisallowFetchForDocWrittenScript(params, element_document);

  // <spec step="2">Set request's client to settings object.</spec>
  //
  // Note: |element_document| corresponds to the settings object.
  //
  // We allow streaming, as WatchForLoad() is always called when the script
  // needs to execute and the ScriptResource is not finished, so
  // SetClientIsWaitingForFinished is always set on the resource.

  Page* page = element_document.GetPage();
  v8_compile_hints::V8CrowdsourcedCompileHintsProducer* compile_hints_producer =
      nullptr;
  v8_compile_hints::V8CrowdsourcedCompileHintsConsumer* compile_hints_consumer =
      nullptr;
  if (page->MainFrame()->IsLocalFrame()) {
    compile_hints_producer = &page->GetV8CrowdsourcedCompileHintsProducer();
    compile_hints_consumer = &page->GetV8CrowdsourcedCompileHintsConsumer();
  }
  const bool v8_compile_hints_magic_comment_runtime_enabled =
      RuntimeEnabledFeatures::JavaScriptCompileHintsMagicRuntimeEnabled(
          element_document.GetExecutionContext());

  ScriptResource::Fetch(params, element_document.Fetcher(), pending_script,
                        context->GetIsolate(), ScriptResource::kAllowStreaming,
                        compile_hints_producer, compile_hints_consumer,
                        v8_compile_hints_magic_comment_runtime_enabled);
  pending_script->CheckState();
  return pending_script;
}

ClassicPendingScript* ClassicPendingScript::CreateInline(
    ScriptElementBase* element,
    const TextPosition& starting_position,
    const KURL& source_url,
    const KURL& base_url,
    const String& source_text,
    ScriptSourceLocationType source_location_type,
    const ScriptFetchOptions& options,
    scheduler::TaskAttributionInfo* parent_task) {
  ClassicPendingScript* pending_script =
      MakeGarbageCollected<ClassicPendingScript>(
          element, starting_position, source_url, base_url, source_text,
          source_location_type, options, /*is_external=*/false, parent_task);
  pending_script->CheckState();
  return pending_script;
}

ClassicPendingScript::ClassicPendingScript(
    ScriptElementBase* element,
    const TextPosition& starting_position,
    const KURL& source_url_for_inline_script,
    const KURL& base_url_for_inline_script,
    const String& source_text_for_inline_script,
    ScriptSourceLocationType source_location_type,
    const ScriptFetchOptions& options,
    bool is_external,
    scheduler::TaskAttributionInfo* parent_task)
    : PendingScript(element, starting_position, parent_task),
      options_(options),
      source_url_for_inline_script_(source_url_for_inline_script),
      base_url_for_inline_script_(base_url_for_inline_script),
      source_text_for_inline_script_(source_text_for_inline_script),
      source_location_type_(source_location_type),
      is_external_(is_external),
      ready_state_(is_external ? kWaitingForResource : kReady) {
  CHECK(GetElement());

  if (is_external_) {
    DCHECK(base_url_for_inline_script_.IsNull());
    DCHECK(source_text_for_inline_script_.IsNull());
  } else {
    DCHECK(!base_url_for_inline_script_.IsNull());
    DCHECK(!source_text_for_inline_script_.IsNull());
  }
}

ClassicPendingScript::~ClassicPendingScript() = default;

NOINLINE void ClassicPendingScript::CheckState() const {
  DCHECK(GetElement());
  DCHECK_EQ(is_external_, !!GetResource());
  switch (ready_state_) {
    case kWaitingForResource:
      DCHECK(is_external_);
      DCHECK(!classic_script_);
      break;
    case kWaitingForCacheConsumer:
      DCHECK(is_external_);
      DCHECK(classic_script_);
      DCHECK(classic_script_->CacheConsumer());
      break;
    case kReady:
      DCHECK(!is_external_ || classic_script_);
      break;
    case kErrorOccurred:
      DCHECK(is_external_);
      DCHECK(!classic_script_);
      break;
  }
}


void ClassicPendingScript::RecordThirdPartyRequestWithCookieIfNeeded(
    const ResourceResponse& response) const {
  // Can be null in some cases where loading failed.
  if (response.IsNull())
    return;

  // Ignore cookie-less requests.
  if (!response.WasCookieInRequest()) {
    return;
  }

  // Ignore scripts that can be delayed. This is only async scripts currently.
  // kDefer and kForceDefer don't count as delayable since delaying them
  // artificially further while prerendering would prevent the page from making
  // progress.
  if (GetSchedulingType() == ScriptSchedulingType::kAsync) {
    return;
  }

  ExecutionContext* execution_context = OriginalExecutionContext();
  Document* element_document = OriginalElementDocument();
  if (!execution_context || !element_document) {
    return;
  }

  scoped_refptr<const SecurityOrigin> top_frame_origin =
      element_document->TopFrameOrigin();
  if (!top_frame_origin) {
    return;
  }

  // The use counter is meant to gather data for prerendering: how often do
  // pages make credentialed requests to third parties from first-party frames,
  // that cannot be delayed during prerendering until the page is navigated to.
  // Therefore...
  String doc_registrable_domain =
      execution_context->GetSecurityOrigin()->RegistrableDomain();
  // Ignore third-party frames.
  if (top_frame_origin->RegistrableDomain() != doc_registrable_domain) {
    return;
  }

  scoped_refptr<SecurityOrigin> script_origin =
      SecurityOrigin::Create(response.ResponseUrl());
  // Ignore first-party requests.
  if (doc_registrable_domain == script_origin->RegistrableDomain()) {
    return;
  }

  execution_context->CountUse(
      mojom::blink::WebFeature::
          kUndeferrableThirdPartySubresourceRequestWithCookie);
}

void ClassicPendingScript::DisposeInternal() {
  ClearResource();
}

bool ClassicPendingScript::IsEligibleForLowPriorityAsyncScriptExecution()
    const {
  DCHECK_EQ(GetSchedulingType(), ScriptSchedulingType::kAsync);

  static const bool feature_enabled =
      base::FeatureList::IsEnabled(features::kLowPriorityAsyncScriptExecution);
  if (!feature_enabled)
    return false;

  Document* element_document = OriginalElementDocument();

  if (!IsA<HTMLDocument>(element_document))
    return false;

  // Most LCP elements are provided by the main frame, and delaying subframe's
  // resources seems not to improve LCP.
  const bool main_frame_only =
      features::kLowPriorityAsyncScriptExecutionMainFrameOnlyParam.Get();
  if (main_frame_only && !element_document->IsInOutermostMainFrame())
    return false;

  const base::TimeDelta feature_limit =
      features::kLowPriorityAsyncScriptExecutionFeatureLimitParam.Get();
  if (!feature_limit.is_zero() &&
      element_document->GetStartTime().Elapsed() > feature_limit) {
    return false;
  }

  // Do not enable kLowPriorityAsyncScriptExecution on reload.
  // No specific reason to use element document here instead of context
  // document though.
  Document& top_document = element_document->TopDocument();
  if (top_document.Loader() &&
      top_document.Loader()->IsReloadedOrFormSubmitted()) {
    return false;
  }

  // Check if LCP influencing scripts are to be excluded.
  const bool exclude_lcp_influencers =
      features::kLowPriorityAsyncScriptExecutionExcludeLcpInfluencersParam
          .Get();
  if (exclude_lcp_influencers && LcppScriptObserverEnabled()) {
    if (LCPCriticalPathPredictor* lcpp = top_document.GetFrame()->GetLCPP()) {
      if (lcpp->IsLcpInfluencerScript(GetResource()->Url())) {
        return false;
      }
    }
  }

  const bool disable_when_lcp_not_in_html =
      features::kLowPriorityAsyncScriptExecutionDisableWhenLcpNotInHtmlParam
          .Get();
  if (disable_when_lcp_not_in_html && !top_document.IsLcpElementFoundInHtml()) {
    // If LCP element isn't found in main document HTML during preload scanning,
    // disable delaying.
    return false;
  }

  const bool cross_site_only =
      features::kLowPriorityAsyncScriptExecutionCrossSiteOnlyParam.Get();
  if (cross_site_only && GetResource() &&
      element_document->GetExecutionContext()) {
    scoped_refptr<const SecurityOrigin> url_origin =
        SecurityOrigin::Create(GetResource()->Url());
    if (url_origin->IsSameSiteWith(
            element_document->GetExecutionContext()->GetSecurityOrigin())) {
      return false;
    }
  }

  if (GetElement() && GetElement()->IsPotentiallyRenderBlocking())
    return false;

  // We don't delay async scripts that have matched a resource in the preload
  // cache, because we're using <link rel=preload> as a signal that the script
  // is higher-than-usual priority, and therefore should be executed earlier
  // rather than later.
  if (GetResource() && GetResource()->IsLinkPreload())
    return false;

  bool is_ad_resource =
      GetResource() && GetResource()->GetResourceRequest().IsAdResource();
  static const features::AsyncScriptExperimentalSchedulingTarget target =
      features::kLowPriorityAsyncScriptExecutionTargetParam.Get();
  switch (target) {
    case features::AsyncScriptExperimentalSchedulingTarget::kAds:
      if (!is_ad_resource) {
        return false;
      }
      break;
    case features::AsyncScriptExperimentalSchedulingTarget::kNonAds:
      if (is_ad_resource) {
        return false;
      }
      break;
    case features::AsyncScriptExperimentalSchedulingTarget::kBoth:
      break;
  }

  const bool exclude_non_parser_inserted =
      features::kLowPriorityAsyncScriptExecutionExcludeNonParserInsertedParam
          .Get();
  if (exclude_non_parser_inserted && !parser_inserted()) {
    return false;
  }

  const bool exclude_scripts_via_document_write =
      features::kLowPriorityAsyncScriptExecutionExcludeDocumentWriteParam.Get();
  if (exclude_scripts_via_document_write && is_in_document_write()) {
    return false;
  }

  const bool opt_out_low =
      features::kLowPriorityAsyncScriptExecutionOptOutLowFetchPriorityHintParam
          .Get();
  const bool opt_out_auto =
      features::kLowPriorityAsyncScriptExecutionOptOutAutoFetchPriorityHintParam
          .Get();
  const bool opt_out_high =
      features::kLowPriorityAsyncScriptExecutionOptOutHighFetchPriorityHintParam
          .Get();

  if (GetResource()) {
    switch (GetResource()->GetResourceRequest().GetFetchPriorityHint()) {
      case mojom::blink::FetchPriorityHint::kLow:
        if (opt_out_low) {
          return false;
        }
        break;
      case mojom::blink::FetchPriorityHint::kAuto:
        if (opt_out_auto) {
          return false;
        }
        break;
      case mojom::blink::FetchPriorityHint::kHigh:
        if (opt_out_high) {
          return false;
        }
        break;
    }
  }

  return true;
}

void ClassicPendingScript::NotifyFinished(Resource* resource) {
  // The following SRI checks need to be here because, unfortunately, fetches
  // are not done purely according to the Fetch spec. In particular,
  // different requests for the same resource do not have different
  // responses; the memory cache can (and will) return the exact same
  // Resource object.
  //
  // For different requests, the same Resource object will be returned and
  // will not be associated with the particular request.  Therefore, when the
  // body of the response comes in, there's no way to validate the integrity
  // of the Resource object against a particular request (since there may be
  // several pending requests all tied to the identical object, and the
  // actual requests are not stored).
  //
  // In order to simulate the correct behavior, Blink explicitly does the SRI
  // checks here, when a PendingScript tied to a particular request is
  // finished (and in the case of a StyleSheet, at the point of execution),
  // while having proper Fetch checks in the fetch module for use in the
  // fetch JavaScript API. In a future world where the ResourceFetcher uses
  // the Fetch algorithm, this should be fixed by having separate Response
  // objects (perhaps attached to identical Resource objects) per request.
  //
  // See https://crbug.com/500701 for more information.
  CheckState();
  DCHECK(GetResource());

  // If the original execution context/element document is gone, consider this
  // as network error. Anyway the script wouldn't evaluated / no events are
  // fired, so this is not observable.
  ExecutionContext* execution_context = OriginalExecutionContext();
  Document* element_document = OriginalElementDocument();
  if (!execution_context || execution_context->IsContextDestroyed() ||
      !element_document || !element_document->IsActive()) {
    AdvanceReadyState(kErrorOccurred);
    return;
  }

  SubresourceIntegrityHelper::DoReport(*execution_context,
                                       resource->IntegrityReportInfo());

  // It is possible to get back a script resource with integrity metadata
  // for a request with an empty integrity attribute. In that case, the
  // integrity check should be skipped, as the integrity may not have been
  // "meant" for this specific request. If the resource is being served from
  // the preload cache however, we know any associated integrity metadata and
  // checks were destined for this request, so we cannot skip the integrity
  // check.
  bool integrity_failure = false;
  if (!options_.GetIntegrityMetadata().empty() || resource->IsLinkPreload()) {
    integrity_failure = resource->IntegrityDisposition() !=
                        ResourceIntegrityDisposition::kPassed;
  }

  if (intervened_) {
    CrossOriginAttributeValue cross_origin =
        GetCrossOriginAttributeValue(GetElement()->CrossOriginAttributeValue());
    PossiblyFetchBlockedDocWriteScript(resource, *element_document, options_,
                                       cross_origin);
  }

  // <specdef href="https://fetch.spec.whatwg.org/#concept-main-fetch">
  // <spec step="17">If response is not a network error and any of the following
  // returns blocked</spec>
  // <spec step="17.C">should internalResponse to request be blocked due to its
  // MIME type</spec>
  // <spec step="17.D">should internalResponse to request be blocked due to
  // nosniff</spec>
  // <spec step="17">then set response and internalResponse to a network
  // error.</spec>
  auto* fetcher = execution_context->Fetcher();
  const bool mime_type_failure = !AllowedByNosniff::MimeTypeAsScript(
      fetcher->GetUseCounter(), &fetcher->GetConsoleLogger(),
      resource->GetResponse(), AllowedByNosniff::MimeTypeCheck::kLaxForElement);

  TRACE_EVENT_WITH_FLOW1(TRACE_DISABLED_BY_DEFAULT("v8.compile"),
                         "ClassicPendingScript::NotifyFinished", this,
                         TRACE_EVENT_FLAG_FLOW_OUT, "data",
                         [&](perfetto::TracedValue context) {
                           inspector_parse_script_event::Data(
                               std::move(context), resource->InspectorId(),
                               resource->Url().GetString());
                         });

  // Ordinal ErrorOccurred(), SRI, and MIME check are all considered as network
  // errors in the Fetch spec.
  bool error_occurred =
      resource->ErrorOccurred() || integrity_failure || mime_type_failure;
  if (error_occurred) {
    AdvanceReadyState(kErrorOccurred);
    return;
  }

  // At this point, the load is successful, and ClassicScript is created.
  classic_script_ =
      ClassicScript::CreateFromResource(To<ScriptResource>(resource), options_);

  // We'll still wait for ScriptCacheConsumer before marking this PendingScript
  // ready.
  if (classic_script_->CacheConsumer()) {
    AdvanceReadyState(kWaitingForCacheConsumer);
    // TODO(leszeks): Decide whether kNetworking is the right task type here.
    classic_script_->CacheConsumer()->NotifyClientWaiting(
        this, classic_script_,
        execution_context->GetTaskRunner(TaskType::kNetworking));
  } else {
    // Either there was never a cache consume, or it was dropped. Either way, we
    // are ready.
    AdvanceReadyState(kReady);
  }
}

void ClassicPendingScript::NotifyCacheConsumeFinished() {
  CHECK_EQ(ready_state_, kWaitingForCacheConsumer);
  if (IsDisposed()) {
    // Silently ignore if `this` is already Dispose()d, because `this` is no
    // longer used.
    return;
  }
  AdvanceReadyState(kReady);
}

void ClassicPendingScript::Trace(Visitor* visitor) const {
  visitor->Trace(classic_script_);
  ResourceClient::Trace(visitor);
  PendingScript::Trace(visitor);
}

ClassicScript* ClassicPendingScript::GetSource() const {
  CheckState();
  DCHECK(IsReady());

  if (ready_state_ == kErrorOccurred)
    return nullptr;

  TRACE_EVENT0("blink", "ClassicPendingScript::GetSource");
  if (!is_external_) {
    InlineScriptStreamer* streamer = nullptr;
    // We only create an inline cache handler for html-embedded scripts, not
    // for scripts produced by document.write, or not parser-inserted. This is
    // because we expect those to be too dynamic to benefit from caching.
    // TODO(leszeks): ScriptSourceLocationType was previously only used for UMA,
    // so it's a bit of a layer violation to use it for affecting cache
    // behaviour. We should decide whether it is ok for this parameter to be
    // used for behavioural changes (and if yes, update its documentation), or
    // otherwise trigger this behaviour differently.
    Document* element_document = OriginalElementDocument();
    if (source_location_type_ == ScriptSourceLocationType::kInline &&
        element_document && element_document->IsActive()) {
      streamer = GetInlineScriptStreamer(source_text_for_inline_script_,
                                         *element_document);
    }

    DCHECK(!GetResource());
    ScriptStreamer::RecordStreamingHistogram(
        GetSchedulingType(), streamer,
        ScriptStreamer::NotStreamingReason::kInlineScript);

    return ClassicScript::Create(
        source_text_for_inline_script_,
        ClassicScript::StripFragmentIdentifier(source_url_for_inline_script_),
        base_url_for_inline_script_, options_, source_location_type_,
        SanitizeScriptErrors::kDoNotSanitize, nullptr, StartingPosition(),
        streamer ? ScriptStreamer::NotStreamingReason::kInvalid
                 : ScriptStreamer::NotStreamingReason::kInlineScript,
        streamer);
  }

  DCHECK(classic_script_);

  // Record histograms here, because these uses `GetSchedulingType()` but it
  // might be unavailable yet at the time of `NotifyFinished()`.
  DCHECK(GetResource()->IsLoaded());
  RecordThirdPartyRequestWithCookieIfNeeded(GetResource()->GetResponse());

  ScriptStreamer::RecordStreamingHistogram(
      GetSchedulingType(), classic_script_->Streamer(),
      classic_script_->NotStreamingReason());

  TRACE_EVENT_WITH_FLOW1(TRACE_DISABLED_BY_DEFAULT("v8.compile"),
                         "ClassicPendingScript::GetSource", this,
                         TRACE_EVENT_FLAG_FLOW_IN, "not_streamed_reason",
                         classic_script_->NotStreamingReason());

  return classic_script_.Get();
}

// static
bool ClassicPendingScript::StateIsReady(ReadyState state) {
  return state >= kReady;
}

bool ClassicPendingScript::IsReady() const {
  CheckState();
  return StateIsReady(ready_state_);
}

void ClassicPendingScript::AdvanceReadyState(ReadyState new_ready_state) {
  // We will allow exactly these state transitions:
  //
  // kWaitingForResource -> kWaitingForCacheConsumer -> [kReady, kErrorOccurred]
  //                     |                           ^
  //                     `---------------------------'
  //
  switch (ready_state_) {
    case kWaitingForResource:
      CHECK(new_ready_state == kReady || new_ready_state == kErrorOccurred ||
            new_ready_state == kWaitingForCacheConsumer);
      break;
    case kWaitingForCacheConsumer:
      CHECK(new_ready_state == kReady);
      break;
    case kReady:
    case kErrorOccurred:
      NOTREACHED_IN_MIGRATION();
      break;
  }

  // All the ready states are marked not reachable above, so we can't have been
  // ready beforehand.
  DCHECK(!StateIsReady(ready_state_));

  ready_state_ = new_ready_state;

  // Did we transition into a 'ready' state?
  if (IsReady() && IsWatchingForLoad())
    PendingScriptFinished();
}

bool ClassicPendingScript::WasCanceled() const {
  if (!is_external_)
    return false;
  return GetResource()->WasCanceled();
}

KURL ClassicPendingScript::UrlForTracing() const {
  if (!is_external_ || !GetResource())
    return NullURL();

  return GetResource()->Url();
}

}  // namespace blink
