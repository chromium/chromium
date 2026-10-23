---
name: ios/unregister-observers-or-use-scoped-observation
description: Manage observer lifecycles with ScopedObservation, guard shutdown unregistration, reset observations or bridge ivars before clearing service pointers, and omit redundant KeyedService shutdown overrides
rubric: safety
severity: blocking
citations:
  - path: docs/ui/learn/bestpractices/ownership.md
    heading: Prefer scoping objects
    quote: Prefer scoping objects to paired Add/Remove-type calls. For example, use a base::ScopedObservation<> instead of directly calling View::AddObserver() and RemoveObserver(). Such objects reduce the likelihood of use-after-free.
  - path: docs/dangling_ptr_guide.md
    heading: Observer callback
    quote: It is important to clear the pointer when the object is about to be deleted. Chrome uses the observer pattern heavily. In some cases, the observer does not clear its pointer toward the observed class when notified of its destruction.
  - path: components/signin/public/identity_manager/README.md
    heading: KeyedService Lifecycle
    quote: Observers must detach prior to destruction or during `OnIdentityManagerShutdown()`.
  - path: components/signin/public/identity_manager/objc/README.md
    heading: Invariants
    quote: During `OnIdentityManagerShutdown`, the bridge resets its observation and clears its `IdentityManager` pointer before notifying the delegate via `identityManagerDidShutdown:`, safely tolerating immediate deallocation of the bridge by its owner inside the callback.
  - path: components/signin/public/identity_manager/README.md
    heading: Invariants
    quote: 'KeyedService Lifecycle: `IdentityManager` is tied to the lifecycle of its associated `Profile` / `BrowserState`. Observers must detach prior to destruction or during `OnIdentityManagerShutdown()`.'
---

**Rule.** Prefer managing observer lifetimes with `base::ScopedObservation`,
`base::ScopedMultiSourceObservation`, or `PrefChangeRegistrar` over manual
paired calls to `AddObserver` and `RemoveObserver`. Declare scoped observations
as member variables initialized with `this` (e.g.,
`base::ScopedObservation<Source, Observer> observation_{this};`) and call
`.Observe(source)` during setup. When manual registration is required outside
shutdown, explicitly unregister listeners or notification tokens before
destruction, but omit redundant unregistration calls in destructors or
`-dealloc` for RAII- or system-managed observers. When unregistering observers
during lifecycle teardown or shutdown (such as in `KeyedService::Shutdown()`):

- Prefer resetting scoped observations via `observation_.Reset()`.
- In classes inheriting from `KeyedService` whose factories declare dependencies
  (such as `DependsOn(IdentityManagerFactory::GetInstance())`), reset
  observations (such as `identity_manager_observation_.Reset()`) in
  `KeyedService::Shutdown()` and omit redundant observer shutdown overrides like
  `signin::IdentityManager::Observer::OnIdentityManagerShutdown()`, because
  dependency ordering guarantees `Shutdown()` is invoked in topological order
  before the observed service shuts down.
- If manual unregistration from a dependency pointer is used (e.g.,
  `dependency_->RemoveObserver(this); dependency_ = nullptr;`), always guard the
  unregistration and pointer reset with a null check (e.g., `if (dependency_)`)
  to ensure shutdown operations are idempotent and safely tolerate repeated
  invocations or uninitialized dependencies.

When an observed subject notifies of shutdown or destruction (such as
`*BeingDeleted()`, `On*Destroyed()`, `*SourceDestroyed()`, `OnShuttingDown()`,
`OnBrowserListShutdown()`, or Objective-C bridge callbacks like
`-identityManagerDidShutdown:` or `-undoManagerShutdown`):

- Never leave the callback body empty.
- If lifetime or service dependencies (such as KeyedService `DependsOn`)
  guarantee the observer is always destroyed before the observed object, assert
  `NOTREACHED()` (or omit optional shutdown overrides like
  `OnIdentityManagerShutdown()`).
- Otherwise, disarm the observation (e.g., call `.Reset()`,
  `.RemoveObservation()`, `StopObserving`, or reset the owning observer bridge
  ivar via `_bridge.reset()`) AND clear stored raw pointers (such as
  `raw_ptr<T>` or Objective-C service pointers) to the dying object to `nullptr`
  or `nil`.
