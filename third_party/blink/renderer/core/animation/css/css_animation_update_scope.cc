// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/css/css_animation_update_scope.h"

#include "third_party/blink/renderer/core/animation/document_animations.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

CSSAnimationUpdateScope* CSSAnimationUpdateScope::current_ = nullptr;

CSSAnimationUpdateScope::CSSAnimationUpdateScope(Document& document)
    : document_(document) {
  if (!current_)
    current_ = this;
}

CSSAnimationUpdateScope::~CSSAnimationUpdateScope() {
  if (current_ == this) {
    if (RuntimeEnabledFeatures::CSSDelayedAnimationUpdatesEnabled())
      document_.GetDocumentAnimations().ApplyPendingElementUpdates();
    current_ = nullptr;
  }
}

}  // namespace blink
