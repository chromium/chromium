// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SCRIPT_SCRIPT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SCRIPT_SCRIPT_H_

#include "third_party/blink/public/mojom/script/script_type.mojom-blink.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/loader/fetch/script_fetch_options.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class LocalFrame;
class SecurityOrigin;
class WorkerGlobalScope;

// https://html.spec.whatwg.org/C/#concept-script
class CORE_EXPORT Script : public GarbageCollected<Script> {
 public:
  virtual void Trace(Visitor* visitor) {}

  virtual ~Script() {}

  virtual mojom::ScriptType GetScriptType() const = 0;

  // https://html.spec.whatwg.org/C/#run-a-classic-script
  // or
  // https://html.spec.whatwg.org/C/#run-a-module-script,
  // depending on the script type,
  // on Window or on WorkerGlobalScope, respectively.
  virtual void RunScript(LocalFrame*, const SecurityOrigin*) = 0;
  virtual void RunScriptOnWorker(WorkerGlobalScope&) = 0;

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

#endif
