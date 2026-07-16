// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TRUSTEDTYPES_TRUSTED_PARSER_OPTIONS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TRUSTEDTYPES_TRUSTED_PARSER_OPTIONS_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/sanitizer/sanitizer.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

class CORE_EXPORT TrustedParserOptions final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit TrustedParserOptions(Sanitizer* sanitizer, bool run_scripts)
      : sanitizer_(sanitizer ? sanitizer->Clone() : nullptr),
        run_scripts_(run_scripts) {}
  void Trace(Visitor* visitor) const override {
    ScriptWrappable::Trace(visitor);
    visitor->Trace(sanitizer_);
  }

  Sanitizer* sanitizer() const {
    return sanitizer_ ? sanitizer_->Clone() : Sanitizer::CreateEmpty();
  }

  Sanitizer* EffectiveSanitizer() const { return sanitizer_; }
  bool runScripts() const { return run_scripts_; }

 private:
  Member<Sanitizer> sanitizer_;
  bool run_scripts_;
};
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TRUSTEDTYPES_TRUSTED_PARSER_OPTIONS_H_
