// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PARSER_FRAGMENT_PARSER_OPTIONS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PARSER_FRAGMENT_PARSER_OPTIONS_H_

#include "base/memory/stack_allocated.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_set_html_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_set_html_unsafe_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_sethtmloptions_trustedparseroptions.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_sethtmlunsafeoptions_trustedparseroptions.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/container_node.h"
#include "third_party/blink/renderer/core/sanitizer/sanitizer.h"
#include "third_party/blink/renderer/core/trustedtypes/trusted_parser_options.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

class CustomElementRegistry;
class V8UnionSanitizerOrSanitizerConfigOrSanitizerPresets;

class CORE_EXPORT FragmentParserConfig {
  STACK_ALLOCATED();

 public:
  enum class ParseDeclarativeShadowRoots {
    kDontParse = 0,
    kParse = 1,
  };
  // ForceHtml specifies whether the HTML parser should be used when parsing
  // markup even if we are in an XML document.
  enum class ForceHtml {
    kDontForce = 0,
    kForce = 1,
  };

  Sanitizer::Mode sanitizer_mode = Sanitizer::Mode::kUnsafe;
  ParseDeclarativeShadowRoots parse_declarative_shadows =
      ParseDeclarativeShadowRoots::kDontParse;
  ForceHtml force_html = ForceHtml::kDontForce;
  AtomicString interface_name = g_empty_atom;
  AtomicString property_name = g_empty_atom;
  Element* context_element = nullptr;
  CustomElementRegistry* registry = nullptr;
};

class CORE_EXPORT FragmentParserOptions {
  STACK_ALLOCATED();

 public:
  enum class RunScripts { kRunScripts, kDontRunScripts };
  enum class TrustMode { kTrusted, kUntrusted };

  FragmentParserOptions() = default;
  explicit FragmentParserOptions(RunScripts run_scripts)
      : run_scripts_(run_scripts) {}
  FragmentParserOptions(const FragmentParserOptions&) = default;
  FragmentParserOptions& operator=(const FragmentParserOptions&) = default;
  explicit FragmentParserOptions(TrustedParserOptions* options)
      : trust_mode_(TrustMode::kTrusted),
        run_scripts_(options->runScripts() ? RunScripts::kRunScripts
                                           : RunScripts::kDontRunScripts),
        sanitizer_init_(options->sanitizer()) {}

  explicit FragmentParserOptions(SetHTMLUnsafeOptions* options)
      : run_scripts_(options->runScripts() ? RunScripts::kRunScripts
                                           : RunScripts::kDontRunScripts),
        sanitizer_init_(options->sanitizer()) {}

  explicit FragmentParserOptions(SetHTMLOptions* options)
      : sanitizer_init_(options->sanitizer()) {}

  static FragmentParserOptions From(
      const V8UnionSetHTMLUnsafeOptionsOrTrustedParserOptions* options) {
    switch (options->GetContentType()) {
      case V8UnionSetHTMLUnsafeOptionsOrTrustedParserOptions::ContentType::
          kSetHTMLUnsafeOptions:
        return FragmentParserOptions(options->GetAsSetHTMLUnsafeOptions());
      case V8UnionSetHTMLUnsafeOptionsOrTrustedParserOptions::ContentType::
          kTrustedParserOptions:
        return FragmentParserOptions(options->GetAsTrustedParserOptions());
    }
  }

  static FragmentParserOptions From(
      const V8UnionSetHTMLOptionsOrTrustedParserOptions* options) {
    switch (options->GetContentType()) {
      case V8UnionSetHTMLOptionsOrTrustedParserOptions::ContentType::
          kSetHTMLOptions:
        return FragmentParserOptions(options->GetAsSetHTMLOptions());
      case V8UnionSetHTMLOptionsOrTrustedParserOptions::ContentType::
          kTrustedParserOptions:
        return FragmentParserOptions(options->GetAsTrustedParserOptions());
    }
  }

  TrustMode trust_mode() const { return trust_mode_; }
  RunScripts run_scripts() const { return run_scripts_; }
  V8UnionSanitizerOrSanitizerConfigOrSanitizerPresets* sanitizer_init() const {
    return sanitizer_init_;
  }

 private:
  TrustMode trust_mode_ = TrustMode::kUntrusted;
  RunScripts run_scripts_ = RunScripts::kDontRunScripts;
  V8UnionSanitizerOrSanitizerConfigOrSanitizerPresets* sanitizer_init_ =
      nullptr;
};

}  // namespace blink
#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PARSER_FRAGMENT_PARSER_OPTIONS_H_
