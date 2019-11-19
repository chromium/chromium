// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/document_style_environment_variables.h"

#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hasher.h"

namespace blink {

// static
unsigned DocumentStyleEnvironmentVariables::GenerateHashFromName(
    const AtomicString& name) {
  if (name.Is8Bit())
    return StringHasher::ComputeHash(name.Characters8(), name.length());
  return StringHasher::ComputeHash(name.Characters16(), name.length());
}

// static
scoped_refptr<DocumentStyleEnvironmentVariables>
DocumentStyleEnvironmentVariables::Create(StyleEnvironmentVariables& parent,
                                          Document& document) {
  scoped_refptr<DocumentStyleEnvironmentVariables> obj =
      base::AdoptRef(new DocumentStyleEnvironmentVariables(document));

  // Add a reference to this instance from the root.
  obj->BindToParent(parent);

  return obj;
}

CSSVariableData* DocumentStyleEnvironmentVariables::ResolveVariable(
    const AtomicString& name,
    bool record_metrics) {
  unsigned id = GenerateHashFromName(name);
  if (record_metrics)
    RecordVariableUsage(id);

  // Mark the variable as seen so we will invalidate the style if we change it.
  seen_variables_.insert(id);
  return StyleEnvironmentVariables::ResolveVariable(name);
}

CSSVariableData* DocumentStyleEnvironmentVariables::ResolveVariable(
    const AtomicString& name) {
  return ResolveVariable(name, true /* record_metrics */);
}

void DocumentStyleEnvironmentVariables::InvalidateVariable(
    const AtomicString& name) {
  DCHECK(document_);

  // Invalidate the document if we have seen this variable on this document.
  if (seen_variables_.Contains(GenerateHashFromName(name)))
    document_->GetStyleEngine().EnvironmentVariableChanged();

  StyleEnvironmentVariables::InvalidateVariable(name);
}

DocumentStyleEnvironmentVariables::DocumentStyleEnvironmentVariables(
    Document& document)
    : document_(&document) {}

void DocumentStyleEnvironmentVariables::RecordVariableUsage(unsigned id) {
  UseCounter::Count(document_, WebFeature::kCSSEnvironmentVariable);

  // See the unittest DISABLED_PrintExpectedVariableNameHashes() for how these
  // values are computed.
  switch (id) {
    case 0x3eb492df:
      UseCounter::Count(document_,
                        WebFeature::kCSSEnvironmentVariable_SafeAreaInsetTop);
      break;
    case 0xe0994c83:
      UseCounter::Count(document_,
                        WebFeature::kCSSEnvironmentVariable_SafeAreaInsetLeft);
      break;
    case 0x898873a2:
      UseCounter::Count(
          document_, WebFeature::kCSSEnvironmentVariable_SafeAreaInsetBottom);
      break;
    case 0xd99fe75b:
      UseCounter::Count(document_,
                        WebFeature::kCSSEnvironmentVariable_SafeAreaInsetRight);
      break;
    default:
      // Do nothing if this is an unknown variable.
      break;
  }
}

}  // namespace blink
