// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SCRIPT_CLASSIC_SCRIPT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SCRIPT_CLASSIC_SCRIPT_H_

#include "third_party/blink/renderer/bindings/core/v8/sanitize_script_errors.h"
#include "third_party/blink/renderer/bindings/core/v8/script_source_location_type.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_script_runner.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/loader/resource/script_resource.h"
#include "third_party/blink/renderer/core/script/script.h"
#include "third_party/blink/renderer/platform/loader/fetch/script_fetch_options.h"

namespace blink {

struct WebScriptSource;

class CORE_EXPORT ClassicScript final : public Script {
 public:
  // For `source_url`.
  static KURL StripFragmentIdentifier(const KURL&);

  // For scripts specified in the HTML spec or for tests.
  // Please leave spec comments and spec links that explain given argument
  // values at non-test callers.
  static ClassicScript* Create(
      const String& source_text,
      const KURL& source_url,
      const KURL& base_url,
      const ScriptFetchOptions&,
      ScriptSourceLocationType = ScriptSourceLocationType::kUnknown,
      SanitizeScriptErrors = SanitizeScriptErrors::kSanitize,
      SingleCachedMetadataHandler* = nullptr,
      const TextPosition& start_position = TextPosition::MinimumPosition(),
      ScriptStreamer::NotStreamingReason =
          ScriptStreamer::NotStreamingReason::kInlineScript);
  static ClassicScript* CreateFromResource(ScriptResource*,
                                           const KURL& base_url,
                                           const ScriptFetchOptions&,
                                           ScriptStreamer*,
                                           ScriptStreamer::NotStreamingReason,
                                           ScriptCacheConsumer*);

  // For scripts not specified in the HTML spec.
  //
  // New callers should use SanitizeScriptErrors::kSanitize as a safe default
  // value, while some existing callers uses kDoNotSanitize to preserve existing
  // behavior.
  // TODO(crbug/1112266): Use kSanitize for all existing callers if possible, or
  // otherwise add comments why kDoNotSanitize should be used.
  static ClassicScript* CreateUnspecifiedScript(
      const String& source_text,
      ScriptSourceLocationType = ScriptSourceLocationType::kUnknown,
      SanitizeScriptErrors = SanitizeScriptErrors::kSanitize);
  static ClassicScript* CreateUnspecifiedScript(
      const WebScriptSource&,
      SanitizeScriptErrors = SanitizeScriptErrors::kSanitize);

  // Use Create*() helpers above.
  ClassicScript(
      const ParkableString& source_text,
      const KURL& source_url,
      const KURL& base_url,
      const ScriptFetchOptions&,
      ScriptSourceLocationType,
      SanitizeScriptErrors,
      SingleCachedMetadataHandler* = nullptr,
      const TextPosition& start_position = TextPosition::MinimumPosition(),
      ScriptStreamer* = nullptr,
      ScriptStreamer::NotStreamingReason =
          ScriptStreamer::NotStreamingReason::kInlineScript,
      ScriptCacheConsumer* = nullptr,
      const String& source_map_url = String());

  void Trace(Visitor*) const override;

  const ParkableString& SourceText() const { return source_text_; }
  const KURL& SourceUrl() const { return source_url_; }

  ScriptSourceLocationType SourceLocationType() const {
    return source_location_type_;
  }

  SanitizeScriptErrors GetSanitizeScriptErrors() const {
    return sanitize_script_errors_;
  }

  SingleCachedMetadataHandler* CacheHandler() const { return cache_handler_; }
  const TextPosition& StartPosition() const { return start_position_; }

  ScriptStreamer* Streamer() const { return streamer_; }
  ScriptStreamer::NotStreamingReason NotStreamingReason() const {
    return not_streaming_reason_;
  }

  ScriptCacheConsumer* CacheConsumer() const { return cache_consumer_; }

  const String& SourceMapUrl() const { return source_map_url_; }

  // TODO(crbug.com/1111134): Methods with ExecuteScriptPolicy are declared and
  // overloaded here to avoid modifying Script::RunScript*(), because this is a
  // tentative interface. When crbug/1111134 is done, these should be gone.
  // TODO(crbug.com/1111134): Refactor RunScript*() interfaces.
  void RunScript(LocalDOMWindow*) override;
  void RunScript(LocalDOMWindow*, ExecuteScriptPolicy);
  bool RunScriptOnWorkerOrWorklet(WorkerOrWorkletGlobalScope&) override;

  // Unlike RunScript() and RunScriptOnWorkerOrWorklet(), callers of the
  // following methods must enter a v8::HandleScope before calling.
  // TODO(crbug.com/1129743): Use ScriptEvaluationResult instead of
  // v8::Local<v8::Value> as the return type.
  ScriptEvaluationResult RunScriptOnScriptStateAndReturnValue(
      ScriptState*,
      ExecuteScriptPolicy =
          ExecuteScriptPolicy::kDoNotExecuteScriptWhenScriptsDisabled,
      V8ScriptRunner::RethrowErrorsOption =
          V8ScriptRunner::RethrowErrorsOption::DoNotRethrow());
  v8::Local<v8::Value> RunScriptAndReturnValue(
      LocalDOMWindow*,
      ExecuteScriptPolicy =
          ExecuteScriptPolicy::kDoNotExecuteScriptWhenScriptsDisabled);
  v8::Local<v8::Value> RunScriptInIsolatedWorldAndReturnValue(LocalDOMWindow*,
                                                              int32_t world_id);

 private:
  mojom::blink::ScriptType GetScriptType() const override {
    return mojom::blink::ScriptType::kClassic;
  }

  std::pair<size_t, size_t> GetClassicScriptSizes() const override;

  const ParkableString source_text_;

  // The URL of the script, which is primarily intended for DevTools
  // javascript debugger, and can be observed as:
  // 1) The 'source-file' in CSP violations reports.
  // 2) The URL(s) in javascript stack traces.
  // 3) How relative source map are resolved.
  //
  // The fragment is stripped due to https://crbug.com/306239 (except for worker
  // top-level scripts), at the callers of Create(), or inside
  // CreateFromResource() and CreateUnspecifiedScript().
  //
  // It is important to keep the url fragment for worker top-level scripts so
  // that errors in worker scripts can include the fragment when reporting the
  // location of the failure. This is enforced by several tests in
  // external/wpt/workers/interfaces/WorkerGlobalScope/onerror/.
  //
  // Note that this can be different from the script's base URL
  // (`Script::BaseURL()`, #concept-script-base-url).
  const KURL source_url_;

  const ScriptSourceLocationType source_location_type_;

  const SanitizeScriptErrors sanitize_script_errors_;

  const Member<SingleCachedMetadataHandler> cache_handler_;

  const TextPosition start_position_;

  const Member<ScriptStreamer> streamer_;
  const ScriptStreamer::NotStreamingReason not_streaming_reason_;

  const Member<ScriptCacheConsumer> cache_consumer_;

  const String source_map_url_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SCRIPT_CLASSIC_SCRIPT_H_
