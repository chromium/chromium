---
name: ios/prefer-weakptr-over-observer-reset
description: When referencing dynamic objects like web::WebState, prefer WeakPtr over raw pointers or observer boilerplate, but use cleared raw_ptr when actively observing
rubric: safety
severity: should_fix
citations:
  - path: base/memory/README.md
    heading: base::WeakPtr<T>
    quote: Use when a reference to an object might outlive the object itself. ... When the associated factory (usually owned by T) is destroyed, all `WeakPtr<T>` are invalidated (becomes null) rather than becoming use-after-frees.
  - path: docs/dangling_ptr_guide.md
    heading: Observer callback
    quote: It is important to clear the pointer when the object is about to be deleted. Chrome uses the observer pattern heavily. In some cases, the observer does not clear its pointer toward the observed class when notified of its destruction.
  - path: docs/dangling_ptr_guide.md
    heading: Challenging lifespan
    quote: "It can be challenging to deal with an object's lifespan. Sometimes, the lifetime of two objects are completely different. Removing the pointer may be a good thing to do. Sometimes, it can be replaced by: ... A WeakPtr"
  - path: styleguide/c++/c++.md
    heading: Non-owning pointers in class fields
    quote: Class and struct fields that are not garbage-collected should be written `const raw_ref<T>` or `raw_ptr<T>` rather than `T&` or `T*` whenever possible.
---

**Rule.** When holding non-owning references to objects with dynamic lifecycles
(such as web::WebState, web::DownloadTask, or content::WebContents) across
classes with independent lifecycles—including mediators, coordinators, helper
objects, request handlers, bridges, or Objective-C++ properties/ivars:

1. When the pointee may be destroyed asynchronously (independently of the
   holder), store `base::WeakPtr<T>` (obtained via `web_state->GetWeakPtr()`) in
   the member or `@implementation` ivar rather than a bare `T*` or `raw_ptr<T>`,
   and always verify that the weak reference is valid before dereferencing it.
   This includes drag-and-drop payload wrappers and other asynchronous UI
   tracking objects (for example a `TabGroupInfo` holding
   `base::WeakPtr<const TabGroup>` and `base::WeakPtr<ProfileIOS>`), which check
   the pointer before acting on a drop. Unowned pointers whose pointee is
   guaranteed to outlive the holder, or that are cleared by an observer (item
   4), stay `raw_ptr<T>` (`const raw_ref<T>` when never null). In both cases an
   Objective-C `@property` exposing the pointer is declared with a bare `T*`
   (`@property(nonatomic, assign) web::WebState* webState;`) and implemented
   with an explicit getter and setter backed by the `base::WeakPtr<T>` or
   `raw_ptr<T>` ivar; method and function parameters stay bare `T*`/`T&`, never
   `base::WeakPtr<T>` or `raw_ptr<T>`.
2. Do not introduce or implement an observer interface (such as
   web::WebStateObserver or WebStateObserverBridge) solely to observe
   destruction (e.g., WebStateDestroyed()) and clear a raw pointer.
3. Conversely, when destruction requires active teardown, UI cleanup, or
   notifying consumers (such as detaching views or tearing down child
   coordinators), do not rely on base::WeakPtr<T>—which invalidates silently
   without notification—and instead actively observe destruction (e.g., via
   WebStateObserver or destruction notification handlers).
4. When an object's lifecycle is explicitly observed or managed (via an observer
   interface, base::ScopedObservation, or destruction callbacks), store the
   non-owning member pointer as raw_ptr<T> rather than base::WeakPtr<T>. Always
   explicitly reset or clear the stored raw_ptr<T> to nullptr both in
   destruction callbacks (e.g., On...Destroyed(), WebStateDestroyed()) and when
   explicitly stopping observation (e.g., calling ScopedObservation::Reset() or
   RemoveObservation()) to prevent dangling pointers.

**Why.** Objects such as web::WebState represent browser tabs whose lifecycles
are dynamically managed by WebStateList and can be destroyed at any moment due
to tab closure, replacement, or memory eviction. Holding unobserved bare
pointers (auto-synthesized `assign` properties or bare `T*` ivars) across
independent lifecycles, asynchronous tasks, or presentation flows creates
dangling pointers and use-after-free vulnerabilities. base::WeakPtr<T> safely
clears itself upon destruction without requiring explicit observer boilerplate,
and keeping the property type a bare `T*` with explicit accessors hides the
storage choice from callers and keeps parameters free of smart-pointer wrappers.
However, base::WeakPtr<T> invalidates silently without notification; if
dependent UI or state requires active teardown, relying on base::WeakPtr leaves
orphaned views or stale state, necessitating synchronous destruction
observation. When an observer is already tracking object lifecycle, holding
base::WeakPtr is redundant and obscures the synchronous lifecycle contract.
Because raw_ptr does not automatically clear itself, failing to null it out in
destruction callbacks or when unobserving (ScopedObservation::Reset() or
RemoveObservation()) leaves member pointers dangling, triggering dangling
pointer detector crashes.

## Examples

Bad:

```cpp
// Bad: Auto-synthesized assign property stores a bare web::WebState* that
// nothing clears, or an observer implemented solely to null the pointer out.
@property(nonatomic, assign) web::WebState* webState;  // No explicit accessors.
class TabTracker : public web::WebStateObserver {
  void WebStateDestroyed(web::WebState*) override { web_state_ = nullptr; }
  raw_ptr<web::WebState> web_state_;
};

// Bad: WeakPtr when observing lifecycle, or failing to clear raw_ptr on unobserve.
class Service : public web::DownloadTaskObserver {
  base::ScopedObservation<web::DownloadTask, web::DownloadTaskObserver> obs_{this};
  base::WeakPtr<web::DownloadTask> task_; // Redundant WeakPtr
};
void Service::OnDone(web::DownloadTask* t) { obs_.RemoveObservation(t); /* task_ dangles! */ }

// Bad: Silent WeakPtr invalidation when active UI teardown is required.
@implementation Mediator { base::WeakPtr<web::WebState> _ws; }
```

Good:

```cpp
// Good: Bare T* property backed by a WeakPtr ivar with explicit accessors, for
// independent lifetimes without active teardown.
@property(nonatomic, assign) web::WebState* webState;

@implementation Helper {
  base::WeakPtr<web::WebState> _webState;
}
- (web::WebState*)webState {
  return _webState.get();
}
- (void)setWebState:(web::WebState*)webState {
  _webState = webState ? webState->GetWeakPtr() : nullptr;
}
@end
class Helper { base::WeakPtr<web::WebState> web_state_; };

// Good: Use raw_ptr when observing lifecycle; reset on destruction or unobserve.
class Service : public web::DownloadTaskObserver {
  void OnDownloadDestroyed(web::DownloadTask* t) override { obs_.Reset(); task_ = nullptr; }
  base::ScopedObservation<web::DownloadTask, web::DownloadTaskObserver> obs_{this};
  raw_ptr<web::DownloadTask> task_;
};
void Service::OnDone(web::DownloadTask* t) { obs_.RemoveObservation(t); task_ = nullptr; }

// Good: Observe destruction when active UI teardown is required, clearing raw_ptr.
- (void)webStateDestroyed:(web::WebState*)ws { [self.consumer setView:nil]; _ws = nullptr; }
```
