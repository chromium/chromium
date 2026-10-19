// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_MATHML_MATHML_SCRIPTS_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_MATHML_MATHML_SCRIPTS_ELEMENT_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/mathml/mathml_element.h"

namespace blink {

class Document;

enum class MathScriptType {
  kSub,
  kSuper,
  kSubSup,
  kMultiscripts,
  kUnder,
  kOver,
  kUnderOver
};

class CORE_EXPORT MathMLScriptsElement : public MathMLElement {
 public:
  MathScriptType GetScriptType() const { return script_type_; }

  MathMLScriptsElement(const QualifiedName& tagName, Document& document);
  ElementType GetElementType() const override {
    return ElementType::kMathMLScriptsElement;
  }

 private:
  const MathScriptType script_type_;
};

template <>
struct DowncastTraits<MathMLScriptsElement> {
  static bool AllowFrom(const Node& node) {
    const ElementType type = node.GetElementType();
    return type == ElementType::kMathMLScriptsElement ||
           type == ElementType::kMathMLUnderOverElement;
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_MATHML_MATHML_SCRIPTS_ELEMENT_H_
