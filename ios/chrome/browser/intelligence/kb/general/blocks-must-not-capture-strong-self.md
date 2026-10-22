---
name: ios/blocks-must-not-capture-strong-self
rubric: safety
severity: should_fix
---

**Rule.** Do not capture `self`, instance variables, UI views, subviews, or
coordinators strongly inside blocks, including UIKit animation blocks and
completion handlers, configuration blocks (e.g., `registerForTraitChanges:`),
and asynchronous callbacks; always capture
`__weak __typeof(self) weakSelf = self;` and message `weakSelf`, and qualify
intermediate local pointer variables (such as extracted ivars or subviews) with
`__weak` to prevent retain cycles and prolonged object lifetimes. Do not extract
ivars into unadorned local pointer variables before block capture, as ARC
implicitly treats them as `__strong` references. Inside blocks using `weakSelf`,
ensure all references message `weakSelf` rather than referencing `self` or
`_ivar` directly; delegate to a helper method on `weakSelf` if `self` must be
passed as an argument. Do not capture outer `strongSelf` pointers in nested
escaping closures or `base::BindOnce` callbacks. Outside block closures, in
ordinary synchronous method bodies, use `self` directly rather than creating or
messaging `weakSelf` or `strongSelf` aliases.

**Why.** Under Objective-C ARC, blocks strongly retain all captured object
references by default, and referencing `self` or `_ivar` inside a block retains
`self`. Capturing `self`, UI views, subviews, or view controllers strongly in
blocks, including UIKit animation and completion blocks whose execution timing
the caller does not control, creates retain cycles and keeps view hierarchies
alive long after dismissal. Extracting an ivar or subview to an unadorned local
pointer variable before block capture avoids naming `self` directly, but creates
an implicit `__strong` reference that artificially extends the referenced
object's lifetime until the block completes. Capturing an outer `strongSelf` in
nested escaping closures or `base::BindOnce` callbacks prolongs object lifetimes
across secondary tasks. Outside blocks, the receiver (`self`) is guaranteed to
remain alive for the entire synchronous call, so `weakSelf` aliases there add
noise and misleadingly imply the receiver could deallocate mid-execution. See
`ios/best_practices.md#blocks` and `docs/mac/arc.md#examples-blocks`.

## Examples

Bad:

```cpp
// Extracting ivar/subview into unadorned local variable before block capture:
ToolbarProgressBar* progressBar = _progressBar;
_hideClosure.Reset(base::BindOnce(^{
  [progressBar setHidden:YES];
}));

// Capturing outer strongSelf in nested async task:
base::BindOnce(^(NSData* data) {
  [strongSelf loadData:data];
});

// Referencing self or an ivar directly inside an animation block:
[UIView animateWithDuration:0.2 animations:^{
  [self updateAlpha];
  _overlayView.alpha = 0;
}];
```

Good:

```cpp
// Qualify extracted ivar/subview or captured view with __weak:
__weak ToolbarProgressBar* progressBar = _progressBar;
_hideClosure.Reset(base::BindOnce(^{
  [progressBar setHidden:YES];
}));

// Passing weakSelf into nested async task:
base::BindOnce(^(NSData* data) {
  [weakSelf loadData:data];
});

// Capturing weakSelf and keeping the animation block to a single call:
__weak __typeof(self) weakSelf = self;
[UIView animateWithDuration:0.2 animations:^{
  [weakSelf updateAlpha];
}];
```
