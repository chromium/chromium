// Copyright 2018 The Chromium Authors
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

class FeatureContext;

// UADefinedVariable contains all user agent defined environment variables with
// a single dimension.
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

  // The title bar area variables are four environment variables that define a
  // rectangle by its x and y position as well as its width and height. They are
  // intended for desktop PWAs that use the window controls overlay.
  // Explainer:
  // https://github.com/WICG/window-controls-overlay/blob/main/explainer.md
  kTitlebarAreaX,
  kTitlebarAreaY,
  kTitlebarAreaWidth,
  kTitlebarAreaHeight
};

enum class UADefinedTwoDimensionalVariable {
  // The viewport segment variables describe logically distinct regions of the
  // viewport, and are indexed in two dimensions (x and y).
  kViewportSegmentTop,
  kViewportSegmentRight,
  kViewportSegmentBottom,
  kViewportSegmentLeft,
  kViewportSegmentWidth,
  kViewportSegmentHeight,
};

// StyleEnvironmentVariables stores user agent and user defined CSS environment
// variables. It has a static root instance that stores global values and
// each document has a child that stores document level values.
// Setting and removing values can only be done for the set of variables in
// UADefinedVariable. Note that UADefinedVariables are not always set/defined,
// as they depend on the environment.
class CORE_EXPORT StyleEnvironmentVariables
    : public RefCounted<StyleEnvironmentVariables> {
 public:
  static StyleEnvironmentVariables& GetRootInstance();

  // Gets the name of a |UADefinedVariable| as a string.
  // |feature_context| is required for a RuntimeEnabledFeatures check for a
  // variable in origin trial, otherwise nullptr can be passed.
  static const AtomicString GetVariableName(
      UADefinedVariable variable,
      const FeatureContext* feature_context);
  static const AtomicString GetVariableName(
      UADefinedTwoDimensionalVariable variable,
      const FeatureContext* feature_context);

  // Create a new instance bound to |parent|.
  static scoped_refptr<StyleEnvironmentVariables> Create(
      StyleEnvironmentVariables& parent);

  virtual ~StyleEnvironmentVariables();

  // Tokenize |value| and set it. This will invalidate any dependents.
  void SetVariable(UADefinedVariable variable, const String& value);

  // Tokenize |value| and set it. This will invalidate any dependents.
  void SetVariable(UADefinedTwoDimensionalVariable variable,
                   unsigned first_dimension,
                   unsigned second_dimenison,
                   const String& value);

  // Remove the variable |name| and invalidate any dependents.
  void RemoveVariable(UADefinedVariable variable);
  // Remove all the indexed variables referenced by the enum, and invalidate any
  // dependents.
  void RemoveVariable(UADefinedTwoDimensionalVariable variable);

  // Resolve the variable |name| by traversing the tree of
  // |StyleEnvironmentVariables|.
  virtual CSSVariableData* ResolveVariable(const AtomicString& name,
                                           WTF::Vector<unsigned> indices);

  // Detach |this| from |parent|.
  void DetachFromParent();

  // Stringify |value| and append 'px'. Helper for setting variables that are
  // CSS lengths.
  static String FormatPx(int value);

  virtual const FeatureContext* GetFeatureContext() const;

 protected:
  friend class StyleEnvironmentVariablesTest;
  friend class StyleCascadeTest;

  // Tokenize |value| and set it, invalidating dependents along the way.
  void SetVariable(const AtomicString& name, const String& value);
  void SetVariable(const AtomicString& name,
                   unsigned first_dimension,
                   unsigned second_dimension,
                   const String& value);

  void RemoveVariable(const AtomicString& name);

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

  typedef WTF::Vector<WTF::Vector<scoped_refptr<CSSVariableData>>>
      TwoDimensionVariableValues;

  Vector<scoped_refptr<StyleEnvironmentVariables>> children_;
  HashMap<AtomicString, scoped_refptr<CSSVariableData>> data_;
  HashMap<AtomicString, TwoDimensionVariableValues> two_dimension_data_;
  scoped_refptr<StyleEnvironmentVariables> parent_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_STYLE_ENVIRONMENT_VARIABLES_H_
