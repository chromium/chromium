---
name: ios/avoid-dangling-untriaged-on-raw-ptr
rubric: safety
severity: should_fix
---

**Rule.** Do not annotate new `raw_ptr` members with `DanglingUntriaged` (or
related untriaged traits like `FlakyDanglingUntriaged`,
`AcrossTasksDanglingUntriaged`, or `LeakedDanglingUntriaged`). Design object
lifetimes so pointers do not outlive their pointees, or reset pointers to
`nullptr` during teardown (such as in `-disconnect`, `TearDown`, or
destructors). If a dangling pointer is genuinely unavoidable due to external
architectural constraints, use `DisableDanglingPtrDetection` with an inline
comment justifying why it is safe and will not be dereferenced.

**Why.** `DanglingUntriaged` is technical debt reserved for pre-existing
dangling pointers across legacy code so that dangling pointer detection can run
without breaking builds. Bypassing detection in new code masks object lifetime
bugs and undermines MiraclePtr protections against use-after-free
vulnerabilities.

## Examples

Bad:

```cpp
// Bypassing dangling pointer detection in new code with DanglingUntriaged:
raw_ptr<WebStateList, DanglingUntriaged> _webStateList;
```

Good:

```cpp
// Properly structured lifetime or explicit teardown with standard raw_ptr:
raw_ptr<WebStateList> _webStateList;

- (void)disconnect {
  _webStateList = nullptr;
}
```
