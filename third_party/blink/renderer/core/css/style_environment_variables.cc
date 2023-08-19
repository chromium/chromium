// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/style_environment_variables.h"

#include "third_party/blink/renderer/core/css/parser/css_tokenizer.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
namespace blink {

namespace {

// This is the default value for all safe-area-inset-* variables.
static const char kSafeAreaInsetDefault[] = "0px";
// This is the default value for all keyboard-inset-* variables.
static const char kKeyboardInsetDefault[] = "0px";

// Use this to set default values for environment variables when the root
// instance is created.
void SetDefaultEnvironmentVariables(StyleEnvironmentVariables* instance) {
  instance->SetVariable(UADefinedVariable::kSafeAreaInsetTop,
                        kSafeAreaInsetDefault);
  instance->SetVariable(UADefinedVariable::kSafeAreaInsetLeft,
                        kSafeAreaInsetDefault);
  instance->SetVariable(UADefinedVariable::kSafeAreaInsetBottom,
                        kSafeAreaInsetDefault);
  instance->SetVariable(UADefinedVariable::kSafeAreaInsetRight,
                        kSafeAreaInsetDefault);
  instance->SetVariable(UADefinedVariable::kKeyboardInsetTop,
                        kKeyboardInsetDefault);
  instance->SetVariable(UADefinedVariable::kKeyboardInsetLeft,
                        kKeyboardInsetDefault);
  instance->SetVariable(UADefinedVariable::kKeyboardInsetBottom,
                        kKeyboardInsetDefault);
  instance->SetVariable(UADefinedVariable::kKeyboardInsetRight,
                        kKeyboardInsetDefault);
  instance->SetVariable(UADefinedVariable::kKeyboardInsetWidth,
                        kKeyboardInsetDefault);
  instance->SetVariable(UADefinedVariable::kKeyboardInsetHeight,
                        kKeyboardInsetDefault);
}

}  // namespace.

// This owns the static root instance.
class StyleEnvironmentVariables::RootOwner {
 public:
  StyleEnvironmentVariables& GetRoot() {
    if (!instance_) {
      instance_ = base::AdoptRef(new StyleEnvironmentVariables());
      SetDefaultEnvironmentVariables(instance_.get());
    }

    return *instance_.get();
  }

