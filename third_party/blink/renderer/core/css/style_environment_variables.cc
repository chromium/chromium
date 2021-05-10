// Copyright 2018 The Chromium Authors. All rights reserved.
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
  if (RuntimeEnabledFeatures::VirtualKeyboardEnabled()) {
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
      return "safe-area-inset-top";
    case UADefinedVariable::kSafeAreaInsetLeft:
      return "safe-area-inset-left";
    case UADefinedVariable::kSafeAreaInsetBottom:
      return "safe-area-inset-bottom";
    case UADefinedVariable::kSafeAreaInsetRight:
      return "safe-area-inset-right";
    case UADefinedVariable::kKeyboardInsetTop:
      DCHECK(RuntimeEnabledFeatures::VirtualKeyboardEnabled());
      return "keyboard-inset-top";
    case UADefinedVariable::kKeyboardInsetLeft:
      DCHECK(RuntimeEnabledFeatures::VirtualKeyboardEnabled());
      return "keyboard-inset-left";
    case UADefinedVariable::kKeyboardInsetBottom:
      DCHECK(RuntimeEnabledFeatures::VirtualKeyboardEnabled());
      return "keyboard-inset-bottom";
    case UADefinedVariable::kKeyboardInsetRight:
      DCHECK(RuntimeEnabledFeatures::VirtualKeyboardEnabled());
      return "keyboard-inset-right";
    case UADefinedVariable::kKeyboardInsetWidth:
      DCHECK(RuntimeEnabledFeatures::VirtualKeyboardEnabled());
      return "keyboard-inset-width";
    case UADefinedVariable::kKeyboardInsetHeight:
      DCHECK(RuntimeEnabledFeatures::VirtualKeyboardEnabled());
      return "keyboard-inset-height";
    case UADefinedVariable::kFoldTop:
      DCHECK(RuntimeEnabledFeatures::CSSFoldablesEnabled());
      return "fold-top";
    case UADefinedVariable::kFoldRight:
      DCHECK(RuntimeEnabledFeatures::CSSFoldablesEnabled());
      return "fold-right";
    case UADefinedVariable::kFoldBottom:
      DCHECK(RuntimeEnabledFeatures::CSSFoldablesEnabled());
      return "fold-bottom";
    case UADefinedVariable::kFoldLeft:
      DCHECK(RuntimeEnabledFeatures::CSSFoldablesEnabled());
      return "fold-left";
    case UADefinedVariable::kFoldWidth:
      DCHECK(RuntimeEnabledFeatures::CSSFoldablesEnabled());
      return "fold-width";
    case UADefinedVariable::kFoldHeight:
      DCHECK(RuntimeEnabledFeatures::CSSFoldablesEnabled());
      return "fold-height";
    case UADefinedVariable::kTitlebarAreaX:
      DCHECK(RuntimeEnabledFeatures::WebAppWindowControlsOverlayEnabled(
          feature_context));
      return "titlebar-area-x";
    case UADefinedVariable::kTitlebarAreaY:
      DCHECK(RuntimeEnabledFeatures::WebAppWindowControlsOverlayEnabled(
          feature_context));
      return "titlebar-area-y";
    case UADefinedVariable::kTitlebarAreaWidth:
      DCHECK(RuntimeEnabledFeatures::WebAppWindowControlsOverlayEnabled(
          feature_context));
      return "titlebar-area-width";
    case UADefinedVariable::kTitlebarAreaHeight:
      DCHECK(RuntimeEnabledFeatures::WebAppWindowControlsOverlayEnabled(
          feature_context));
      return "titlebar-area-height";
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

void StyleEnvironmentVariables::SetVariable(
    const AtomicString& name,
    scoped_refptr<CSSVariableData> value) {
  data_.Set(name, std::move(value));
  InvalidateVariable(name);
}

void StyleEnvironmentVariables::SetVariable(const AtomicString& name,
                                            const String& value) {
  CSSTokenizer tokenizer(value);
  Vector<CSSParserToken> tokens;
  tokens.AppendVector(tokenizer.TokenizeToEOF());

  Vector<String> backing_strings;
  backing_strings.push_back(value);

  SetVariable(
      name,
      CSSVariableData::CreateResolved(
          std::move(tokens), std::move(backing_strings),
          false /* is_animation_tainted */, false /* has_font_units */,
          false /* has_root_font_units*/, g_null_atom, WTF::TextEncoding()));
}

void StyleEnvironmentVariables::SetVariable(const UADefinedVariable name,
                                            const String& value) {
  SetVariable(GetVariableName(name, GetFeatureContext()), value);
}

void StyleEnvironmentVariables::RemoveVariable(const AtomicString& name) {
  data_.erase(name);
  InvalidateVariable(name);
}

CSSVariableData* StyleEnvironmentVariables::ResolveVariable(
    const AtomicString& name) {
  auto result = data_.find(name);
  if (result == data_.end() && parent_)
    return parent_->ResolveVariable(name);
  if (result == data_.end())
    return nullptr;
  return result->value.get();
}

void StyleEnvironmentVariables::DetachFromParent() {
  DCHECK(parent_);

  // Remove any reference the |parent| has to |this|.
  auto it = parent_->children_.Find(this);
  if (it != kNotFound)
    parent_->children_.EraseAt(it);

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
  if (!parent_)
    SetDefaultEnvironmentVariables(this);
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
  if (data_.find(name) == data_.end())
    InvalidateVariable(name);
}

void StyleEnvironmentVariables::InvalidateVariable(const AtomicString& name) {
  for (auto& it : children_)
    it->ParentInvalidatedVariable(name);
}

}  // namespace blink
