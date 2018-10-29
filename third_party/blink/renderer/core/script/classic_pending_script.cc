// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/script/classic_pending_script.h"

#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/script_source_code.h"
#include "third_party/blink/renderer/bindings/core/v8/script_streamer.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/scriptable_document_parser.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/loader/allowed_by_nosniff.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/resource/script_resource.h"
#include "third_party/blink/renderer/core/loader/subresource_integrity_helper.h"
#include "third_party/blink/renderer/core/script/document_write_intervention.h"
#include "third_party/blink/renderer/core/script/script_loader.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/histogram.h"
#include "third_party/blink/renderer/platform/loader/fetch/cached_metadata.h"
#include "third_party/blink/renderer/platform/loader/fetch/memory_cache.h"
#include "third_party/blink/renderer/platform/loader/fetch/raw_resource.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_client.h"
#include "third_party/blink/renderer/platform/loader/fetch/source_keyed_cached_metadata_handler.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

// <specdef href="https://html.spec.whatwg.org/#fetch-a-classic-script">
ClassicPendingScript* ClassicPendingScript::Fetch(
    const KURL& url,
    Document& element_document,
    const ScriptFetchOptions& options,
    CrossOriginAttributeValue cross_origin,
    const WTF::TextEncoding& encoding,
    ScriptElementBase* element,
    FetchParameters::DeferOption defer) {
  FetchParameters params = options.CreateFetchParameters(
      url, element_document.GetSecurityOrigin(), cross_origin, encoding, defer);

  ClassicPendingScript* pending_script = new ClassicPendingScript(
      element, TextPosition(), ScriptSourceLocationType::kExternalFile, options,
      true /* is_external */);

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
  ScriptResource::Fetch(params, element_document.Fetcher(), pending_script);
  pending_script->CheckState();
  return pending_script;
}

ClassicPendingScript* ClassicPendingScript::CreateInline(
    ScriptElementBase* element,
    const TextPosition& starting_position,
    ScriptSourceLocationType source_location_type,
    const ScriptFetchOptions& options) {
  ClassicPendingScript* pending_script =
      new ClassicPendingScript(element, starting_position, source_location_type,
                               options, false /* is_external */);
  pending_script->CheckState();
  return pending_script;
}

ClassicPendingScript::ClassicPendingScript(
    ScriptElementBase* element,
    const TextPosition& starting_position,
    ScriptSourceLocationType source_location_type,
    const ScriptFetchOptions& options,
    bool is_external)
    : PendingScript(element, starting_position),
      options_(options),
      base_url_for_inline_script_(
          is_external ? KURL() : element->GetDocument().BaseURL()),
      source_text_for_inline_script_(is_external ? String()
                                                 : element->TextFromChildren()),
      source_location_type_(source_location_type),
      is_external_(is_external),
      ready_state_(is_external ? kWaitingForResource : kReady),
      integrity_failure_(false),
      is_currently_streaming_(false),
      not_streamed_reason_(is_external
                               ? ScriptStreamer::kDidntTryToStartStreaming
                               : ScriptStreamer::kInlineScript) {
  CHECK(GetElement());
  MemoryCoordinator::Instance().RegisterClient(this);
}

ClassicPendingScript::~ClassicPendingScript() {}

NOINLINE void ClassicPendingScript::CheckState() const {
  // TODO(hiroshige): Turn these CHECK()s into DCHECK() before going to beta.
  CHECK(GetElement());
  CHECK_EQ(is_external_, !!GetResource());
  CHECK(GetResource() || !streamer_);
}

