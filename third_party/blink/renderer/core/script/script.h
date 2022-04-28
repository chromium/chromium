// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SCRIPT_SCRIPT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SCRIPT_SCRIPT_H_

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/script/script_type.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_script_runner.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/loader/fetch/script_fetch_options.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class LocalDOMWindow;
class WorkerOrWorkletGlobalScope;

// https://html.spec.whatwg.org/C/#concept-script
class CORE_EXPORT Script : public GarbageCollected<Script> {
 public:
  virtual void Trace(Visitor* visitor) const {}

  virtual ~Script() {}

  virtual mojom::blink::ScriptType GetScriptType() const = 0;
  static absl::optional<mojom::blink::ScriptType> ParseScriptType(
      const String& script_type);

  // https://html.spec.whatwg.org/C/#run-a-classic-script
  // or
  // https://html.spec.whatwg.org/C/#run-a-module-script,
  // depending on the script type.
  // - Callers of `RunScript*AndReturnValue()` must enter a v8::HandleScope
  //   before calling.
  // - `ScriptState` == the script's modulator's `ScriptState` for modules.
  // - `ExecuteScriptPolicy::kExecuteScriptWhenScriptsDisabled` is allowed only
  //   for classic scripts with clear or historical reasons.
  //
  // On a ScriptState:
  void RunScriptOnScriptState(
      ScriptState*,
      ExecuteScriptPolicy =
          ExecuteScriptPolicy::kDoNotExecuteScriptWhenScriptsDisabled,
      V8ScriptRunner::RethrowErrorsOption =
          V8ScriptRunner::RethrowErrorsOption::DoNotRethrow());
  [[nodiscard]] virtual ScriptEvaluationResult
  RunScriptOnScriptStateAndReturnValue(
      ScriptState*,
      ExecuteScriptPolicy =
          ExecuteScriptPolicy::kDoNotExecuteScriptWhenScriptsDisabled,
      V8ScriptRunner::RethrowErrorsOption =
          V8ScriptRunner::RethrowErrorsOption::DoNotRethrow()) = 0;
  // On the main world of LocalDOMWindow:
  void RunScript(
      LocalDOMWindow*,
      ExecuteScriptPolicy =
          ExecuteScriptPolicy::kDoNotExecuteScriptWhenScriptsDisabled,
      V8ScriptRunner::RethrowErrorsOption =
          V8ScriptRunner::RethrowErrorsOption::DoNotRethrow());
  [[nodiscard]] ScriptEvaluationResult RunScriptAndReturnValue(
      LocalDOMWindow*,
      ExecuteScriptPolicy =
          ExecuteScriptPolicy::kDoNotExecuteScriptWhenScriptsDisabled,
      V8ScriptRunner::RethrowErrorsOption =
          V8ScriptRunner::RethrowErrorsOption::DoNotRethrow());
  // For worker top-level scripts and worklets.
  // Returns true if evaluated successfully.
  // TODO(crbug.com/1111134): Remove RunScriptOnWorkerOrWorklet().
  virtual bool RunScriptOnWorkerOrWorklet(WorkerOrWorkletGlobalScope&) = 0;

  const ScriptFetchOptions& FetchOptions() const { return fetch_options_; }
  const KURL& BaseURL() const { return base_url_; }

 protected:
  explicit Script(const ScriptFetchOptions& fetch_options, const KURL& base_url)
      : fetch_options_(fetch_options), base_url_(base_url) {}

 private:
  // https://html.spec.whatwg.org/C/#concept-script-script-fetch-options
  const ScriptFetchOptions fetch_options_;

  // https://html.spec.whatwg.org/C/#concept-script-base-url
  const KURL base_url_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SCRIPT_SCRIPT_H_
