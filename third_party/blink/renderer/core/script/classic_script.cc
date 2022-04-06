// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/script/classic_script.h"

#include "third_party/blink/public/web/web_script_source.h"
#include "third_party/blink/renderer/bindings/core/v8/script_controller.h"
#include "third_party/blink/renderer/bindings/core/v8/worker_or_worklet_script_controller.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/workers/worker_or_worklet_global_scope.h"
#include "third_party/blink/renderer/core/workers/worker_reporting_proxy.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

namespace {

ParkableString TreatNullSourceAsEmpty(const ParkableString& source) {
  // The following is the historical comment for this method, while this might
  // be already obsolete, because `TreatNullSourceAsEmpty()` has been applied in
  // all constructors since before.
  //
  // ScriptSourceCode allows for the representation of the null/not-there-really
  // ScriptSourceCode value.  Encoded by way of a source_.IsNull() being true,
  // with the nullary constructor to be used to construct such a value.
  //
  // Should the other constructors be passed a null string, that is interpreted
  // as representing the empty script. Consequently, we need to disambiguate
  // between such null string occurrences.  Do that by converting the latter
  // case's null strings into empty ones.
  if (source.IsNull())
    return ParkableString();

  return source;
}

KURL SanitizeBaseUrl(const KURL& raw_base_url,
                     SanitizeScriptErrors sanitize_script_errors) {
  // https://html.spec.whatwg.org/C/#creating-a-classic-script
  // 2. If muted errors is true, then set baseURL to about:blank.
  // [spec text]
  if (sanitize_script_errors == SanitizeScriptErrors::kSanitize) {
    return BlankURL();
  }

  return raw_base_url;
}

String SourceMapUrlFromResponse(const ResourceResponse& response) {
  String source_map_url = response.HttpHeaderField(http_names::kSourceMap);
  if (!source_map_url.IsEmpty())
    return source_map_url;

  // Try to get deprecated header.
  return response.HttpHeaderField(http_names::kXSourceMap);
}

}  // namespace

KURL ClassicScript::StripFragmentIdentifier(const KURL& url) {
  if (url.IsEmpty())
    return KURL();

  if (!url.HasFragmentIdentifier())
    return url;

  KURL copy = url;
  copy.RemoveFragmentIdentifier();
  return copy;
}

ClassicScript* ClassicScript::Create(
    const String& source_text,
    const KURL& source_url,
    const KURL& base_url,
    const ScriptFetchOptions& fetch_options,
    ScriptSourceLocationType source_location_type,
    SanitizeScriptErrors sanitize_script_errors,
    SingleCachedMetadataHandler* cache_handler,
    const TextPosition& start_position,
    ScriptStreamer::NotStreamingReason not_streaming_reason) {
  // External files should use CreateFromResource().
  DCHECK(source_location_type != ScriptSourceLocationType::kExternalFile);

  return MakeGarbageCollected<ClassicScript>(
      ParkableString(source_text.Impl()), source_url, base_url, fetch_options,
      source_location_type, sanitize_script_errors, cache_handler,
      start_position, nullptr, not_streaming_reason);
}

ClassicScript* ClassicScript::CreateFromResource(
    ScriptResource* resource,
    const KURL& base_url,
    const ScriptFetchOptions& fetch_options,
    ScriptStreamer* streamer,
    ScriptStreamer::NotStreamingReason not_streamed_reason,
    ScriptCacheConsumer* cache_consumer) {
  DCHECK_EQ(!streamer, not_streamed_reason !=
                           ScriptStreamer::NotStreamingReason::kInvalid);

  ParkableString source;
  const char web_snapshot_prefix[4] = {'+', '+', '+', ';'};
  if (RuntimeEnabledFeatures::ExperimentalWebSnapshotsEnabled() &&
      resource->DataHasPrefix(base::span<const char>(web_snapshot_prefix))) {
    source = resource->RawSourceText();
  } else {
    source = resource->SourceText();
  }
  // We lose the encoding information from ScriptResource.
  // Not sure if that matters.
  return MakeGarbageCollected<ClassicScript>(
      source, StripFragmentIdentifier(resource->Url()), base_url, fetch_options,
      ScriptSourceLocationType::kExternalFile,
      resource->GetResponse().IsCorsSameOrigin()
          ? SanitizeScriptErrors::kDoNotSanitize
          : SanitizeScriptErrors::kSanitize,
      resource->CacheHandler(), TextPosition::MinimumPosition(), streamer,
      not_streamed_reason, cache_consumer,
      SourceMapUrlFromResponse(resource->GetResponse()));
}

