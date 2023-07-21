// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Background: V8's "Fast API" requires embedders to provide two versions
// of API function implementation: a Fast API-compatible implementation
// that does not perform any allocations on the V8 heap and a fallback
// implementation that gets executed when the API function needs to perform an
// allocation, or when the fast path cannot be used for any other reason.
//
// Motivation: the blink bindings abstract-away the need for dual
// implementations by providing a base class NoAllocHost, which, along with the
// bindings, abstracts away the implementation details that differ between the
// NoAlloc path and the regular slow path.
//
// Usage:
// * Add the [NoAllocDirectCall] extended IDL attribute to the methods to be
//   accelerated. See V8's "Fast API" for more details on limitations on what
//   can be accelerated.
// * Make sure the interface implementation class that the methods belong to
//   inherits NoAllocHost.
// * In the method implementation, call PostDeferrableAction() on all code paths
//   that need to escape the constraints that forbid the allocation of objects
//   on the V8 heap. See inlined documentation below for instruction on how to
//   use these methods.
//

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_NO_ALLOC_DIRECT_CALL_HOST_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_NO_ALLOC_DIRECT_CALL_HOST_H_

#include "base/check.h"
#include "base/compiler_specific.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "v8/include/cppgc/heap-consistency.h"
#include "v8/include/v8-fast-api-calls.h"

namespace blink {

// Manadatory base class for API interface classes with methods that use the
// NoAllocDirectCall extended IDL attribute.
class PLATFORM_EXPORT NoAllocDirectCallHost {
 public:
  NoAllocDirectCallHost();

  using DeferrableAction = base::OnceCallback<void()>;

  // Methods called from the implementations of APIs that use NoAllocDirectCall.
  //============================================================================

  // Posts an action that may be executed later by FlushDeferredActions(). If
  // not currently inside a NoAllocScope, the action will be executed
  // immediately.
  //
  // The purpose of deferrable actions is to provide an escape hatch in
  // situations where an API call that can normally run without performing any
  // allocations, needs to allocate on the V8 managed heap due to a rare
  // circumstance, e.g. a JavaScript programming error.
  //
  // It is allowed to call PostDeferrableAction action from code that was
  // invoked outside of the scope of a [NoAllocDirectCall] entry point. This is
  // convenient for code paths that are shared between fast and non-fast APIs.
  void PostDeferrableAction(DeferrableAction&& action);

  bool IsInFastMode() const { return callback_options_; }

  // Methods called by bindings code
  //=================================

  bool HasDeferredActions();
  void FlushDeferredActions();

  // Methods used by NoAllocDirectCallScope
  //========================================

  cppgc::HeapHandle& heap_handle() { return heap_handle_; }
  void EnterNoAllocDirectCallScope(v8::FastApiCallbackOptions*);
  void ExitNoAllocDirectCallScope();

 private:
  // We cache the heap handle here to avoid accessing thread-local storage
  // (ThreadState::Current) on each NADC method call.
  cppgc::HeapHandle& heap_handle_;

  WTF::Vector<DeferrableAction> deferred_actions_;
  v8::FastApiCallbackOptions* callback_options_ = nullptr;
};

class NoAllocDirectCallScope {
  STACK_ALLOCATED();

 public:
  NoAllocDirectCallScope(NoAllocDirectCallHost*, v8::FastApiCallbackOptions*);
  ~NoAllocDirectCallScope();

 private:
  NoAllocDirectCallHost* const host_;
  const cppgc::subtle::DisallowGarbageCollectionScope disallow_gc_;
};

// We use inline definitions for the methods used in bindings boilerplate that
// are systematically invoked on all NoAlloDirectCall entry points.

inline void NoAllocDirectCallHost::EnterNoAllocDirectCallScope(
    v8::FastApiCallbackOptions* callback_options) {
  DCHECK(!callback_options_);
  callback_options_ = callback_options;
}

inline void NoAllocDirectCallHost::ExitNoAllocDirectCallScope() {
  DCHECK(callback_options_);
  callback_options_ = nullptr;
}

inline bool NoAllocDirectCallHost::HasDeferredActions() {
  return !deferred_actions_.empty();
}

inline NoAllocDirectCallScope::NoAllocDirectCallScope(
    NoAllocDirectCallHost* host,
    v8::FastApiCallbackOptions* callback_options)
    : host_(host), disallow_gc_(host->heap_handle()) {
  host_->EnterNoAllocDirectCallScope(callback_options);
}

inline NoAllocDirectCallScope::~NoAllocDirectCallScope() {
  host_->ExitNoAllocDirectCallScope();
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_NO_ALLOC_DIRECT_CALL_HOST_H_
