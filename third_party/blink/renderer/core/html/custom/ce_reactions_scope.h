// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CUSTOM_CE_REACTIONS_SCOPE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CUSTOM_CE_REACTIONS_SCOPE_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

namespace blink {

class Agent;
class CustomElementReaction;
class CustomElementReactionStack;
class Element;
class ExecutionContext;

// https://html.spec.whatwg.org/C/#cereactions
class CORE_EXPORT CEReactionsScope final {
  STACK_ALLOCATED();

 public:
  explicit CEReactionsScope(ExecutionContext* execution_context);
  explicit CEReactionsScope(Agent& agent);
  ~CEReactionsScope();

  CEReactionsScope(const CEReactionsScope&) = delete;
  CEReactionsScope& operator=(const CEReactionsScope&) = delete;

  void EnqueueToCurrentQueue(Element&, CustomElementReaction&);

 private:
  CustomElementReactionStack& stack_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CUSTOM_CE_REACTIONS_SCOPE_H_
