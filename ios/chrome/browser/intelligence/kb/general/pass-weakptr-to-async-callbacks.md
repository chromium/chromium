---
name: ios/pass-weakptr-to-async-callbacks
rubric: safety
severity: blocking
---

**Rule.** When binding arguments to asynchronous callbacks (e.g., via
base::BindOnce or base::BindRepeating), do not pass raw pointers (T\*) to
objects with unmanaged or dynamic lifecycles. Instead, bind a base::WeakPtr<T>
(e.g., via GetWeakPtr() or AsWeakPtr()) or extract and pass a stable unique
identifier by value (such as base::IdType or integer ID) to look up the object
safely upon callback execution. Additionally, for components or objects that
lack weak pointer semantics (such as WebStateList owned by Browser), do not pass
raw pointers to the component; bind a base::WeakPtr to the owning object (e.g.,
browser->AsWeakPtr()) and retrieve the owned component only after verifying the
owner is still alive. In the receiving callback, block handler, or task lambda,
always verify that the base::WeakPtr is non-null (e.g., if (!weak_ptr) return;
or if (auto\* ptr = weak_ptr.get())) before dereferencing it, accessing members,
retrieving owned objects, or passing .get() to functions expecting a valid
object.

**Why.** Asynchronous tasks and completion blocks execute after an indeterminate
delay or across nested run loops, during which referenced objects (such as tabs,
WebStates, tasks, profiles, or Browsers) may be destroyed or closed. Passing raw
pointers creates use-after-free hazards because raw pointers cannot detect
destruction and freed memory addresses may be recycled. Binding unique
identifiers decouples execution from object lifetime, while base::WeakPtr<T>
safely invalidates to null. Similarly, components like WebStateList are
destroyed when their owner Browser is closed (e.g., during window teardown);
binding base::WeakPtr<Browser> allows the callback to detect teardown safely
rather than risking a use-after-free on an unweakable child pointer. In all
cases, dereferencing an invalidated weak pointer or passing weak_ptr.get()
without checking validity causes null pointer dereference crashes or assertion
failures (CHECK(ref\_.IsValid())).

## Examples

Bad:

```cpp
// 1. Binding raw pointer when only an ID or WeakPtr is needed across async boundary.
task->GetResponseData(
    base::BindOnce(&AutoDeletionService::MarkTaskForDeletionHelper,
                   weak_ptr_factory_.GetWeakPtr(), task));

// 2. Passing raw WebStateList* when Browser can be destroyed during async work.
service_->LoadData(
    base::BindOnce(&TabsCounter::OnDataLoaded,
                   weak_ptr_factory_.GetWeakPtr(),
                   browser->GetWebStateList()));

// 3. Passing weak_ptr.get() without checking validity first.
void FinishCopy(base::WeakPtr<ProfileIOS> profile, CopyPolicyVerdicts v) {
  if (v.level() > Rule::Level::kNotSet) {
    ReportCopy(source_url, profile.get(), metadata);
  }
}
```

Good:

```cpp
// 1. Pass unique identifier by value (e.g., IdType) instead of raw pointer.
task->GetResponseData(
    base::BindOnce(&AutoDeletionService::MarkTaskForDeletionHelper,
                   weak_ptr_factory_.GetWeakPtr(), task_id));

// 2. Bind WeakPtr<Browser> to guard unweakable WebStateList across async teardown.
service_->LoadData(
    base::BindOnce(&TabsCounter::OnDataLoaded,
                   weak_ptr_factory_.GetWeakPtr(),
                   browser->AsWeakPtr()));

void TabsCounter::OnDataLoaded(base::WeakPtr<Browser> weak_browser) {
  Browser* browser = weak_browser.get();
  if (!browser) return;
  WebStateList* web_state_list = browser->GetWebStateList();
  // Safe to use web_state_list here...
}

// 3. Verify weak pointer is non-null before passing profile.get().
void FinishCopy(base::WeakPtr<ProfileIOS> profile, CopyPolicyVerdicts v) {
  if (v.level() > Rule::Level::kNotSet && profile) {
    ReportCopy(source_url, profile.get(), metadata);
  }
}
```
