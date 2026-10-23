---
name: ios/avoid-dangling-untriaged-on-raw-ptr
description: When declaring new raw_ptr member fields, do not annotate them with DanglingUntriaged; manage object lifetimes or reset pointers on teardown instead
rubric: safety
severity: should_fix
citations:
  - path: docs/dangling_ptr.md
    heading: Dangling Pointer Detector
    quote: "The DanglingUntriaged option has been used to annotate pre-existing dangling pointers in Chrome: ... Contrary to DisableDanglingPtrDetection, we don't know yet why it dangles. It is meant to be either refactored to avoid dangling, or turned into \"DisableDanglingPtrDetection\" with a comment explaining what happens."
  - path: base/memory/raw_ptr.md
    heading: Dangling Pointer Detection (kMayDangle)
    quote: '- **DanglingUntriaged**: Technical debt; identified but not yet triaged.'
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
