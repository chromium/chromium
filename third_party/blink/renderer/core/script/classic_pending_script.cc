// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/script/classic_pending_script.h"

#include "base/feature_list.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/script/script_type.mojom-blink-forward.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/referrer_script_info.h"
#include "third_party/blink/renderer/bindings/core/v8/script_streamer.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/scriptable_document_parser.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/inspector/inspector_trace_events.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/resource/script_resource.h"
#include "third_party/blink/renderer/core/loader/subresource_integrity_helper.h"
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
#include "third_party/blink/renderer/platform/loader/fetch/source_keyed_cached_metadata_handler.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
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

namespace {
// The base URL for external classic script is
//
// <spec href="https://html.spec.whatwg.org/C/#concept-script-base-url">
// ... the URL from which the script was obtained, ...</spec>
KURL BaseUrl(const ScriptResource& resource) {
  return resource.GetResponse().ResponseUrl();
}

bool CheckIfEligibleForDelay(const KURL& url,
                             const Document& element_document,
                             const ScriptElementBase& element) {
  if (!base::FeatureList::IsEnabled(features::kDelayAsyncScriptExecution))
    return false;

  if (element.IsPotentiallyRenderBlocking())
    return false;

  if (features::kDelayAsyncScriptExecutionCrossSiteOnlyParam.Get()) {
    ExecutionContext* context = element_document.GetExecutionContext();
    scoped_refptr<const SecurityOrigin> src_security_origin =
        SecurityOrigin::Create(url);
    if (src_security_origin->IsSameSiteWith(context->GetSecurityOrigin()))
      return false;
  }

  return true;
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
    FetchParameters::DeferOption defer) {
  ExecutionContext* context = element_document.GetExecutionContext();
  FetchParameters params(options.CreateFetchParameters(
      url, context->GetSecurityOrigin(), context->GetCurrentWorld(),
      cross_origin, encoding, defer));

  ClassicPendingScript* pending_script =
      MakeGarbageCollected<ClassicPendingScript>(
          element, TextPosition::MinimumPosition(), KURL(), KURL(), String(),
          ScriptSourceLocationType::kExternalFile, options,
          true /* is_external */,
          CheckIfEligibleForDelay(url, element_document, *element));

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
  ScriptResource::Fetch(params, element_document.Fetcher(), pending_script,
                        ScriptResource::kAllowStreaming);
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
    const ScriptFetchOptions& options) {
  ClassicPendingScript* pending_script =
      MakeGarbageCollected<ClassicPendingScript>(
          element, starting_position, source_url, base_url, source_text,
          source_location_type, options, false /* is_external */,
          false /* is_eligible_for_delay */);
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
    bool is_eligible_for_delay)
    : PendingScript(element, starting_position),
      options_(options),
      source_url_for_inline_script_(source_url_for_inline_script),
      base_url_for_inline_script_(base_url_for_inline_script),
      source_text_for_inline_script_(source_text_for_inline_script),
      source_location_type_(source_location_type),
      is_external_(is_external),
      ready_state_(is_external ? kWaitingForResource : kReady),
      integrity_failure_(false),
      is_eligible_for_delay_(is_eligible_for_delay) {
  CHECK(GetElement());

  if (is_external_) {
    DCHECK(base_url_for_inline_script_.IsNull());
    DCHECK(source_text_for_inline_script_.IsNull());
  } else {
    DCHECK(!base_url_for_inline_script_.IsNull());
    DCHECK(!source_text_for_inline_script_.IsNull());
  }

  MemoryPressureListenerRegistry::Instance().RegisterClient(this);
}

ClassicPendingScript::~ClassicPendingScript() = default;

NOINLINE void ClassicPendingScript::CheckState() const {
  DCHECK(GetElement());
  DCHECK_EQ(is_external_, !!GetResource());
  if (ready_state_ == kWaitingForCacheConsumer) {
    DCHECK(cache_consumer_);
  } else if (ready_state_ == kWaitingForResource) {
    DCHECK(!cache_consumer_);
  }
}


void ClassicPendingScript::RecordThirdPartyRequestWithCookieIfNeeded(
    const ResourceResponse& response) const {
  // Can be null in some cases where loading failed.
  if (response.IsNull())
    return;

  scoped_refptr<SecurityOrigin> script_origin =
      SecurityOrigin::Create(response.ResponseUrl());
  const SecurityOrigin* doc_origin =
      GetElement()->GetExecutionContext()->GetSecurityOrigin();
  scoped_refptr<const SecurityOrigin> top_frame_origin =
      GetElement()->GetDocument().TopFrameOrigin();

  // The use counter is meant to gather data for prerendering: how often do
  // pages make credentialed requests to third parties from first-party frames,
  // that cannot be delayed during prerendering until the page is navigated to.
  // Therefore...

  // Ignore third-party frames.
  if (!top_frame_origin || top_frame_origin->RegistrableDomain() !=
                               doc_origin->RegistrableDomain()) {
    return;
  }

  // Ignore first-party requests.
  if (doc_origin->RegistrableDomain() == script_origin->RegistrableDomain())
    return;

  // Ignore cookie-less requests.
  if (!response.WasCookieInRequest())
    return;

  // Ignore scripts that can be delayed. This is only async scripts currently.
  // kDefer and kForceDefer don't count as delayable since delaying them
  // artificially further while prerendering would prevent the page from making
  // progress.
  if (GetSchedulingType() == ScriptSchedulingType::kAsync)
    return;

  GetElement()->GetExecutionContext()->CountUse(
      mojom::blink::WebFeature::
          kUndeferrableThirdPartySubresourceRequestWithCookie);
}


void ClassicPendingScript::DisposeInternal() {
  MemoryPressureListenerRegistry::Instance().UnregisterClient(this);
  ClearResource();
  integrity_failure_ = false;
}

bool ClassicPendingScript::IsEligibleForDelay() const {
  DCHECK_EQ(GetSchedulingType(), ScriptSchedulingType::kAsync);
  // We don't delay async scripts that have matched a resource in the preload
  // cache, because we're using <link rel=preload> as a signal that the script
  // is higher-than-usual priority, and therefore should be executed earlier
  // rather than later. IsLinkPreload() can't be checked in
  // CheckIfEligibleForDelay() since ClassicPendingScript::Fetch() initialize
  // the state.
  return is_eligible_for_delay_ && !GetResource()->IsLinkPreload();
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
  ScriptElementBase* element = GetElement();
  if (element) {
    SubresourceIntegrityHelper::DoReport(*element->GetExecutionContext(),
                                         GetResource()->IntegrityReportInfo());

    // It is possible to get back a script resource with integrity metadata
    // for a request with an empty integrity attribute. In that case, the
    // integrity check should be skipped, as the integrity may not have been
    // "meant" for this specific request. If the resource is being served from
    // the preload cache however, we know any associated integrity metadata and
    // checks were destined for this request, so we cannot skip the integrity
    // check.
    if (!options_.GetIntegrityMetadata().IsEmpty() ||
        GetResource()->IsLinkPreload()) {
      integrity_failure_ = GetResource()->IntegrityDisposition() !=
                           ResourceIntegrityDisposition::kPassed;
    }
  }

  if (intervened_) {
    CrossOriginAttributeValue cross_origin =
        GetCrossOriginAttributeValue(element->CrossOriginAttributeValue());
    PossiblyFetchBlockedDocWriteScript(resource, element->GetDocument(),
                                       options_, cross_origin);
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
  auto* fetcher = GetElement()->GetExecutionContext()->Fetcher();
  const bool mime_type_failure = !AllowedByNosniff::MimeTypeAsScript(
      fetcher->GetUseCounter(), &fetcher->GetConsoleLogger(),
      resource->GetResponse(), AllowedByNosniff::MimeTypeCheck::kLaxForElement);

  TRACE_EVENT_WITH_FLOW1(TRACE_DISABLED_BY_DEFAULT("v8.compile"),
                         "ClassicPendingScript::NotifyFinished", this,
                         TRACE_EVENT_FLAG_FLOW_OUT, "data",
                         [&](perfetto::TracedValue context) {
                           inspector_parse_script_event::Data(
                               std::move(context), GetResource()->InspectorId(),
                               GetResource()->Url().GetString());
                         });

  // Ordinal ErrorOccurred(), SRI, and MIME check are all considered as network
  // errors in the Fetch spec.
  bool error_occurred =
      GetResource()->ErrorOccurred() || integrity_failure_ || mime_type_failure;
  if (error_occurred) {
    AdvanceReadyState(kErrorOccurred);
    return;
  }

  auto* script_resource = To<ScriptResource>(GetResource());
  CHECK(!cache_consumer_);
  cache_consumer_ = script_resource->TakeCacheConsumer();

  if (cache_consumer_) {
    ExecutionContext* execution_context = element->GetExecutionContext();
    // Only wait for the cache consume if there is an execution context it can
    // notify us on.
    if (execution_context) {
      AdvanceReadyState(kWaitingForCacheConsumer);
      // TODO(leszeks): Decide whether kNetworking is the right task type here.
      cache_consumer_->NotifyClientWaiting(
          this, execution_context->GetTaskRunner(TaskType::kNetworking));

    } else {
      // Otherwise (probably when our Document isn't active anymore),
      // we can simply drop the cache consumer and let it be cleaned up by the
      // GC; no one is waiting on it so it's safe to just drop it without
      // needing to cancel or dispose it in any way.
      cache_consumer_ = nullptr;
    }
  }

  if (!cache_consumer_) {
    // Either there was never a cache consume, or it was dropped. Either way, we
    // are ready.
    AdvanceReadyState(kReady);
  }
}

void ClassicPendingScript::NotifyCacheConsumeFinished() {
  CHECK_EQ(ready_state_, kWaitingForCacheConsumer);
  AdvanceReadyState(kReady);
}

const ParkableString& ClassicPendingScript::GetSourceText() {
  // This method is an implementation of a virtual function defined by
  // ScriptCacheConsumerClient, and is only used for external scripts.
  CHECK(is_external_);

  return To<ScriptResource>(GetResource())->SourceText();
}

v8::ScriptOrigin ClassicPendingScript::GetScriptOrigin() {
  // This method is an implementation of a virtual function defined by
  // ScriptCacheConsumerClient, and is only used for external scripts.
  CHECK(is_external_);

  v8::Isolate* isolate = GetElement()->GetExecutionContext()->GetIsolate();
  auto* resource = To<ScriptResource>(GetResource());
  const KURL& source_url = ClassicScript::SourceUrlFromResource(*resource);
  TextPosition script_start_position = TextPosition::MinimumPosition();
  SanitizeScriptErrors sanitize_script_errors =
      ClassicScript::ShouldSanitizeScriptErrors(resource->GetResponse());
  String source_map_url =
      ClassicScript::SourceMapUrlFromResponse(resource->GetResponse());
  const KURL& base_url = BaseUrl(*resource);
  const ReferrerScriptInfo referrer_info(base_url, options_);
  v8::Local<v8::Data> host_defined_options =
      referrer_info.ToV8HostDefinedOptions(isolate, source_url);
  return v8::ScriptOrigin(
      isolate, V8String(isolate, source_url),
      script_start_position.line_.ZeroBasedInt(),
      script_start_position.column_.ZeroBasedInt(),
      sanitize_script_errors == SanitizeScriptErrors::kDoNotSanitize, -1,
      V8String(isolate, source_map_url),
      sanitize_script_errors == SanitizeScriptErrors::kSanitize,
      false,  // is_wasm
      false,  // is_module
      host_defined_options);
}

void ClassicPendingScript::Trace(Visitor* visitor) const {
  visitor->Trace(cache_consumer_);
  ResourceClient::Trace(visitor);
  MemoryPressureListener::Trace(visitor);
  PendingScript::Trace(visitor);
}

static SingleCachedMetadataHandler* GetInlineCacheHandler(const String& source,
                                                          Document& document) {
  if (!base::FeatureList::IsEnabled(kCacheInlineScriptCode))
    return nullptr;

  ScriptableDocumentParser* scriptable_parser =
      document.GetScriptableDocumentParser();
  if (!scriptable_parser)
    return nullptr;

  SourceKeyedCachedMetadataHandler* document_cache_handler =
      scriptable_parser->GetInlineScriptCacheHandler();

  if (!document_cache_handler)
    return nullptr;

  return document_cache_handler->HandlerForSource(source);
}

ClassicScript* ClassicPendingScript::GetSource() const {
  CheckState();
  DCHECK(IsReady());

  if (ready_state_ == kErrorOccurred)
    return nullptr;

  TRACE_EVENT0("blink", "ClassicPendingScript::GetSource");
  if (!is_external_) {
    InlineScriptStreamer* streamer = nullptr;
    SingleCachedMetadataHandler* cache_handler = nullptr;
    // We only create an inline cache handler for html-embedded scripts, not
    // for scripts produced by document.write, or not parser-inserted. This is
    // because we expect those to be too dynamic to benefit from caching.
    // TODO(leszeks): ScriptSourceLocationType was previously only used for UMA,
    // so it's a bit of a layer violation to use it for affecting cache
    // behaviour. We should decide whether it is ok for this parameter to be
    // used for behavioural changes (and if yes, update its documentation), or
    // otherwise trigger this behaviour differently.
    if (source_location_type_ == ScriptSourceLocationType::kInline) {
      cache_handler = GetInlineCacheHandler(source_text_for_inline_script_,
                                            GetElement()->GetDocument());
      streamer = GetInlineScriptStreamer(source_text_for_inline_script_,
                                         GetElement()->GetDocument());
    }

    DCHECK(!GetResource());
    ScriptStreamer::RecordStreamingHistogram(
        GetSchedulingType(), streamer,
        ScriptStreamer::NotStreamingReason::kInlineScript);

    return ClassicScript::Create(
        source_text_for_inline_script_,
        ClassicScript::StripFragmentIdentifier(source_url_for_inline_script_),
        base_url_for_inline_script_, options_, source_location_type_,
        SanitizeScriptErrors::kDoNotSanitize, cache_handler, StartingPosition(),
        streamer ? ScriptStreamer::NotStreamingReason::kInvalid
                 : ScriptStreamer::NotStreamingReason::kInlineScript,
        streamer);
  }

  DCHECK(GetResource()->IsLoaded());
  auto* resource = To<ScriptResource>(GetResource());
  RecordThirdPartyRequestWithCookieIfNeeded(resource->GetResponse());

  // Check if we can use the script streamer.
  ResourceScriptStreamer* streamer;
  ScriptStreamer::NotStreamingReason not_streamed_reason;
  std::tie(streamer, not_streamed_reason) = ResourceScriptStreamer::TakeFrom(
      resource, mojom::blink::ScriptType::kClassic);

  if (ready_state_ == kErrorOccurred) {
    not_streamed_reason = ScriptStreamer::NotStreamingReason::kErrorOccurred;
    streamer = nullptr;
  }
  if (streamer)
    CHECK_EQ(ready_state_, kReady);
  ScriptStreamer::RecordStreamingHistogram(GetSchedulingType(), streamer,
                                           not_streamed_reason);

  TRACE_EVENT_WITH_FLOW1(TRACE_DISABLED_BY_DEFAULT("v8.compile"),
                         "ClassicPendingScript::GetSource", this,
                         TRACE_EVENT_FLAG_FLOW_IN, "not_streamed_reason",
                         not_streamed_reason);

  const KURL& base_url = BaseUrl(*resource);
  return ClassicScript::CreateFromResource(resource, base_url, options_,
                                           streamer, not_streamed_reason,
                                           cache_consumer_);
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
      NOTREACHED();
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

void ClassicPendingScript::OnPurgeMemory() {
  CheckState();
  // TODO(crbug.com/846951): the implementation of CancelStreaming() is
  // currently incorrect and consequently a call to this method was removed from
  // here.
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
