// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/cssom/css_transform_value.h"

#include "base/ranges/algorithm.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/cssom/css_transform_component.h"
#include "third_party/blink/renderer/core/geometry/dom_matrix.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

CSSTransformValue* CSSTransformValue::Create(
    const HeapVector<Member<CSSTransformComponent>>& transform_components,
    ExceptionState& exception_state) {
  CSSTransformValue* value = Create(transform_components);
  if (!value) {
    exception_state.ThrowTypeError(
        "CSSTransformValue must have at least one component");
    return nullptr;
  }
  return value;
}

CSSTransformValue* CSSTransformValue::Create(
    const HeapVector<Member<CSSTransformComponent>>& transform_components) {
  if (transform_components.empty()) {
    return nullptr;
  }
  return MakeGarbageCollected<CSSTransformValue>(transform_components);
}

CSSTransformValue* CSSTransformValue::FromCSSValue(const CSSValue& css_value) {
  auto* css_value_list = DynamicTo<CSSValueList>(css_value);
  if (!css_value_list) {
    // TODO(meade): Also need to check the separator here if we care.
    return nullptr;
  }
  HeapVector<Member<CSSTransformComponent>> components;
  for (const CSSValue* value : *css_value_list) {
    CSSTransformComponent* component =
        CSSTransformComponent::FromCSSValue(*value);
    if (!component) {
      return nullptr;
    }
    components.push_back(component);
  }
  return CSSTransformValue::Create(components);
}

bool CSSTransformValue::is2D() const {
  return base::ranges::all_of(transform_components_, [](const auto& component) {
    return component->is2D();
  });
}

DOMMatrix* CSSTransformValue::toMatrix(ExceptionState& exception_state) const {
  DOMMatrix* matrix = DOMMatrix::Create();
  for (wtf_size_t i = 0; i < transform_components_.size(); i++) {
    const DOMMatrix* matrixComponent =
        transform_components_[i]->toMatrix(exception_state);
    if (matrixComponent) {
      matrix->multiplySelf(*matrixComponent);
    }
  }
  return matrix;
}

const CSSValue* CSSTransformValue::ToCSSValue() const {
  CSSValueList* transform_css_value = CSSValueList::CreateSpaceSeparated();
  for (wtf_size_t i = 0; i < transform_components_.size(); i++) {
    const CSSValue* component = transform_components_[i]->ToCSSValue();
    // TODO(meade): Remove this check once numbers and lengths are rewritten.
    if (!component) {
      return nullptr;
    }
    transform_css_value->Append(*component);
  }
  return transform_css_value;
}

IndexedPropertySetterResult CSSTransformValue::AnonymousIndexedSetter(
    unsigned index,
    const Member<CSSTransformComponent> component,
    ExceptionState& exception_state) {
  if (index < transform_components_.size()) {
    transform_components_[index] = component;
    return IndexedPropertySetterResult::kIntercepted;
  }

  if (index == transform_components_.size()) {
    transform_components_.push_back(component);
    return IndexedPropertySetterResult::kIntercepted;
  }

  exception_state.ThrowRangeError(
      ExceptionMessages::IndexOutsideRange<unsigned>(
          "index", index, 0, ExceptionMessages::kInclusiveBound,
          transform_components_.size(), ExceptionMessages::kInclusiveBound));
  return IndexedPropertySetterResult::kIntercepted;
}

}  // namespace blink