 private:
  scoped_refptr<StyleEnvironmentVariables> instance_;
};

// static
StyleEnvironmentVariables& StyleEnvironmentVariables::GetRootInstance() {
  static auto* instance = new StyleEnvironmentVariables::RootOwner();
  return instance->GetRoot();
}

// static
const AtomicString StyleEnvironmentVariables::GetVariableName(
    UADefinedVariable variable,
    const FeatureContext* feature_context) {
  switch (variable) {
    case UADefinedVariable::kSafeAreaInsetTop:
      return AtomicString("safe-area-inset-top");
    case UADefinedVariable::kSafeAreaInsetLeft:
      return AtomicString("safe-area-inset-left");
    case UADefinedVariable::kSafeAreaInsetBottom:
      return AtomicString("safe-area-inset-bottom");
    case UADefinedVariable::kSafeAreaInsetRight:
      return AtomicString("safe-area-inset-right");
    case UADefinedVariable::kKeyboardInsetTop:
      return AtomicString("keyboard-inset-top");
    case UADefinedVariable::kKeyboardInsetLeft:
      return AtomicString("keyboard-inset-left");
    case UADefinedVariable::kKeyboardInsetBottom:
      return AtomicString("keyboard-inset-bottom");
    case UADefinedVariable::kKeyboardInsetRight:
      return AtomicString("keyboard-inset-right");
    case UADefinedVariable::kKeyboardInsetWidth:
      return AtomicString("keyboard-inset-width");
    case UADefinedVariable::kKeyboardInsetHeight:
      return AtomicString("keyboard-inset-height");
    case UADefinedVariable::kTitlebarAreaX:
      DCHECK(RuntimeEnabledFeatures::WebAppWindowControlsOverlayEnabled(
          feature_context));
      return AtomicString("titlebar-area-x");
    case UADefinedVariable::kTitlebarAreaY:
      DCHECK(RuntimeEnabledFeatures::WebAppWindowControlsOverlayEnabled(
          feature_context));
      return AtomicString("titlebar-area-y");
    case UADefinedVariable::kTitlebarAreaWidth:
      DCHECK(RuntimeEnabledFeatures::WebAppWindowControlsOverlayEnabled(
          feature_context));
      return AtomicString("titlebar-area-width");
    case UADefinedVariable::kTitlebarAreaHeight:
      DCHECK(RuntimeEnabledFeatures::WebAppWindowControlsOverlayEnabled(
          feature_context));
      return AtomicString("titlebar-area-height");
    default:
      break;
  }

  NOTREACHED();
}

const AtomicString StyleEnvironmentVariables::GetVariableName(
    UADefinedTwoDimensionalVariable variable,
    const FeatureContext* feature_context) {
  switch (variable) {
    case UADefinedTwoDimensionalVariable::kViewportSegmentTop:
      DCHECK(RuntimeEnabledFeatures::ViewportSegmentsEnabled());
      return AtomicString("viewport-segment-top");
    case UADefinedTwoDimensionalVariable::kViewportSegmentRight:
      DCHECK(RuntimeEnabledFeatures::ViewportSegmentsEnabled());
      return AtomicString("viewport-segment-right");
    case UADefinedTwoDimensionalVariable::kViewportSegmentBottom:
      DCHECK(RuntimeEnabledFeatures::ViewportSegmentsEnabled());
      return AtomicString("viewport-segment-bottom");
    case UADefinedTwoDimensionalVariable::kViewportSegmentLeft:
      DCHECK(RuntimeEnabledFeatures::ViewportSegmentsEnabled());
      return AtomicString("viewport-segment-left");
    case UADefinedTwoDimensionalVariable::kViewportSegmentWidth:
      DCHECK(RuntimeEnabledFeatures::ViewportSegmentsEnabled());
      return AtomicString("viewport-segment-width");
    case UADefinedTwoDimensionalVariable::kViewportSegmentHeight:
      DCHECK(RuntimeEnabledFeatures::ViewportSegmentsEnabled());
      return AtomicString("viewport-segment-height");
    default:
      break;
  }

  NOTREACHED();
}

// static
scoped_refptr<StyleEnvironmentVariables> StyleEnvironmentVariables::Create(
    StyleEnvironmentVariables& parent) {
  scoped_refptr<StyleEnvironmentVariables> obj =
      base::AdoptRef(new StyleEnvironmentVariables());

  // Add a reference to this instance from the parent.
  obj->BindToParent(parent);

  return obj;
}

StyleEnvironmentVariables::~StyleEnvironmentVariables() {
  // Remove a reference to this instance from the parent.
  if (parent_) {
    auto it = parent_->children_.Find(this);
    DCHECK(it != kNotFound);
    parent_->children_.EraseAt(it);
  }
}

void StyleEnvironmentVariables::SetVariable(const AtomicString& name,
                                            const String& value) {
  scoped_refptr<CSSVariableData> variable_data =
      CSSVariableData::Create(value, false /* is_animation_tainted */,
                              false /* needs_variable_resolution */);
  data_.Set(name, std::move(variable_data));
  InvalidateVariable(name);
}

void StyleEnvironmentVariables::SetVariable(const AtomicString& name,
                                            unsigned first_dimension,
                                            unsigned second_dimension,
                                            const String& value) {
  base::CheckedNumeric<unsigned> first_dimension_size = first_dimension;
  ++first_dimension_size;
  if (!first_dimension_size.IsValid()) {
    return;
  }

  base::CheckedNumeric<unsigned> second_dimension_size = second_dimension;
  ++second_dimension_size;
  if (!second_dimension_size.IsValid()) {
    return;
  }

  scoped_refptr<CSSVariableData> variable_data =
      CSSVariableData::Create(value, false /* is_animation_tainted */,
                              false /* needs_variable_resolution */);

  TwoDimensionVariableValues* values_to_set = nullptr;
  auto it = two_dimension_data_.find(name);
  if (it == two_dimension_data_.end()) {
    auto result = two_dimension_data_.Set(name, TwoDimensionVariableValues());
    values_to_set = &result.stored_value->value;
  } else {
    values_to_set = &it->value;
  }

  if (first_dimension_size.ValueOrDie() > values_to_set->size()) {
    values_to_set->Grow(first_dimension_size.ValueOrDie());
  }

  if (second_dimension_size.ValueOrDie() >
      (*values_to_set)[first_dimension].size()) {
    (*values_to_set)[first_dimension].Grow(second_dimension_size.ValueOrDie());
  }

  (*values_to_set)[first_dimension][second_dimension] = variable_data;
  InvalidateVariable(name);
}

void StyleEnvironmentVariables::SetVariable(UADefinedVariable variable,
                                            const String& value) {
  SetVariable(GetVariableName(variable, GetFeatureContext()), value);
}

void StyleEnvironmentVariables::SetVariable(
    UADefinedTwoDimensionalVariable variable,
    unsigned first_dimension,
    unsigned second_dimension,
    const String& value) {
  SetVariable(GetVariableName(variable, GetFeatureContext()), first_dimension,
              second_dimension, value);
}

void StyleEnvironmentVariables::RemoveVariable(UADefinedVariable variable) {
  const AtomicString name = GetVariableName(variable, GetFeatureContext());
  RemoveVariable(name);
}

void StyleEnvironmentVariables::RemoveVariable(
    UADefinedTwoDimensionalVariable variable) {
  const AtomicString name = GetVariableName(variable, GetFeatureContext());
  RemoveVariable(name);
}

void StyleEnvironmentVariables::RemoveVariable(const AtomicString& name) {
  data_.erase(name);
  two_dimension_data_.erase(name);
  InvalidateVariable(name);
}

CSSVariableData* StyleEnvironmentVariables::ResolveVariable(
    const AtomicString& name,
    WTF::Vector<unsigned> indices) {
  if (indices.size() == 0u) {
    auto result = data_.find(name);
    if (result == data_.end() && parent_) {
      return parent_->ResolveVariable(name, std::move(indices));
    }
    if (result == data_.end()) {
      return nullptr;
    }
    return result->value.get();
  } else if (indices.size() == 2u) {
    auto result = two_dimension_data_.find(name);
    if (result == two_dimension_data_.end() && parent_) {
      return parent_->ResolveVariable(name, std::move(indices));
    }

    unsigned first_dimension = indices[0];
    unsigned second_dimension = indices[1];
    if (result == two_dimension_data_.end()) {
      return nullptr;
    }
    if (first_dimension >= result->value.size() ||
        second_dimension >= result->value[first_dimension].size()) {
      return nullptr;
    }
    return result->value[first_dimension][second_dimension].get();
  }

  return nullptr;
}

void StyleEnvironmentVariables::DetachFromParent() {
  DCHECK(parent_);

  // Remove any reference the |parent| has to |this|.
  auto it = parent_->children_.Find(this);
  if (it != kNotFound) {
    parent_->children_.EraseAt(it);
  }

  parent_ = nullptr;
}

String StyleEnvironmentVariables::FormatPx(int value) {
  return String::Format("%dpx", value);
}

const FeatureContext* StyleEnvironmentVariables::GetFeatureContext() const {
  return nullptr;
}

void StyleEnvironmentVariables::ClearForTesting() {
  data_.clear();

  // If we are the root then we should re-apply the default variables.
  if (!parent_) {
    SetDefaultEnvironmentVariables(this);
  }
}

void StyleEnvironmentVariables::BindToParent(
    StyleEnvironmentVariables& parent) {
  DCHECK_EQ(nullptr, parent_);
  parent_ = &parent;
  parent.children_.push_back(this);
}

void StyleEnvironmentVariables::ParentInvalidatedVariable(
    const AtomicString& name) {
  // If we have not overridden the variable then we should invalidate it
  // locally.
  if (data_.find(name) == data_.end() &&
      two_dimension_data_.find(name) == two_dimension_data_.end()) {
    InvalidateVariable(name);
  }
}

void StyleEnvironmentVariables::InvalidateVariable(const AtomicString& name) {
  for (auto& it : children_) {
    it->ParentInvalidatedVariable(name);
  }
}

}  // namespace blink
