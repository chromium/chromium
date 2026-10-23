---
name: ios/reserve-set-prefix-for-property-accessors
description: 'In Objective-C, use -set<Name>: (or @property) for state assignment and consumer push methods; do not name delegate or observer event callbacks -set...:, and do not use -inject...: or -update...: for assignment'
rubric: readability
severity: should_fix
citations:
  - path: styleguide/objective-c/objective-c.md
    heading: Chromium Objective-C and Objective-C++ style guide
    quote: Chromium follows the Google Objective-C style guide unless an exception is listed below.
---

**Rule.** In Objective-C, `-set<Name>:` denotes the assignment of state: a
property accessor, or a consumer method through which a mediator pushes state to
a view controller (`-setProgress:`, `-setFooEnabled:`). Do not name delegate or
observer callback methods like setters; name them after the event they report
(e.g., `<source>Did<Action>:`). Conversely, when assigning or replacing object
dependencies or mutating backing state, follow standard Cocoa conventions using
`set<Name>:` or declaring a `@property` instead of ad-hoc prefixes like
`inject<Name>:`. `-set<State>:` methods on `*Consumer` protocols and other
state-push methods are the documented convention and are not flagged, even when
the receiver configures views rather than storing the value in a property.

**Why.** In Cocoa and Objective-C conventions, `set<Name>:` methods denote
property mutators linked to dot syntax, Key-Value Coding (KVC), and property
synthesis, and the mediator documentation extends the same spelling to consumer
methods that push model state to the UI. Naming delegate or observer callbacks
with `set` creates misleading APIs because a callback reports an event rather
than assigning state, while inventing non-standard prefixes like `inject` breaks
established Cocoa naming patterns and public interface consistency.

## Examples

Bad:

```cpp
// Delegate callback named like a property setter
@protocol OmniboxPopupPresenterDelegate
- (void)setPresenter:(OmniboxPopupPresenter*)presenter;
@end

// Dependency assignment with an ad-hoc prefix
- (void)injectProvider:(id<FormInputSuggestionsProvider>)provider;
```

Good:

```cpp
// Delegate callback named after the event it reports
@protocol OmniboxPopupPresenterDelegate
- (void)popupDidInitializePresenter:(OmniboxPopupPresenter*)presenter;
@end

// Dependency assignment
- (void)setProvider:(id<FormInputSuggestionsProvider>)provider;

// Consumer state push: set<State>: is the documented convention even though
// the view controller configures a view rather than storing a property.
@protocol DownloadToolbarConsumer <NSObject>
- (void)setProgress:(float)progress;
- (void)setDownloadState:(DownloadToolbarState)state;
@end

- (void)setProgress:(float)progress {
  self.progressView.progress = progress;
}
```
