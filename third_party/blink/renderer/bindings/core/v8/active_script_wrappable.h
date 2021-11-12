// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_ACTIVE_SCRIPT_WRAPPABLE_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_ACTIVE_SCRIPT_WRAPPABLE_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/bindings/active_script_wrappable_base.h"

namespace blink {

class ExecutionContext;

// Derived by wrappable objects which need to remain alive due to ongoing
// asynchronous activity, even if they are not referenced in the JavaScript or
// Blink heap.
//
// This is useful for ScriptWrappable objects that are not held alive by regular
// references from the object graph. E.g., XMLHttpRequest may have a pending
// activity that may be visible (e.g. firing event listeners or resolving
// promises) and should thus not be collected.
//
// Such objects should derive from ActiveScriptWrappable<T>, and override
// ScriptWrappable::HasPendingActivity:
//   bool HasPendingActivity() const final;
// which returns true if there may be pending activity which requires the
// wrappable remain alive.
//
// To avoid leaking objects after the context is destroyed, users of
// ActiveScriptWrappable<T> also have to provide a GetExecutionContext() method
// that returns the ExecutionContext or nullptr. A nullptr or already destroyed
// context results in ignoring HasPendingActivity().
template <typename T>
class ActiveScriptWrappable : public ActiveScriptWrappableBase {
 public:
  ActiveScriptWrappable(const ActiveScriptWrappable&) = delete;
  ActiveScriptWrappable& operator=(const ActiveScriptWrappable&) = delete;

  ~ActiveScriptWrappable() override = default;

 protected:
  ActiveScriptWrappable() = default;

  bool IsContextDestroyed() const final {
    return IsContextDestroyedForActiveScriptWrappable(
        static_cast<const T*>(this)->GetExecutionContext());
  }

  bool DispatchHasPendingActivity() const final {
    return static_cast<const T*>(this)->HasPendingActivity();
  }
};

// Helper for ActiveScriptWrappable<T>::IsContextDestroyed();
CORE_EXPORT bool IsContextDestroyedForActiveScriptWrappable(
    const ExecutionContext* execution_context);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_ACTIVE_SCRIPT_WRAPPABLE_H_
