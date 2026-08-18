// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file

#include "third_party/blink/renderer/core/sanitizer/sanitizer_builtins.h"

#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/mathml_names.h"
#include "third_party/blink/renderer/core/sanitizer/sanitizer.h"
#include "third_party/blink/renderer/core/svg_names.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

namespace blink {

namespace {

const SanitizerNameSet BuildNonReplaceableElements() {
  // https://html.spec.whatwg.org/#built-in-non-replaceable-elements-list
  return SanitizerNameSet{
      html_names::kHTMLTag,
      svg_names::kSVGTag,
      mathml_names::kMathTag,
  };
}

}  // anonymous namespace

namespace sanitizer_generated_builtins {

std::unique_ptr<SanitizerNameSet> MakeNameSet(
    base::span<const QualifiedName* const> names) {
  auto set = std::make_unique<SanitizerNameSet>();
  for (const QualifiedName* name : names) {
    set->insert(*name);
  }
  return set;
}

SanitizerNameMap MakeNameMap(base::span<const ElementAttrs> entries) {
  SanitizerNameMap map;
  for (const ElementAttrs& entry : entries) {
    SanitizerNameSet attrs;
    for (const QualifiedName* attr : entry.attrs) {
      attrs.insert(*attr);
    }
    map.insert(*entry.element, std::move(attrs));
  }
  return map;
}

}  // namespace sanitizer_generated_builtins

const Sanitizer* SanitizerBuiltins::GetDefaultUnsafe() {
  DEFINE_STATIC_LOCAL(Persistent<Sanitizer>, default_unsafe_,
                      (Sanitizer::CreateEmpty()));
  return default_unsafe_.Get();
}

const Sanitizer* SanitizerBuiltins::GetDefaultSafe() {
  DEFINE_STATIC_LOCAL(
      Persistent<Sanitizer>, default_safe_,
      (blink::sanitizer_generated_builtins::BuildDefaultConfig()));
  return default_safe_.Get();
}

const Sanitizer* SanitizerBuiltins::GetBaseline() {
  DEFINE_STATIC_LOCAL(
      Persistent<Sanitizer>, baseline_,
      (blink::sanitizer_generated_builtins::BuildBaselineConfig()));
  return baseline_.Get();
}

const SanitizerNameSet* SanitizerBuiltins::GetNonReplaceableElements() {
  DEFINE_STATIC_LOCAL(SanitizerNameSet, non_replaceable_elements_,
                      (BuildNonReplaceableElements()));
  return &non_replaceable_elements_;
}

}  // namespace blink