namespace {

enum class StreamedBoolean {
  // Must match BooleanStreamed in enums.xml.
  kNotStreamed = 0,
  kStreamed = 1,
  kMaxValue = kStreamed
};

void RecordStartedStreamingHistogram(ScriptSchedulingType type,
                                     bool did_use_streamer) {
  StreamedBoolean streamed = did_use_streamer ? StreamedBoolean::kStreamed
                                              : StreamedBoolean::kNotStreamed;
  switch (type) {
    case ScriptSchedulingType::kParserBlocking: {
      UMA_HISTOGRAM_ENUMERATION(
          "WebCore.Scripts.ParsingBlocking.StartedStreaming", streamed);
      break;
    }
    case ScriptSchedulingType::kDefer: {
      UMA_HISTOGRAM_ENUMERATION("WebCore.Scripts.Deferred.StartedStreaming",
                                streamed);
      break;
    }
    case ScriptSchedulingType::kAsync: {
      UMA_HISTOGRAM_ENUMERATION("WebCore.Scripts.Async.StartedStreaming",
                                streamed);
      break;
    }
    default: {
      UMA_HISTOGRAM_ENUMERATION("WebCore.Scripts.Other.StartedStreaming",
                                streamed);
      break;
    }
  }
}

void RecordNotStreamingReasonHistogram(
    ScriptSchedulingType type,
    ScriptStreamer::NotStreamingReason reason) {
  switch (type) {
    case ScriptSchedulingType::kParserBlocking: {
      UMA_HISTOGRAM_ENUMERATION(
          "WebCore.Scripts.ParsingBlocking.NotStreamingReason", reason,
          ScriptStreamer::NotStreamingReason::kCount);
      break;
    }
    case ScriptSchedulingType::kDefer: {
      UMA_HISTOGRAM_ENUMERATION("WebCore.Scripts.Deferred.NotStreamingReason",
                                reason,
                                ScriptStreamer::NotStreamingReason::kCount);
      break;
    }
    case ScriptSchedulingType::kAsync: {
      UMA_HISTOGRAM_ENUMERATION("WebCore.Scripts.Async.NotStreamingReason",
                                reason,
                                ScriptStreamer::NotStreamingReason::kCount);
      break;
    }
    default: {
      UMA_HISTOGRAM_ENUMERATION("WebCore.Scripts.Other.NotStreamingReason",
                                reason,
                                ScriptStreamer::NotStreamingReason::kCount);
      break;
    }
  }
}

}  // namespace

void ClassicPendingScript::RecordStreamingHistogram(
    ScriptSchedulingType type,
    bool can_use_streamer,
    ScriptStreamer::NotStreamingReason reason) {
  RecordStartedStreamingHistogram(type, can_use_streamer);
  if (!can_use_streamer) {
    DCHECK_NE(ScriptStreamer::kInvalid, reason);
    RecordNotStreamingReasonHistogram(type, reason);
  }
}

void ClassicPendingScript::DisposeInternal() {
  MemoryCoordinator::Instance().UnregisterClient(this);
  ClearResource();
  integrity_failure_ = false;
  CancelStreaming();
}

void ClassicPendingScript::StreamingFinished() {
  CheckState();
  DCHECK(streamer_);  // Should only be called by ScriptStreamer.
  DCHECK(IsCurrentlyStreaming());

  if (ready_state_ == kWaitingForStreaming) {
    FinishWaitingForStreaming();
  } else if (ready_state_ == kReadyStreaming) {
    FinishReadyStreaming();
  } else {
    NOTREACHED();
  }

  DCHECK(!IsCurrentlyStreaming());
}

void ClassicPendingScript::FinishWaitingForStreaming() {
  CheckState();
  DCHECK(GetResource());
  DCHECK_EQ(ready_state_, kWaitingForStreaming);

  bool error_occurred = GetResource()->ErrorOccurred() || integrity_failure_;
  AdvanceReadyState(error_occurred ? kErrorOccurred : kReady);
}

void ClassicPendingScript::FinishReadyStreaming() {
  CheckState();
  DCHECK(GetResource());
  DCHECK_EQ(ready_state_, kReadyStreaming);
  AdvanceReadyState(kReady);
}

