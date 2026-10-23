---
name: ios/prefer-raw-pointer-over-weakptr-for-sync-params
description: When passing, returning, or storing non-owning objects in synchronous functions or accessors, use raw pointers or references instead of base::WeakPtr
rubric: readability
severity: should_fix
citations:
  - path: styleguide/c++/c++.md
    heading: Object ownership and calling conventions
    quote: Return `T*` if-and-only-if the caller does not take ownership.
  - path: base/memory/README.md
    heading: base::WeakPtr<T>
    quote: Use when a reference to an object might outlive the object itself.
---

**Rule.** When passing or returning non-owning objects in synchronous functions
and accessors, or storing references in objects whose lifetime cannot outlive
the pointee (such as KeyedServices owned by a Profile), do not use
base::WeakPtr<T> or ->AsWeakPtr(). Return raw pointers (T\*) from non-owning
member accessors, letting callers call AsWeakPtr() if weak semantics are needed.
Pass mandatory non-null arguments as references (const T& or T&), use raw
pointers (T\*) for nullable arguments or Objective-C methods, and use raw_ptr<T>
for member fields. Reserve base::WeakPtr<T> for asynchronous callbacks, posted
tasks, or decoupled lifetimes.

**Why.** Under Chromium's ownership conventions, accessors and synchronous
functions providing non-owning access must use raw pointers (T\*) or references
rather than base::WeakPtr<T>, avoiding unnecessary weak pointer tracking
overhead and interface constraints. Callers needing weak semantics for
asynchronous callbacks or posted tasks can call AsWeakPtr() on the returned
pointer. In owned hierarchies or synchronous calls, base::WeakPtr obscures
lifetimes and bypasses MiraclePtr dangling protection.

## Examples

Bad:

```cpp
// Returning WeakPtr from an owned member accessor:
base::WeakPtr<syncer::SyncableService> GetThemeSyncableService();

// Passing WeakPtr for a mandatory synchronous argument in C++:
ContentAnalysisInfo::ContentAnalysisInfo(
    const GURL& url,
    AnalysisSettings settings,
    base::WeakPtr<web::WebState> web_state);

// Passing WeakPtr in Objective-C or owned hierarchies (e.g. KeyedService):
- (BOOL)canHandleSelectionInWebState:(base::WeakPtr<web::WebState>)webState;
auto model = std::make_unique<Model>(client->AsWeakPtr());
```

Good:

```cpp
// Return raw pointer from accessor; callers invoke AsWeakPtr() if needed:
syncer::SyncableService* GetThemeSyncableService();

// Pass mandatory synchronous arguments as const references:
ContentAnalysisInfo::ContentAnalysisInfo(
    const GURL& url,
    AnalysisSettings settings,
    const web::WebState& web_state);

// Pass raw pointers for Obj-C / nullable params, and store raw_ptr<T>:
- (BOOL)canHandleSelectionInWebState:(web::WebState*)webState;
auto model = std::make_unique<Model>(client);
```
