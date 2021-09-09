// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_CSS_CSS_ANIMATION_UPDATE_SCOPE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_CSS_CSS_ANIMATION_UPDATE_SCOPE_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class Document;

// CSSAnimationUpdateScope applies pending animation on destruction, if it
// is the *current* scope. A CSSAnimationUpdateScope becomes the current
// scope upon construction if there isn't one already.
class CORE_EXPORT CSSAnimationUpdateScope {
  STACK_ALLOCATED();

 public:
  explicit CSSAnimationUpdateScope(Document&);
  ~CSSAnimationUpdateScope();

  static bool HasCurrent() { return current_; }

 private:
  Document& document_;

  static CSSAnimationUpdateScope* current_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_CSS_CSS_ANIMATION_UPDATE_SCOPE_H_
