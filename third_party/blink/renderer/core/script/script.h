// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SCRIPT_SCRIPT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SCRIPT_SCRIPT_H_

#include "base/optional.h"
#include "third_party/blink/public/mojom/script/script_type.mojom-blink.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/loader/fetch/script_fetch_options.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class LocalFrame;
class WorkerOrWorkletGlobalScope;

// https://html.spec.whatwg.org/C/#concept-script
class CORE_EXPORT Script : public GarbageCollected<Script> {
 public:
  virtual void Trace(Visitor* visitor) const {}

  virtual ~Script() {}

  virtual mojom::blink::ScriptType GetScriptType() const = 0;
  static base::Optional<mojom::blink::ScriptType> ParseScriptType(
      const String& script_type);

  // https://html.spec.whatwg.org/C/#run-a-classic-script
  // or
  // https://html.spec.whatwg.org/C/#run-a-module-script,
  // depending on the script type,
  // on Window or on WorkerGlobalScope, respectively.
  // RunScriptOnWorkerOrWorklet returns true if evaluated successfully.
  virtual void RunScript(LocalFrame*) = 0;
  virtual bool RunScriptOnWorkerOrWorklet(WorkerOrWorkletGlobalScope&) = 0;

  const ScriptFetchOptions& FetchOptions() const { return fetch_options_; }
  const KURL& BaseURL() const { return base_url_; }

  // Returns a pair of (script's size, cached metadata's size) only for classic
  // scripts. This is used only for metrics via
  // ServiceWorkerGlobalScopeProxy::WillEvaluateClassicScript().
  // TODO(asamidoi, hiroshige): Remove this once the metrics are no longer
  // referenced.
  virtual std::pair<size_t, size_t> GetClassicScriptSizes() const = 0;

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

#endif
