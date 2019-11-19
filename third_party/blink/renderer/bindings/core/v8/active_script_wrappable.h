// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_ACTIVE_SCRIPT_WRAPPABLE_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_ACTIVE_SCRIPT_WRAPPABLE_H_

#include "base/macros.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/bindings/active_script_wrappable_base.h"

namespace blink {

class ExecutionContext;
class ScriptWrappable;

// Derived by wrappable objects which need to remain alive due to ongoing
// asynchronous activity, even if they are not referenced in the JavaScript or
// Blink heap.
//
// A ScriptWrappable ordinarily is held alive only if it has some such
// reference, usually via a wrapper object held by script. However, some
// objects, such as XMLHttpRequest, have pending activity that may be visible
// (e.g. firing event listeners or resolving promises), and so should not be
// collected, even if no references remain.
//
// Such objects should derive from ActiveScriptWrappable<T>, and override
// ScriptWrappable::HasPendingActivity:
//   bool HasPendingActivity() const final;
// which returns true if there may be pending activity which requires the
// wrappable remain alive.
//
// During wrapper tracing, ActiveScriptWrappables which belong to a
// non-destroyed execution context and have pending activity are treated as
// roots for the purposes of marking and so will keep themselves and objects
// they reference alive.
//
// Since this pending activity will not keep the wrappable alive after the
// context is destroyed, it is common for ActiveScriptWrappable objects to also
// derive from ContextLifecycleObserver to abort the activity at that time.
template <typename T>
class ActiveScriptWrappable : public ActiveScriptWrappableBase {
 public:
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
  ScriptWrappable* ToScriptWrappable() final { return static_cast<T*>(this); }

 private:
  DISALLOW_COPY_AND_ASSIGN(ActiveScriptWrappable);
};

// Helper for ActiveScriptWrappable<T>::IsContextDestroyed();
CORE_EXPORT bool IsContextDestroyedForActiveScriptWrappable(
    const ExecutionContext* execution_context);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_ACTIVE_SCRIPT_WRAPPABLE_H_
