// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_STYLE_RETAIN_SCOPE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_STYLE_RETAIN_SCOPE_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class ComputedStyle;

// This class retains references to temporary styles during layout.
class CORE_EXPORT StyleRetainScope {
  STACK_ALLOCATED();

 public:
  StyleRetainScope();
  ~StyleRetainScope();

  static StyleRetainScope* Current();

  // Retain a reference to |style| for the lifetime of |this|.
  void Retain(const ComputedStyle& style) {
    styles_retained_during_layout_.push_back(&style);
  }

 private:
  Vector<scoped_refptr<const ComputedStyle>> styles_retained_during_layout_;
  StyleRetainScope* parent_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_STYLE_RETAIN_SCOPE_H_
