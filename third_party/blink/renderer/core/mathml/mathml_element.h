// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_MATHML_MATHML_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_MATHML_MATHML_ELEMENT_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/mathml_names.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

class CORE_EXPORT MathMLElement : public Element {
  DEFINE_WRAPPERTYPEINFO();

 public:
  MathMLElement(const QualifiedName& tagName,
                Document& document,
                ConstructionType constructionType = kCreateMathMLElement);
  ~MathMLElement() override;

  bool HasTagName(const MathMLQualifiedName& name) const {
    return HasLocalName(name.LocalName());
  }

 private:
  bool IsPresentationAttribute(const QualifiedName&) const final;
  void CollectStyleForPresentationAttribute(const QualifiedName&,
                                            const AtomicString&,
                                            MutableCSSPropertyValueSet*) final;

  void ParseAttribute(const AttributeModificationParams&) final;

  bool IsMathMLElement() const =
      delete;  // This will catch anyone doing an unnecessary check.
};

DEFINE_ELEMENT_TYPE_CASTS(MathMLElement, IsMathMLElement());

template <>
struct DowncastTraits<MathMLElement> {
  static bool AllowFrom(const Node& node) { return node.IsMathMLElement(); }
};

template <typename T>
bool IsElementOfType(const MathMLElement&);
template <>
inline bool IsElementOfType<const MathMLElement>(const MathMLElement&) {
  return true;
}

inline bool Node::HasTagName(const MathMLQualifiedName& name) const {
  auto* mathml_element = DynamicTo<MathMLElement>(this);
  return mathml_element && mathml_element->HasTagName(name);
}

// This requires IsMathML*Element(const MathMLElement&).
#define DEFINE_MATHMLELEMENT_TYPE_CASTS_WITH_FUNCTION(thisType)               \
  inline bool Is##thisType(const thisType* element);                          \
  inline bool Is##thisType(const thisType& element);                          \
  inline bool Is##thisType(const MathMLElement* element) {                    \
    return element && Is##thisType(*element);                                 \
  }                                                                           \
  inline bool Is##thisType(const Node& node) {                                \
    auto* mathml_element = DynamicTo<MathMLElement>(node);                    \
    return mathml_element && Is##thisType(mathml_element);                    \
  }                                                                           \
  inline bool Is##thisType(const Node* node) {                                \
    return node && Is##thisType(*node);                                       \
  }                                                                           \
  template <typename T>                                                       \
  inline bool Is##thisType(const Member<T>& node) {                           \
    return Is##thisType(node.Get());                                          \
  }                                                                           \
  template <>                                                                 \
  inline bool IsElementOfType<const thisType>(const MathMLElement& element) { \
    return Is##thisType(element);                                             \
  }                                                                           \
  DEFINE_ELEMENT_TYPE_CASTS_WITH_FUNCTION(thisType)

}  // namespace blink

#include "third_party/blink/renderer/core/mathml_element_type_helpers.h"

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_MATHML_MATHML_ELEMENT_H_
