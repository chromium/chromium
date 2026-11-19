---
name: ios/consumer-data-source-only-for-interaction-queries
description: Use consumer data sources only to pull immediate, transient decisions at interaction time without sender parameters, and push persistent UI state through the consumer protocol.
rubric: design
severity: should_fix
citations:
  - path: ios/architecture/mediators.md
    heading: Interaction-Time Queries
    quote: 'If the value is only used for an immediate, transient decision, the view controller may pull it from a consumer data source: a protocol declared in `<feature>_data_source.h` in `ui/` and implemented by the mediator. Unlike a UIKit data source (such as `UITableViewDataSource`), a consumer data source method must not take the requesting consumer (`self`) as a sender parameter.'
---

**Rule.** Use a consumer data source (`<Feature>DataSource`) only to pull
immediate, transient decisions during a user interaction, such as context menu
actions or drag-and-drop eligibility. Push any state that remains visible in the
UI through the consumer protocol (`<Feature>Consumer`) instead. Do not pass a
sender parameter (such as the requesting view controller) to data source
methods.

**Why.** Pulling displayed state on demand leaves the UI stale when the model
changes, whereas transient interaction-time decisions are used immediately and
discarded. Omitting the sender parameter keeps the mediator decoupled from the
concrete consumer implementation. See
`ios/architecture/mediators.md#interaction-time-queries`.

## Examples

Bad:

```cpp
@class DownloadToolbarViewController;

// Bad: Pulling persistent displayed state from a data source and passing the
// requesting view controller as a sender parameter.
@protocol DownloadToolbarDataSource <NSObject>
- (float)downloadProgressInToolbar:(DownloadToolbarViewController*)toolbar;
- (NSString*)downloadFilenameInToolbar:(DownloadToolbarViewController*)toolbar;
@end
```

Good:

```cpp
typedef NS_OPTIONS(NSUInteger, DownloadToolbarActions) {
  DownloadToolbarActionNone = 0,
  DownloadToolbarActionOpenIn = 1 << 0,
  DownloadToolbarActionSaveToDrive = 1 << 1,
};

// Good: Persistent displayed state is pushed over the consumer protocol.
@protocol DownloadToolbarConsumer <NSObject>
- (void)setProgress:(float)progress;
- (void)setFilename:(NSString*)filename;
@end

// Good: Only transient decisions needed at interaction time are pulled from the
// data source, with no sender parameter.
@protocol DownloadToolbarDataSource <NSObject>
- (DownloadToolbarActions)availableMenuActions;
@end
```
