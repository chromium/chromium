// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TRUSTEDTYPES_TRUSTED_PARSER_OPTIONS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TRUSTEDTYPES_TRUSTED_PARSER_OPTIONS_H_

#include "third_party/blink/renderer/bindings/core/v8/v8_union_sanitizer_sanitizerconfig_sanitizerpresets.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

class V8UnionSanitizerOrSanitizerConfigOrSanitizerPresets;

class CORE_EXPORT TrustedParserOptions final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit TrustedParserOptions(
      V8UnionSanitizerOrSanitizerConfigOrSanitizerPresets* sanitizer,
      bool run_scripts)
      : sanitizer_(sanitizer), run_scripts_(run_scripts) {}
  void Trace(Visitor* visitor) const override {
    ScriptWrappable::Trace(visitor);
    visitor->Trace(sanitizer_);
  }

  V8UnionSanitizerOrSanitizerConfigOrSanitizerPresets* sanitizer() const {
    return sanitizer_;
  }
  bool runScripts() const { return run_scripts_; }

 private:
  Member<V8UnionSanitizerOrSanitizerConfigOrSanitizerPresets> sanitizer_;
  bool run_scripts_;
};
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TRUSTEDTYPES_TRUSTED_PARSER_OPTIONS_H_