- In observer bridge or wrapper shutdown callbacks, never clear the unowned
  service pointer without unregistering observers or resetting the observer
  bridge ivar first, and never initiate new registrations.

Additionally, in observed subjects, avoid calling `Clear()` on `ObserverList` in
destructors, as it defeats empty-list assertions (`check_empty`) that detect
leaking observers.

**Why.** Manual paired calls to `AddObserver()` and `RemoveObserver()` require
boilerplate in destructors and risk dangling registrations or use-after-free
crashes if unregistration is omitted, called in the wrong order, or skipped on
early exit paths. RAII helpers (`base::ScopedObservation`,
`PrefChangeRegistrar`) automate unregistration upon destruction and provide safe
`.Reset()` helpers. During lifecycle teardown or shutdown (such as in
`KeyedService::Shutdown()`), cleanup methods may execute multiple times or when
dependencies were never initialized; unconditionally dereferencing dependency
pointers leads to null-pointer dereference crashes, whereas
`ScopedObservation::Reset()` or null checks guarantee idempotency. In
`KeyedService` subclasses, factory `DependsOn` declarations guarantee
topological shutdown ordering before dependency destruction, making observer
overrides like `OnIdentityManagerShutdown()` redundant when `Shutdown()` already
resets the observation. When an observed subject is destroyed before the
observer, an empty callback or nulling an unowned service pointer without
resetting the observer bridge (such as `_identityManagerBridge.reset()`) leaves
the observer registered with a dying observable, triggering MiraclePtr crashes,
`ObserverList` CHECK failures, or use-after-free when later teardown in
`-dealloc` or `-disconnect` cannot unregister the cleared pointer. Conversely,
removing an observation without clearing stored `raw_ptr<T>` members leaves
dangling pointers. Asserting `NOTREACHED()` documents and enforces unreachable
callbacks when lifetime order is guaranteed. Finally, forcibly clearing an
`ObserverList` in subject destructors silences leak detection assertions.

## Examples

Bad:

```cpp
// Bad: Redundant OnIdentityManagerShutdown in KeyedService with declared dependency.
void SafetyCheckManager::Shutdown() {}
void SafetyCheckManager::OnIdentityManagerShutdown(signin::IdentityManager*) {
  identity_manager_observation_.Reset();
}

// Bad: Clearing service pointer without resetting observer bridge or removing observer.
- (void)identityManagerDidShutdown:(signin::IdentityManager*)identityManager {
  _identityManager = nullptr;  // Missing _identityManagerBridge.reset()!
}

// Bad: Unguarded shutdown unregistration, empty callback, or missing raw_ptr reset.
void CredentialProviderService::Shutdown() {
  sync_service_->RemoveObserver(this);  // Crashes if null or already shutdown!
  sync_service_ = nullptr;
}
void HistoryClientImpl::BookmarkModelBeingDeleted() {}  // Empty callback leaves dangling pointer!
void AutoDeletionService::OnDownloadDestroyed(web::DownloadTask* task) {
  download_tasks_observation_.RemoveObservation(task);
  // Missing task_ = nullptr leaves dangling raw_ptr!
}
```

Good:

```cpp
// Good: Observation reset in KeyedService::Shutdown(); redundant callback omitted.
void SafetyCheckManager::Shutdown() {
  identity_manager_observation_.Reset();
}

// Good: Reset observer bridge and clear service pointer upon service shutdown.
- (void)identityManagerDidShutdown:(signin::IdentityManager*)identityManager {
  _identityManagerBridge.reset();
  _identityManager = nullptr;
}

// Good: Guard manual unregistration, use ScopedObservation, and clear raw_ptrs.
void CredentialProviderService::Shutdown() {
  scoped_observation_.Reset();
  if (sync_service_) {
    sync_service_ = nullptr;
  }
}
void BookmarkModelMetricsService::BookmarkModelBeingDeleted() {
  NOTREACHED();  // KeyedService dependencies guarantee prior destruction.
}
void AutoDeletionService::OnDownloadDestroyed(web::DownloadTask* task) {
  download_tasks_observation_.RemoveObservation(task);
  task_ = nullptr;
}
```
