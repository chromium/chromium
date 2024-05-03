// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/script/classic_script.h"

#include "third_party/blink/public/web/web_script_source.h"
#include "third_party/blink/renderer/bindings/core/v8/referrer_script_info.h"
#include "third_party/blink/renderer/bindings/core/v8/sanitize_script_errors.h"
#include "third_party/blink/renderer/bindings/core/v8/script_evaluation_result.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/loader/fetch/url_loader/cached_metadata_handler.h"

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
  if (!source_map_url.empty())
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
    CachedMetadataHandler* cache_handler,
    const TextPosition& start_position,
    ScriptStreamer::NotStreamingReason not_streaming_reason,
    InlineScriptStreamer* streamer) {
  // External files should use CreateFromResource().
  DCHECK(source_location_type != ScriptSourceLocationType::kExternalFile);

  return MakeGarbageCollected<ClassicScript>(
      ParkableString(source_text.Impl()), source_url, base_url, fetch_options,
      source_location_type, sanitize_script_errors, cache_handler,
      start_position, streamer, not_streaming_reason);
}

ClassicScript* ClassicScript::CreateFromResource(
    ScriptResource* resource,
    const ScriptFetchOptions& fetch_options) {
  // Check if we can use the script streamer.
  ScriptStreamer* streamer;
  ScriptStreamer::NotStreamingReason not_streamed_reason;
  std::tie(streamer, not_streamed_reason) =
      ScriptStreamer::TakeFrom(resource, mojom::blink::ScriptType::kClassic);
  DCHECK_EQ(!streamer, not_streamed_reason !=
                           ScriptStreamer::NotStreamingReason::kInvalid);

  ScriptCacheConsumer* cache_consumer = resource->TakeCacheConsumer();

  KURL source_url = StripFragmentIdentifier(resource->Url());

  // The base URL for external classic script is
  //
  // <spec href="https://html.spec.whatwg.org/C/#concept-script-base-url">
  // ... the URL from which the script was obtained, ...</spec>
  KURL base_url = resource->GetResponse().ResponseUrl();

  // We lose the encoding information from ScriptResource.
  // Not sure if that matters.
  return MakeGarbageCollected<ClassicScript>(
      resource->SourceText(), source_url, base_url, fetch_options,
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
    CachedMetadataHandler* cache_handler,
    const TextPosition& start_position,
    ScriptStreamer* streamer,
    ScriptStreamer::NotStreamingReason not_streaming_reason,
    ScriptCacheConsumer* cache_consumer,
    const String& source_map_url)
    : Script(fetch_options,
             SanitizeBaseUrl(base_url, sanitize_script_errors),
             source_url,
             start_position),
      source_text_(TreatNullSourceAsEmpty(source_text)),
      source_location_type_(source_location_type),
      sanitize_script_errors_(sanitize_script_errors),
      cache_handler_(cache_handler),
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

v8::Local<v8::Data> ClassicScript::CreateHostDefinedOptions(
    v8::Isolate* isolate) const {
  const ReferrerScriptInfo referrer_info(BaseUrl(), FetchOptions());

  v8::Local<v8::Data> host_defined_options =
      referrer_info.ToV8HostDefinedOptions(isolate, SourceUrl());

  return host_defined_options;
}

v8::ScriptOrigin ClassicScript::CreateScriptOrigin(v8::Isolate* isolate) const {
  // Only send the source mapping URL string to v8 if it is not empty.
  v8::Local<v8::Value> source_map_url_or_null;
  if (!SourceMapUrl().empty()) {
    source_map_url_or_null = V8String(isolate, SourceMapUrl());
  }
  // NOTE: For compatibility with WebCore, ClassicScript's line starts at
  // 1, whereas v8 starts at 0.
  // NOTE(kouhei): Probably this comment is no longer relevant and Blink lines
  // start at 1 only for historic reasons now. I guess we could change it, but
  // there's not much benefit doing so.
  return v8::ScriptOrigin(
      V8String(isolate, SourceUrl()), StartPosition().line_.ZeroBasedInt(),
      StartPosition().column_.ZeroBasedInt(),
      GetSanitizeScriptErrors() == SanitizeScriptErrors::kDoNotSanitize, -1,
      source_map_url_or_null,
      GetSanitizeScriptErrors() == SanitizeScriptErrors::kSanitize,
      false,  // is_wasm
      false,  // is_module
      CreateHostDefinedOptions(isolate));
}

ScriptEvaluationResult ClassicScript::RunScriptOnScriptStateAndReturnValue(
    ScriptState* script_state,
    ExecuteScriptPolicy policy,
    V8ScriptRunner::RethrowErrorsOption rethrow_errors) {
  bool sanitize = GetSanitizeScriptErrors() == SanitizeScriptErrors::kSanitize;
  probe::EvaluateScriptBlock probe_scope(script_state,
                                         sanitize ? SourceUrl() : BaseUrl(),
                                         /*module=*/false, sanitize);

  return V8ScriptRunner::CompileAndRunScript(script_state, this, policy,
                                             std::move(rethrow_errors));
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

}  // namespace blink
