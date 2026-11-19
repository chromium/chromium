---
name: ios/blocks-must-be-kept-short-single-method-call
description: In Objective-C++ asynchronous or stored blocks, capture self via weakSelf and local ivar/subview pointers via __weak locals instead of strongly, avoid nested blocks, and keep bodies to a single helper call on weakSelf without strongSelf unwrapping or ivar arrow dereferences; transient synchronous blocks may contain readable inline statements and reference self directly
rubric: safety
severity: should_fix
citations:
  - path: ios/best_practices.md
    heading: Blocks
    quote: Moving the body into a private method avoids `weakSelf`/`strongSelf` dance bugs, prevents accidental ivar capture, and produces clear stack traces.
  - path: ios/best_practices.md
    heading: Blocks
    quote: Referencing `self` or `_ivar` inside a block strongly retains `self`. Always capture `__weak __typeof(self) weakSelf = self;` (except in transient synchronous blocks; see below).
  - path: ios/best_practices.md
    heading: Blocks
    quote: 'Transient synchronous blocks do not need as much scrutiny: they can safely capture `self` directly (without `weakSelf`) and do not need to be factored into a single helper method call as long as they remain readable.'
  - path: ios/best_practices.md
    heading: Properties and Instance Variables
    quote: 'Never access an ivar via `->` on `weakSelf` (or any potentially `nil` pointer): Call a helper method on `weakSelf` instead (which is a safe no-op when `weakSelf` is `nil`).'
  - path: docs/mac/arc.md
    heading: Blocks
    quote: Blocks retain all objects referenced in them. This example is of a block used in an Objective-C method that uses the “weak `self`” idiom to avoid a retain cycle.
---

**Rule.** Never strongly capture `self`, bare ivars (`_ivar`), views, or
coordinators in asynchronous completion/callback blocks or stored Objective-C++
blocks (such as ivars, properties, `NSNotificationCenter` observers, repeating
timers, owned objects, `configurationUpdateHandler`, `actionHandler`, and
`cellProvider`). Capture `__weak __typeof(self) weakSelf = self;` and qualify
captured local pointers to ivars or subviews with `__weak`. Keep the block body
to a single helper method call on `weakSelf` (or capture needed values in local
variables before the block). Message `weakSelf` directly (e.g.,
`[weakSelf handleTransition]` or `[weakSelf handleResult:std::move(records)]`)
without `strongSelf = weakSelf` or `if (!weakSelf)` boilerplate. Never
dereference ivars via `->` (`self->_ivar`, `weakSelf->_ivar`, or
`strongSelf->_ivar`), and do not nest blocks (move the inner block into a helper
method called on `weakSelf`). Transient synchronous blocks executed before the
method returns and not retained afterwards (such as
`-[NSArray enumerateObjectsUsingBlock:]` or
`+[UIView performWithoutAnimation:]`) may contain readable inline multi-line
statements and reference `self` directly.

**Why.** Under ARC, a block strongly retains every Objective-C object it
captures (referencing `_ivar` retains `self`, and an unqualified local pointer
retains its target), so strong captures in stored or asynchronous blocks cause
retain cycles or extend lifetimes past dismissal. Delegating the body to a
helper method on `weakSelf` avoids `weakSelf`/`strongSelf` bugs and produces
clear stack traces; calling a method on `weakSelf` is a safe no-op when `nil`
and retains the receiver until return. Conversely, dereferencing
`weakSelf->_ivar` or `strongSelf->_ivar` violates encapsulation, and
`weakSelf->_ivar` crashes with a segmentation fault if `weakSelf` is `nil`.
Transient synchronous blocks are bound to the current call stack and cannot
outlive the call or create retain cycles. See `ios/best_practices.md#blocks`,
`ios/best_practices.md#properties-and-instance-variables`, and
`docs/mac/arc.md#examples-blocks`.

## Examples

Bad:

```cpp
// 1. Stored observer and async completion blocks strongly capture self, and a
// stored closure keeps a subview alive through a strong local pointer.
_observer = [[NSNotificationCenter defaultCenter]
    addObserverForName:UIApplicationDidBecomeActiveNotification
                object:nil
                 queue:nil
            usingBlock:^(NSNotification* notification) {
              [self refreshContent];
            }];
ToolbarProgressBar* progressBar = _progressBar;
_hideClosure.Reset(base::BindOnce(^{
  [progressBar setHidden:YES];
}));
[_service performOperationWithCompletion:^(BOOL success) {
  [self operationDidCompleteWithSuccess:success];
}];

// 2. Nested blocks.
__weak __typeof(self) weakSelf = self;
[_fetcher fetchWithCompletion:^(NSData* data) {
  [weakSelf.parser parseData:data
                  completion:^(FooResult* result) {
                    [weakSelf showResult:result];
                  }];
}];

// 3. Ivar arrow dereference, direct self/ivar capture, and unnecessary
// strongSelf unwrapping (including before passing a moved C++ argument).
[actions addObject:[factory actionWithBlock:^{
  weakSelf->_dataCount = 10;
  [self.delegate fooViewController:self didSelectItem:self->_item];
}]];
auto completion = ^{
  MyVC* strongSelf = weakSelf;
  if (!strongSelf) {
    return;
  }
  [strongSelf handleTransition];
};
auto callback = ^(std::vector<Record> records) {
  __strong __typeof(weakSelf) strongSelf = weakSelf;
  if (!strongSelf) {
    return;
  }
  [strongSelf handleResult:std::move(records)];
};
```

Good:

```cpp
// 1. Capture weakSelf and qualify captured local subview pointers with __weak.
__weak __typeof(self) weakSelf = self;
_observer = [[NSNotificationCenter defaultCenter]
    addObserverForName:UIApplicationDidBecomeActiveNotification
                object:nil
                 queue:nil
            usingBlock:^(NSNotification* notification) {
              [weakSelf refreshContent];
            }];
__weak ToolbarProgressBar* progressBar = _progressBar;
_hideClosure.Reset(base::BindOnce(^{
  [progressBar setHidden:YES];
}));
[_service performOperationWithCompletion:^(BOOL success) {
  [weakSelf operationDidCompleteWithSuccess:success];
}];

// 2. Move the inner block into a helper method called on weakSelf.
[_fetcher fetchWithCompletion:^(NSData* data) {
  [weakSelf didFetchData:data];
}];

// 3. Delegate to a single helper call on weakSelf without strongSelf
// unwrapping, even when moving a C++ argument.
[actions addObject:[factory actionWithBlock:^{
  [weakSelf didSelectCurrentItem];
}]];
auto completion = ^{
  [weakSelf handleTransition];
};
auto callback = ^(std::vector<Record> records) {
  [weakSelf handleResult:std::move(records)];
};

// 4. Transient synchronous blocks may contain readable inline statements and
// reference self directly.
[UIView performWithoutAnimation:^{
  [self updateHeaderLayout];
  [self.view layoutIfNeeded];
}];
[chapters enumerateObjectsUsingBlock:^(Chapter* chapter, NSUInteger index,
                                       BOOL* stop) {
  [self updateIndexWithContentOf:chapter];
}];
```
