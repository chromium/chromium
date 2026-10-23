---
name: ios/avoid-weakself-in-short-lived-task-callbacks
description: In asynchronous callbacks dispatched by short-lived task or command objects, avoid capturing weakSelf or strong self; use standalone helpers capturing weak context objects
rubric: safety
severity: should_fix
---

**Rule.** Do not use `weakSelf` or capture `self` strongly in asynchronous
completion callbacks dispatched by short-lived, transient request or helper
objects that are not retained by a caller after their synchronous invocation
returns. Instead, extract the completion handling logic into a standalone
function in an anonymous namespace, binding only value-copied state and `__weak`
(or `base::WeakPtr`) references to long-lived context objects (such as
`SceneState`) with null checks.

**Why.** Transient request or helper objects are deallocated as soon as their
synchronous execution phase finishes. Capturing `weakSelf` causes the reference
to become `nil` before the asynchronous callback runs, silently dropping the
operation, while capturing strong `self` unnaturally extends the transient
object's lifetime. A standalone helper bound with weak context references
ensures asynchronous work executes without leaking or relying on transient
object lifecycles.

## Examples

Bad:

```cpp
__weak __typeof(self) weakSelf = self;
spotlight::GetURLForSpotlightItemID(
    itemID, base::CallbackToBlock(base::BindPostTask(
                base::SequencedTaskRunner::GetCurrentDefault(),
                base::BindOnce(^(NSURL* webpageURL) {
                  [weakSelf openSpotlightURL:webpageURL
                                      domain:domain
                                  targetMode:targetMode
                                  sceneState:sceneState];
                }))));
```

Good:

```cpp
__weak SceneState* weakSceneState = sceneState;
spotlight::GetURLForSpotlightItemID(
    itemID, base::CallbackToBlock(base::BindPostTask(
                base::SequencedTaskRunner::GetCurrentDefault(),
                base::BindOnce(^(NSURL* webpageURL) {
                  OpenSpotlightURL(webpageURL, domain, targetMode,
                                   weakSceneState);
                }))));
```
