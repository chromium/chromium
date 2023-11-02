// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_SCOPED_CSS_VALUE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_SCOPED_CSS_VALUE_H_

namespace blink {

class CSSValue;
class TreeScope;

// Store a CSSValue along with a TreeScope to support tree-scoped names and
// references for e.g. @font-face/font-family and @keyframes/animation-name.
// If the TreeScope pointer is null, we do not support such references, for
// instance for UA stylesheets.
class ScopedCSSValue {
  STACK_ALLOCATED();

 public:
  ScopedCSSValue(const CSSValue& value, const TreeScope* tree_scope)
      : value_(value), tree_scope_(tree_scope) {}
  const CSSValue& GetCSSValue() const { return value_; }
  const TreeScope* GetTreeScope() const { return tree_scope_; }

 private:
  const CSSValue& value_;
  const TreeScope* tree_scope_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_SCOPED_CSS_VALUE_H_
