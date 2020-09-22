// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_STYLE_ENVIRONMENT_VARIABLES_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_STYLE_ENVIRONMENT_VARIABLES_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/css_variable_data.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string_hash.h"

namespace blink {

// UADefinedVariable contains all the user agent defined environment variables.
// When adding a new variable the string equivalent needs to be added to
// |GetVariableName|.
enum class UADefinedVariable {
  // The safe area insets are four environment variables that define a
  // rectangle by its top, right, bottom, and left insets from the edge of
  // the viewport.
  kSafeAreaInsetTop,
  kSafeAreaInsetLeft,
  kSafeAreaInsetBottom,
  kSafeAreaInsetRight,

  // The keyboard area insets are six environment variables that define a
  // virtual keyboard rectangle by its top, right, bottom, left, width and
  // height insets
  // from the edge of the viewport.
  // Explainers:
  // https://github.com/MicrosoftEdge/MSEdgeExplainers/blob/main/VirtualKeyboardAPI/explainer.md
  kKeyboardInsetTop,
  kKeyboardInsetLeft,
  kKeyboardInsetBottom,
  kKeyboardInsetRight,
  kKeyboardInsetWidth,
  kKeyboardInsetHeight,

  // The fold environment variables define a rectangle that is splitting the
  // layout viewport.
  // Explainers:
  // https://github.com/MicrosoftEdge/MSEdgeExplainers/blob/master/Foldables/explainer.md
  kFoldTop,
  kFoldRight,
  kFoldBottom,
  kFoldLeft,
  kFoldWidth,
  kFoldHeight,
};

// StyleEnvironmentVariables stores user agent and user defined CSS environment
// variables. It has a static root instance that stores global values and
// each document has a child that stores document level values.
class CORE_EXPORT StyleEnvironmentVariables
    : public RefCounted<StyleEnvironmentVariables> {
 public:
  static StyleEnvironmentVariables& GetRootInstance();

  // Gets the name of a |UADefinedVariable| as a string.
  static const AtomicString GetVariableName(UADefinedVariable);

  // Create a new instance bound to |parent|.
  static scoped_refptr<StyleEnvironmentVariables> Create(
      StyleEnvironmentVariables& parent);

  virtual ~StyleEnvironmentVariables();

  // Set the value of the variable |name| and invalidate any dependents.
  void SetVariable(const AtomicString& name,
                   scoped_refptr<CSSVariableData> value);

  // Tokenize |value| and set it.
  void SetVariable(const AtomicString& name, const String& value);
  void SetVariable(const UADefinedVariable name, const String& value);

  // Remove the variable |name| and invalidate any dependents.
  void RemoveVariable(const AtomicString& name);

  // Resolve the variable |name| by traversing the tree of
  // |StyleEnvironmentVariables|.
  virtual CSSVariableData* ResolveVariable(const AtomicString& name);

  // Detach |this| from |parent|.
  void DetachFromParent();

  // Stringify |value| and append 'px'. Helper for setting variables that are
  // CSS lengths.
  static String FormatPx(int value);

 protected:
  friend class StyleEnvironmentVariablesTest;

  void ClearForTesting();

  // Bind this instance to a |parent|. This should only be called once.
  void BindToParent(StyleEnvironmentVariables& parent);

  // Called by the parent to tell the child that variable |name| has changed.
  void ParentInvalidatedVariable(const AtomicString& name);

  StyleEnvironmentVariables() : parent_(nullptr) {}

  // Called when variable |name| is changed. This will notify any children that
  // this variable has changed.
  virtual void InvalidateVariable(const AtomicString& name);

 private:
  class RootOwner;

  Vector<scoped_refptr<StyleEnvironmentVariables>> children_;
  HashMap<AtomicString, scoped_refptr<CSSVariableData>> data_;
  scoped_refptr<StyleEnvironmentVariables> parent_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_STYLE_ENVIRONMENT_VARIABLES_H_
