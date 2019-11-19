/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 * Copyright (C) 2013 Google Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_EXECUTION_CONTEXT_CONTEXT_LIFECYCLE_NOTIFIER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_EXECUTION_CONTEXT_CONTEXT_LIFECYCLE_NOTIFIER_H_

#include "base/macros.h"
#include "third_party/blink/public/mojom/frame/lifecycle.mojom-blink-forward.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/lifecycle_notifier.h"

namespace blink {

class ContextLifecycleObserver;
class ContextLifecycleStateObserver;
class ExecutionContext;

class CORE_EXPORT ContextLifecycleNotifier
    : public LifecycleNotifier<ExecutionContext, ContextLifecycleObserver> {
 public:
  void NotifyContextLifecycleStateChanged(mojom::FrameLifecycleState state);

  unsigned ContextLifecycleStateObserverCount() const;

#if DCHECK_IS_ON()
  bool Contains(ContextLifecycleStateObserver*) const;
#endif
 protected:
  // Need a default constructor to link core and modules separately.
  // If no default constructor, we will see an error: "constructor for
  // 'blink::ExecutionContext' must explicitly initialize the base class
  // 'blink::ContextLifecycleNotifier' which does not have a default
  // constructor ExecutionContext::ExecutionContext()".
  ContextLifecycleNotifier() = default;

  DISALLOW_COPY_AND_ASSIGN(ContextLifecycleNotifier);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_EXECUTION_CONTEXT_CONTEXT_LIFECYCLE_NOTIFIER_H_
