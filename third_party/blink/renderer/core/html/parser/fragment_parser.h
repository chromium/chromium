// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PARSER_FRAGMENT_PARSER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PARSER_FRAGMENT_PARSER_H_

#include "third_party/blink/renderer/core/sanitizer/sanitizer.h"

namespace blink {

class AtomicString;
class ContainerNode;
class CustomElementRegistry;
class DocumentFragment;
class Element;
class ExceptionState;
class FragmentParserConfig;
class FragmentParserOptions;
class ParseHTMLUnsafeOptions;
class SetHTMLOptions;
class SetHTMLUnsafeOptions;
class String;
class TrustedParserOptions;
class V8UnionSanitizerOrSanitizerConfigOrSanitizerPresets;
class V8UnionSetHTMLUnsafeOptionsOrTrustedParserOptions;

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

  static FragmentParserConfig ForContainer(ContainerNode* context,
                                           Sanitizer::Mode mode,
                                           const AtomicString& interface_name,
                                           const AtomicString& property_name);

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
  explicit FragmentParserOptions(TrustedParserOptions* options);
  explicit FragmentParserOptions(SetHTMLUnsafeOptions* options);
  explicit FragmentParserOptions(ParseHTMLUnsafeOptions* options);
  explicit FragmentParserOptions(SetHTMLOptions* options);

  static FragmentParserOptions From(
      const V8UnionSetHTMLUnsafeOptionsOrTrustedParserOptions* options);

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

DocumentFragment* ParseHTMLFragment(const String& html,
                                    const FragmentParserConfig& config,
                                    FragmentParserOptions options,
                                    ExceptionState& exception_state);

DocumentFragment* CreateContextualFragment(const String& html,
                                           Element*,
                                           ExceptionState&);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PARSER_FRAGMENT_PARSER_H_
