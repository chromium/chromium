---
name: ios/avoid-capturing-this-in-objc-blocks
description: In Objective-C blocks or asynchronous callbacks, do not capture raw C++ pointers, this, or dereference C++ members on weakSelf; use base::WeakPtr, CallbackToBlock, or strongSelf
rubric: safety
severity: blocking
citations:
  - path: docs/mac/arc.md
    heading: Blocks {#examples-blocks}
    quote: For blocks used in C++ functions, a retain cycle is not a concern, though the use of a `base::WeakPtr<>` might be needed to avoid stale pointers.
  - path: docs/callback.md
    heading: Binding A Class Method With Weak Pointers
    quote: A callback bound using a weak pointer receiver will be automatically cancelled (calling `Run()` becomes a no-op) if the weak pointer is invalidated, e.g. its associated class instance is destroyed.
  - path: docs/mac/arc.md
    heading: Blocks
    quote: For blocks used in C++ functions, a retain cycle is not a concern, though the use of a `base::WeakPtr<>` might be needed to avoid stale pointers.
---

**Rule.** In Objective-C++ code, do not capture raw C++ pointers (such as
Browser\*, WebState\*, or helper objects), C++ this (explicitly or implicitly
via member variables), or dereference C++ member pointers and chained properties
on weakSelf (such as weakSelf.cppMember->Method() or passing
weakSelf.sceneState.profileState.profile to C++ APIs) inside Objective-C blocks,
C++ lambdas, or asynchronous callbacks. Under ARC, blocks manage Objective-C
object lifetimes but capture raw C++ pointers by bare value without lifetime
tracking, and weakSelf accessors or helpers (e.g., BrowserUserData::FromBrowser)
unconditionally dereference null or dangling pointers if the instance is
deallocated. Instead, safely manage C++ lifetimes using one of the following
approaches:

1. Member methods and this: Extract logic into a member function and adapt it
   using base::CallbackToBlock(base::BindOnce(&MyClass::Method,
   weak_ptr_factory\_.GetWeakPtr(), ...)) or bind it with
   base::BindOnce/base::BindRepeating.
2. External C++ objects: Bind the callback with
   base::BindOnce/base::BindRepeating and a base::WeakPtr<T> (obtained via
   GetWeakPtr() or AsWeakPtr())—either to a member function of T, or to a free
   function or lambda that takes the base::WeakPtr<T> as a parameter and
   verifies it before use (if (!weak_object) return;)—and convert it with
   base::CallbackToBlock() where a block is required.
3. Primitive or immutable values: If the block only needs specific fields (such
   as IDs, URLs, or primitive values) rather than the living C++ instance,
   extract them into local variables before the block to capture them by value.
4. Operations on weakSelf: Encapsulate C++ calls in an Objective-C helper method
   invoked on weakSelf (which safely no-ops if deallocated), keeping the block
   to that single call rather than promoting weakSelf to a local strongSelf.

**Why.** Objective-C blocks do not track or extend the lifecycle of captured C++
objects. If an asynchronous operation, UI animation, or deferred callback
completes after the C++ instance is destroyed, executing the block dereferences
a dangling pointer, resulting in use-after-free crashes or security
vulnerabilities. Furthermore, when an Objective-C object is deallocated before
an asynchronous callback executes, weakSelf becomes nil. While sending an
Objective-C message to nil is a safe runtime no-op, accessing C++ member
pointers on nil (such as weakSelf.URLLoadingBrowserAgent->LoadURLForQuery(...))
or evaluating property chains on nil into C++ API arguments yields nullptr,
causing immediate segmentation faults or CHECK assertion failures in helpers
like BrowserUserData::FromBrowser or signin::GetIdentities. Pre-extracting
values avoids capturing C++ pointers entirely; binding through base::WeakPtr
ensures callback invocation is safely dropped or aborted when the object is
destroyed; and delegating to an Objective-C helper method leverages
nil-messaging safety while keeping every C++ access out of the block body.

## Examples

Bad:

```cpp
// Bad: Implicit this/member capture or raw C++ pointers in blocks.
void PasswordBridge::FetchData() {
  [data_source_ fetchWithCallback:^(std::vector<Item> items) {
    [observer_bridge_ notifyItems:items];  // Implicitly captures raw this.
  }];
}

UrlLoadingBrowserAgent* agent = UrlLoadingBrowserAgent::FromBrowser(browser);
return [self actionWithBlock:^{ agent->Load(params); }];  // Raw C++ pointer.

// Bad: Dereferencing C++ member pointers or weak property chains on weakSelf.
dispatch_async(dispatch_get_main_queue(), ^{
  if (weakSelf.URLLoadingBrowserAgent) {
    weakSelf.URLLoadingBrowserAgent->LoadURLForQuery(query);
  }
});

base::OnceClosure closure = base::BindOnce(
    [](std::unique_ptr<UIBlocker> blocker, __weak __typeof(self) weakSelf) {
      RunPrefetch(signin::GetIdentities(weakSelf.sceneState.profileState.profile));
    },
    std::move(uiBlocker), weakSelf);
```

Good:

```cpp
// Good: Bridge via CallbackToBlock, bind base::WeakPtr into a helper, or
// extract the value beforehand.
void PasswordBridge::FetchData() {
  [data_source_ fetchWithCallback:base::CallbackToBlock(
      base::BindOnce(&PasswordBridge::OnData, weak_ptr_factory_.GetWeakPtr()))];
}

// Free function in the anonymous namespace: the WeakPtr is checked here, never
// inside a block body.
void LoadParamsInBrowser(base::WeakPtr<Browser> weak_browser,
                         const UrlLoadParams& params) {
  if (!weak_browser) {
    return;
  }
  UrlLoadingBrowserAgent::FromBrowser(weak_browser.get())->Load(params);
}

return [self actionWithBlock:base::CallbackToBlock(base::BindOnce(
                                 &LoadParamsInBrowser, browser->AsWeakPtr(),
                                 params))];

// Good: Delegate to an Objective-C helper method on weakSelf. The helper
// performs the C++ calls and is a safe no-op once self is deallocated.
dispatch_async(dispatch_get_main_queue(), ^{
  [weakSelf loadQuery:query];
});
```