void ClassicPendingScript::CancelStreaming() {
  if (!streamer_)
    return;

  streamer_->Cancel();
  streamer_ = nullptr;
  streamer_done_.Reset();
  is_currently_streaming_ = false;
  DCHECK(!IsCurrentlyStreaming());
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
  ScriptElementBase* element = GetElement();
  if (element) {
    SubresourceIntegrityHelper::DoReport(element->GetDocument(),
                                         GetResource()->IntegrityReportInfo());

    // It is possible to get back a script resource with integrity metadata
    // for a request with an empty integrity attribute. In that case, the
    // integrity check should be skipped, so this check ensures that the
    // integrity attribute isn't empty in addition to checking if the
    // resource has empty integrity metadata.
    if (!element->IntegrityAttributeValue().IsEmpty()) {
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

  // We are now waiting for script streaming to finish.
  // If there is no script streamer, this step completes immediately.
  AdvanceReadyState(kWaitingForStreaming);
  if (streamer_)
    streamer_->NotifyFinished();
  else
    FinishWaitingForStreaming();
}

void ClassicPendingScript::DataReceived(Resource* resource,
                                        const char*,
                                        size_t) {
  if (streamer_)
    streamer_->NotifyAppendData(ToScriptResource(resource));
}

void ClassicPendingScript::Trace(blink::Visitor* visitor) {
  visitor->Trace(streamer_);
  ResourceClient::Trace(visitor);
  MemoryCoordinatorClient::Trace(visitor);
  PendingScript::Trace(visitor);
}

static SingleCachedMetadataHandler* GetInlineCacheHandler(const String& source,
                                                          Document& document) {
  if (!RuntimeEnabledFeatures::CacheInlineScriptCodeEnabled())
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

ClassicScript* ClassicPendingScript::GetSource(const KURL& document_url) const {
  CheckState();
  DCHECK(IsReady());

  if (ready_state_ == kErrorOccurred)
    return nullptr;

  if (!is_external_) {
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
    }

    DCHECK(!streamer_);
    DCHECK_EQ(not_streamed_reason_, ScriptStreamer::kInlineScript);
    RecordStreamingHistogram(GetSchedulingType(), false, not_streamed_reason_);

    ScriptSourceCode source_code(source_text_for_inline_script_,
                                 source_location_type_, cache_handler,
                                 document_url, StartingPosition());
    return ClassicScript::Create(source_code, base_url_for_inline_script_,
                                 options_, kSharableCrossOrigin);
  }

  DCHECK(GetResource()->IsLoaded());
  ScriptResource* resource = ToScriptResource(GetResource());

  // If the MIME check fails, which is considered as load failure.
  if (!AllowedByNosniff::MimeTypeAsScript(
          GetElement()->GetDocument().ContextDocument(),
          resource->GetResponse())) {
    return nullptr;
  }

  // Check if we can use the script streamer.
  bool streamer_ready = false;
  ScriptStreamer::NotStreamingReason not_streamed_reason = not_streamed_reason_;
  if (streamer_) {
    DCHECK_EQ(not_streamed_reason, ScriptStreamer::kInvalid);
    if (streamer_->StreamingSuppressed()) {
      not_streamed_reason = streamer_->StreamingSuppressedReason();
    } else if (ready_state_ == kErrorOccurred) {
      not_streamed_reason = ScriptStreamer::kErrorOccurred;
    } else if (ready_state_ == kReadyStreaming) {
      not_streamed_reason = ScriptStreamer::kStreamerNotReadyOnGetSource;
    } else {
      // Streamer can be used to compile script.
      DCHECK_EQ(ready_state_, kReady);
      streamer_ready = true;
    }
  }
  RecordStreamingHistogram(GetSchedulingType(), streamer_ready,
                           not_streamed_reason);

  ScriptSourceCode source_code(streamer_ready ? streamer_ : nullptr, resource,
                               not_streamed_reason);
  // The base URL for external classic script is
  //
  // <spec href="https://html.spec.whatwg.org/#concept-script-base-url">
  // ... the URL from which the script was obtained, ...</spec>
  const KURL& base_url = source_code.Url();
  return ClassicScript::Create(source_code, base_url, options_,
                               resource->CalculateAccessControlStatus());
}

void ClassicPendingScript::SetStreamer(ScriptStreamer* streamer) {
  DCHECK(streamer);
  DCHECK(!streamer_);
  DCHECK(!IsWatchingForLoad() || ready_state_ != kWaitingForResource);
  DCHECK(!streamer->IsFinished());
  DCHECK(ready_state_ == kWaitingForResource || ready_state_ == kReady);

  streamer_ = streamer;
  is_currently_streaming_ = true;
  if (streamer && ready_state_ == kReady)
    AdvanceReadyState(kReadyStreaming);

  CheckState();
}

bool ClassicPendingScript::IsReady() const {
  CheckState();
  return ready_state_ >= kReady;
}

void ClassicPendingScript::AdvanceReadyState(ReadyState new_ready_state) {
  // We will allow exactly these state transitions:
  //
  // kWaitingForResource -> kWaitingForStreaming -> [kReady, kErrorOccurred]
  // kReady -> kReadyStreaming -> kReady
  switch (ready_state_) {
    case kWaitingForResource:
      CHECK_EQ(new_ready_state, kWaitingForStreaming);
      break;
    case kWaitingForStreaming:
      CHECK(new_ready_state == kReady || new_ready_state == kErrorOccurred);
      break;
    case kReady:
      CHECK_EQ(new_ready_state, kReadyStreaming);
      break;
    case kReadyStreaming:
      CHECK_EQ(new_ready_state, kReady);
      break;
    case kErrorOccurred:
      NOTREACHED();
      break;
  }

  bool old_is_ready = IsReady();
  ready_state_ = new_ready_state;

  // Did we transition into a 'ready' state?
  if (IsReady() && !old_is_ready && IsWatchingForLoad())
    PendingScriptFinished();

  // Did we finish streaming?
  if (IsCurrentlyStreaming()) {
    if (ready_state_ == kReady || ready_state_ == kErrorOccurred) {
      // Call the streamer_done_ callback. Ensure that is_currently_streaming_
      // is reset only after the callback returns, to prevent accidentally
      // start streaming by work done within the callback. (crbug.com/754360)
      base::OnceClosure done = std::move(streamer_done_);
      if (done)
        std::move(done).Run();
      is_currently_streaming_ = false;
    }
  }

  // Streaming-related post conditions:

  // To help diagnose crbug.com/78426, we'll temporarily add some DCHECKs
  // that are a subset of the DCHECKs below:
  if (IsCurrentlyStreaming()) {
    DCHECK(streamer_);
    DCHECK(!streamer_->IsFinished());
  }

  // IsCurrentlyStreaming should match what streamer_ thinks.
  DCHECK_EQ(IsCurrentlyStreaming(), streamer_ && !streamer_->IsFinished());
  // IsCurrentlyStreaming should match the ready_state_.
  DCHECK_EQ(IsCurrentlyStreaming(),
            ready_state_ == kReadyStreaming ||
                (streamer_ && (ready_state_ == kWaitingForResource ||
                               ready_state_ == kWaitingForStreaming)));
  // We can only have a streamer_done_ callback if we are actually streaming.
  DCHECK(IsCurrentlyStreaming() || !streamer_done_);
}

void ClassicPendingScript::OnPurgeMemory() {
  CheckState();
  // TODO(crbug.com/846951): the implementation of CancelStreaming() is
  // currently incorrect and consequently a call to this method was removed from
  // here.
}

bool ClassicPendingScript::StartStreamingIfPossible(
    base::OnceClosure done) {
  if (IsCurrentlyStreaming())
    return false;

  // We can start streaming in two states: While still loading
  // (kWaitingForResource), or after having loaded (kReady).
  if (ready_state_ != kWaitingForResource && ready_state_ != kReady)
    return false;

  Document* document = &GetElement()->GetDocument();
  if (!document || !document->GetFrame())
    return false;

  ScriptState* script_state = ToScriptStateForMainWorld(document->GetFrame());
  if (!script_state)
    return false;

  // To support streaming re-try, we'll clear the existing streamer if
  // it exists; it claims to be finished; but it's finished because streaming
  // has been suppressed.
  if (streamer_ && streamer_->StreamingSuppressed() &&
      streamer_->IsFinished()) {
    DCHECK_EQ(ready_state_, kReady);
    DCHECK(!streamer_done_);
    DCHECK(!IsCurrentlyStreaming());
    streamer_.Clear();
  }

  if (streamer_)
    return false;

  // The two checks above should imply that we're not presently streaming.
  DCHECK(!IsCurrentlyStreaming());

  // Parser blocking scripts tend to do a lot of work in the 'finished'
  // callbacks, while async + in-order scripts all do control-like activities
  // (like posting new tasks). Use the 'control' queue only for control tasks.
  // (More details in discussion for cl 500147.)
  auto task_type = GetSchedulingType() == ScriptSchedulingType::kParserBlocking
                       ? TaskType::kNetworking
                       : TaskType::kNetworkingControl;

  DCHECK(!streamer_);
  DCHECK(!IsCurrentlyStreaming());
  DCHECK(!streamer_done_);
  ScriptStreamer::StartStreaming(this, document->GetTaskRunner(task_type),
                                 &not_streamed_reason_);
  DCHECK(streamer_ || not_streamed_reason_ != ScriptStreamer::kInvalid);
  bool success = streamer_ && !streamer_->IsStreamingFinished();

  // If we have successfully started streaming, we are required to call the
  // callback.
  DCHECK_EQ(success, IsCurrentlyStreaming());
  if (success)
    streamer_done_ = std::move(done);
  return success;
}

bool ClassicPendingScript::IsCurrentlyStreaming() const {
  return is_currently_streaming_;
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
