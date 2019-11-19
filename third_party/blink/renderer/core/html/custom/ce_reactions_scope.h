// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CUSTOM_CE_REACTIONS_SCOPE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CUSTOM_CE_REACTIONS_SCOPE_H_

#include "base/macros.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

namespace blink {

class CustomElementReaction;
class Element;

// https://html.spec.whatwg.org/C/#cereactions
class CORE_EXPORT CEReactionsScope final {
  STACK_ALLOCATED();

 public:
  static CEReactionsScope* Current() { return top_of_stack_; }

  CEReactionsScope() : prev_(top_of_stack_), work_to_do_(false) {
    top_of_stack_ = this;
  }

  ~CEReactionsScope() {
    if (work_to_do_)
      InvokeReactions();
    top_of_stack_ = top_of_stack_->prev_;
  }

  void EnqueueToCurrentQueue(Element&, CustomElementReaction&);

 private:
  static CEReactionsScope* top_of_stack_;

  void InvokeReactions();

  CEReactionsScope* prev_;
  bool work_to_do_;

  DISALLOW_COPY_AND_ASSIGN(CEReactionsScope);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CUSTOM_CE_REACTIONS_SCOPE_H_
