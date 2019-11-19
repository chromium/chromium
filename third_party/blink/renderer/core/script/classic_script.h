// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SCRIPT_CLASSIC_SCRIPT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SCRIPT_CLASSIC_SCRIPT_H_

#include "third_party/blink/renderer/bindings/core/v8/sanitize_script_errors.h"
#include "third_party/blink/renderer/bindings/core/v8/script_source_code.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/loader/resource/script_resource.h"
#include "third_party/blink/renderer/core/script/script.h"
#include "third_party/blink/renderer/platform/loader/fetch/script_fetch_options.h"

namespace blink {

class CORE_EXPORT ClassicScript final : public Script {
 public:
  ClassicScript(const ScriptSourceCode& script_source_code,
                const KURL& base_url,
                const ScriptFetchOptions& fetch_options,
                SanitizeScriptErrors sanitize_script_errors)
      : Script(fetch_options, base_url),
        script_source_code_(script_source_code),
        sanitize_script_errors_(sanitize_script_errors) {}

  void Trace(Visitor*) override;

  const ScriptSourceCode& GetScriptSourceCode() const {
    return script_source_code_;
  }

 private:
  mojom::ScriptType GetScriptType() const override {
    return mojom::ScriptType::kClassic;
  }
  void RunScript(LocalFrame*, const SecurityOrigin*) override;
  void RunScriptOnWorker(WorkerGlobalScope&) override;

  const ScriptSourceCode script_source_code_;
  const SanitizeScriptErrors sanitize_script_errors_;
};

}  // namespace blink

#endif
