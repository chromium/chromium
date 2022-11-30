// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/mathml/mathml_scripts_element.h"

namespace blink {

static MathScriptType ScriptTypeOf(const QualifiedName& tagName) {
  if (tagName == mathml_names::kMsubTag)
    return MathScriptType::kSub;
  if (tagName == mathml_names::kMsupTag)
    return MathScriptType::kSuper;
  if (tagName == mathml_names::kMsubsupTag)
    return MathScriptType::kSubSup;
  if (tagName == mathml_names::kMunderTag)
    return MathScriptType::kUnder;
  if (tagName == mathml_names::kMoverTag)
    return MathScriptType::kOver;
  if (tagName == mathml_names::kMunderoverTag)
    return MathScriptType::kUnderOver;
  DCHECK_EQ(tagName, mathml_names::kMmultiscriptsTag);
  return MathScriptType::kMultiscripts;
}

MathMLScriptsElement::MathMLScriptsElement(const QualifiedName& tagName,
                                           Document& document)
    : MathMLElement(tagName, document), script_type_(ScriptTypeOf(tagName)) {}

}  // namespace blink
