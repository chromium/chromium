// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SCROLL_SCROLL_PROMISE_RESOLVER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SCROLL_SCROLL_PROMISE_RESOLVER_H_

#include <memory>

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

class ScriptState;
class ScrollResult;
template <typename T>
class ScriptPromiseResolver;

// A class to resolve the promise for a programmatic scroll request when all
// active scrollers for the request are done scrolling. This is achieved by
// holding a `ScriptPromiseResolver` for the JS side, and then maintaining one
// `ActiveScrollTracker` instance for each scroller affected by the request so
// that the script promise resolver is resolved automatically when all active
// scroll trackers are gone.
class ScrollPromiseResolver : public GarbageCollected<ScrollPromiseResolver> {
 public:
  // An RAII-style inner class that tracks active (pending/ongoing) scrolling on
  // a particular scroller. A `Element.scrollIntoView` request can create
  // multiple instances of this while any other JS request creates at most one.
  class ActiveScrollTracker {
   public:
    explicit ActiveScrollTracker(ScrollPromiseResolver* scroll_promise_resolver)
        : scroll_promise_resolver_(scroll_promise_resolver) {}

    ~ActiveScrollTracker() {
      scroll_promise_resolver_->ActiveScrollTrackerRemoved();
    }

    void MarkInterrupted() {
      scroll_promise_resolver_->scroll_is_interrupted_ = true;
    }

   private:
    Persistent<ScrollPromiseResolver> scroll_promise_resolver_;
  };

  explicit ScrollPromiseResolver(ScriptState* script_state);
  ~ScrollPromiseResolver();

  // Returns a tracker for an active scroll. This method must be called before
  // `GetScriptPromise`.
  std::unique_ptr<ActiveScrollTracker> CreateActiveScrollTracker();

  // Returns the promise to be used in JS, after resolving it if there is no
  // active scroll. This method must be called exactly once, and that call must
  // come after any possible calls to `CreateActiveScrollTracker`.
  ScriptPromise<ScrollResult> CreateScriptPromise();

  void Trace(Visitor* visitor) const;

 private:
  void ActiveScrollTrackerRemoved();
  void ResolvePromiseIfIdle();

  Member<ScriptPromiseResolver<ScrollResult>> resolver_;
  wtf_size_t num_active_scrolls_ = 0;
  bool script_promise_is_created_ = false;
  bool scroll_is_interrupted_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SCROLL_SCROLL_PROMISE_RESOLVER_H_