ClassicScript* ClassicScript::CreateUnspecifiedScript(
    const String& source_text,
    ScriptSourceLocationType source_location_type,
    SanitizeScriptErrors sanitize_script_errors) {
  return MakeGarbageCollected<ClassicScript>(
      ParkableString(source_text.Impl()), KURL(), KURL(), ScriptFetchOptions(),
      source_location_type, sanitize_script_errors);
}

ClassicScript* ClassicScript::CreateUnspecifiedScript(
    const WebScriptSource& source,
    SanitizeScriptErrors sanitize_script_errors) {
  return MakeGarbageCollected<ClassicScript>(
      ParkableString(String(source.code).Impl()),
      StripFragmentIdentifier(source.url), KURL() /* base_url */,
      ScriptFetchOptions(), ScriptSourceLocationType::kUnknown,
      sanitize_script_errors);
}

ClassicScript::ClassicScript(
    const ParkableString& source_text,
    const KURL& source_url,
    const KURL& base_url,
    const ScriptFetchOptions& fetch_options,
    ScriptSourceLocationType source_location_type,
    SanitizeScriptErrors sanitize_script_errors,
    SingleCachedMetadataHandler* cache_handler,
    const TextPosition& start_position,
    ScriptStreamer* streamer,
    ScriptStreamer::NotStreamingReason not_streaming_reason,
    ScriptCacheConsumer* cache_consumer,
    const String& source_map_url)
    : Script(fetch_options, SanitizeBaseUrl(base_url, sanitize_script_errors)),
      source_text_(TreatNullSourceAsEmpty(source_text)),
      source_url_(source_url),
      source_location_type_(source_location_type),
      sanitize_script_errors_(sanitize_script_errors),
      cache_handler_(cache_handler),
      start_position_(start_position),
      streamer_(streamer),
      not_streaming_reason_(not_streaming_reason),
      cache_consumer_(cache_consumer),
      source_map_url_(source_map_url) {}

void ClassicScript::Trace(Visitor* visitor) const {
  Script::Trace(visitor);
  visitor->Trace(cache_handler_);
  visitor->Trace(streamer_);
  visitor->Trace(cache_consumer_);
}

ScriptEvaluationResult ClassicScript::RunScriptOnScriptStateAndReturnValue(
    ScriptState* script_state,
    ExecuteScriptPolicy policy,
    V8ScriptRunner::RethrowErrorsOption rethrow_errors) {
  return V8ScriptRunner::CompileAndRunScript(script_state, this, policy,
                                             std::move(rethrow_errors));
}

void ClassicScript::RunScript(LocalDOMWindow* window) {
  return RunScript(window,
                   ExecuteScriptPolicy::kDoNotExecuteScriptWhenScriptsDisabled);
}

void ClassicScript::RunScript(LocalDOMWindow* window,
                              ExecuteScriptPolicy policy) {
  v8::HandleScope handle_scope(window->GetIsolate());
  RunScriptAndReturnValue(window, policy);
}

ScriptEvaluationResult ClassicScript::RunScriptAndReturnValue(
    LocalDOMWindow* window,
    ExecuteScriptPolicy policy) {
  return RunScriptOnScriptStateAndReturnValue(
      ToScriptStateForMainWorld(window->GetFrame()), policy);
}

ScriptEvaluationResult ClassicScript::RunScriptInIsolatedWorldAndReturnValue(
    LocalDOMWindow* window,
    int32_t world_id) {
  DCHECK_GT(world_id, 0);

  // Unlike other methods, RunScriptInIsolatedWorldAndReturnValue()'s
  // default policy is kExecuteScriptWhenScriptsDisabled, to keep existing
  // behavior.
  ScriptState* script_state = nullptr;
  if (window->GetFrame()) {
    script_state = ToScriptState(window->GetFrame(),
                                 *DOMWrapperWorld::EnsureIsolatedWorld(
                                     ToIsolate(window->GetFrame()), world_id));
  }
  return RunScriptOnScriptStateAndReturnValue(
      script_state, ExecuteScriptPolicy::kExecuteScriptWhenScriptsDisabled);
}

bool ClassicScript::RunScriptOnWorkerOrWorklet(
    WorkerOrWorkletGlobalScope& global_scope) {
  DCHECK(global_scope.IsContextThread());

  v8::HandleScope handle_scope(
      global_scope.ScriptController()->GetScriptState()->GetIsolate());
  ScriptEvaluationResult result = RunScriptOnScriptStateAndReturnValue(
      global_scope.ScriptController()->GetScriptState());
  return result.GetResultType() == ScriptEvaluationResult::ResultType::kSuccess;
}

std::pair<size_t, size_t> ClassicScript::GetClassicScriptSizes() const {
  size_t cached_metadata_size =
      CacheHandler() ? CacheHandler()->GetCodeCacheSize() : 0;
  return std::pair<size_t, size_t>(SourceText().length(), cached_metadata_size);
}

}  // namespace blink
