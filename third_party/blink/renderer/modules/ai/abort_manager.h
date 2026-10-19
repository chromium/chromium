// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_AI_ABORT_MANAGER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_AI_ABORT_MANAGER_H_

#include "base/functional/callback.h"
#include "third_party/blink/renderer/core/dom/abort_signal.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

class AbortController;

// Manages session-level abort state for Built-In AI session objects, combining
// the `create()` AbortSignal, `destroy()` AbortController, and per-operation
// AbortSignals.
class MODULES_EXPORT AbortManager final {
  DISALLOW_NEW();

 public:
  template <typename Session>
  AbortManager(ScriptState* script_state,
               AbortSignal* create_abort_signal,
               Session* session)
      : AbortManager(
            script_state,
            create_abort_signal,
            create_abort_signal
                ? BindOnce(&AbortManager::OnCreateAbortSignalAborted<Session>,
                           WrapWeakPersistent(session),
                           WrapWeakPersistent(script_state))
                : base::OnceClosure()) {}

  ~AbortManager();

  AbortManager(const AbortManager&) = delete;
  AbortManager& operator=(const AbortManager&) = delete;

  void Trace(Visitor* visitor) const;

  // Returns a composite AbortSignal that combines the session's destruction
  // controller and `operation_signal`. If the signal is already aborted, throws
  // the abort reason on `exception_state` and returns nullptr.
  AbortSignal* CreateSignal(ScriptState* script_state,
                            AbortSignal* operation_signal,
                            ExceptionState& exception_state);

  // Aborts the session's destruction controller and removes the `create()`
  // abort listener; returns false if the context is invalid.
  bool OnDestroy(ScriptState* script_state, ExceptionState& exception_state);

 private:
  AbortManager(ScriptState* script_state,
               AbortSignal* create_abort_signal,
               base::OnceClosure on_create_abort);

  template <typename Session>
  static void OnCreateAbortSignalAborted(Session* session,
                                         ScriptState* script_state) {
    if (!session) {
      return;
    }
    // TODO(crbug.com/468104678): Align destroy() RaisesException behavior.
    if constexpr (requires {
                    session->destroy(script_state, IGNORE_EXCEPTION);
                  }) {
      session->destroy(script_state, IGNORE_EXCEPTION);
    } else {
      session->destroy(script_state);
    }
  }

  Member<AbortController> destruction_abort_controller_;
  Member<AbortSignal> create_abort_signal_;
  Member<AbortSignal::AlgorithmHandle> create_abort_handle_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_AI_ABORT_MANAGER_H_
