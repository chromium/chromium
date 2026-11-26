---
name: ios/mediator-pushes-state-to-consumer-no-polling
description: Push displayed model state from mediators to view controllers via <Feature>Consumer protocols rather than passing changeable state at initialization, polling via getters, or anticipating state that the model will provide
rubric: design
severity: should_fix
citations:
  - path: ios/architecture/mediators.md
    heading: The Mutation-Observation Cycle
    quote: When the view controller invokes a mutator method, the mediator updates the underlying model, observes the resulting model change notification, and pushes a corresponding update back to the view controller over the consumer protocol. The view controller updates its views only in response to that consumer call.
  - path: ios/architecture/mediators.md
    heading: Interaction-Time Queries
    quote: 'Sometimes the UI needs an answer from the model only at the moment of a user interaction, and only for a decision that is used immediately and then discarded: which actions to show in a context menu, whether a drag can be dropped on a given target, or which options to offer in a just-presented action sheet.'
---

**Rule.** Push displayed state from mediators to view controllers over a
`<Feature>Consumer` protocol rather than passing changeable state through view
controller initializers or properties. When a feature has a mediator, push even
non-changing displayed values once from `-setConsumer:`. Do not pull persistent
displayed state via delegate, mutator, or provider getters. Pull only immediate,
transient interaction-time decisions via `<Feature>DataSource`. Follow the
mutation-observation cycle for model changes: push what the model reports rather
than anticipating state that the model will explicitly provide. Mediators may
still push explicit intermediate UI states, such as loading indicators, during
slow asynchronous mutations.

**Why.** Pushing displayed state over `<Feature>Consumer` keeps the view
controller updated as models change, whereas state passed through initializers
or pulled through getters goes stale. Interaction-time decisions are safe to
pull because they are used immediately and then discarded. Waiting for model
observation after a mutation ensures that the UI reflects the authoritative
state of the model. See
`ios/architecture/mediators.md#the-mutation-observation-cycle` and
`ios/architecture/mediators.md#interaction-time-queries`.

## Examples

Bad:

```cpp
enum class DownloadToolbarState { kInProgress, kCancelled, kComplete };

// Anti-pattern 1: View controller accepts changeable displayed state via an
// initializer or public property instead of a consumer protocol.
@interface DownloadToolbarViewController : UIViewController
@property(nonatomic, assign) DownloadToolbarState downloadState;
- (instancetype)initWithDownloadState:(DownloadToolbarState)state;
@end

// Anti-pattern 2: View controller polls persistent displayed state via a
// provider or delegate getter on demand.
@protocol DownloadToolbarStateProvider <NSObject>
- (DownloadToolbarState)currentDownloadState;
- (float)currentDownloadProgress;
@end

// Anti-pattern 3: Mediator anticipates state that the model will provide
// instead of waiting for the model to report the change.
@protocol DownloadToolbarConsumer <NSObject>
- (void)setDownloadState:(DownloadToolbarState)state;
- (void)setDownloadProgress:(float)progress;
@end

@interface DownloadToolbarMediator : NSObject
@property(nonatomic, weak) id<DownloadToolbarConsumer> consumer;
@end

@implementation DownloadToolbarMediator {
  raw_ptr<web::DownloadTask> _downloadTask;
}

- (void)cancelDownload {
  _downloadTask->Cancel();
  [self.consumer setDownloadState:DownloadToolbarState::kCancelled];
}

@end
```

Good:

```cpp
enum class DownloadToolbarState { kInProgress, kCancelled, kComplete };

// Pattern 1: View controller adopts <Feature>Consumer; the mediator pushes
// initial and updated displayed state.
@protocol DownloadToolbarConsumer <NSObject>
- (void)setDownloadState:(DownloadToolbarState)state;
- (void)setDownloadProgress:(float)progress;
@end

@interface DownloadToolbarViewController
    : UIViewController <DownloadToolbarConsumer>
@end

// Pattern 2: Persistent displayed state is pushed via <Feature>Consumer;
// <Feature>DataSource is used only for transient interaction-time decisions.
@protocol DownloadToolbarDataSource <NSObject>
- (BOOL)canSaveDownloadToDrive;
@end

// Pattern 3: Mediator follows the mutation-observation cycle, pushing
// authoritative state reported by the model.
@interface DownloadToolbarMediator : NSObject
@property(nonatomic, weak) id<DownloadToolbarConsumer> consumer;
- (void)disconnect;
@end

@interface DownloadToolbarMediator () <CRWDownloadTaskObserver>
@end

@implementation DownloadToolbarMediator {
  raw_ptr<web::DownloadTask> _downloadTask;
  std::unique_ptr<web::DownloadTaskObserverBridge> _observerBridge;
  std::unique_ptr<
      base::ScopedObservation<web::DownloadTask, web::DownloadTaskObserver>>
      _scopedObservation;
}

- (void)disconnect {
  _scopedObservation.reset();
  _observerBridge.reset();
  _downloadTask = nullptr;
}

- (void)setConsumer:(id<DownloadToolbarConsumer>)consumer {
  _consumer = consumer;
  if (_downloadTask) {
    [self pushDownloadStatus];
  }
}

// DownloadToolbarMutator
- (void)cancelDownload {
  _downloadTask->Cancel();
}

// CRWDownloadTaskObserver
- (void)downloadUpdated:(web::DownloadTask*)task {
  [self pushDownloadStatus];
}

- (void)pushDownloadStatus {
  DownloadToolbarState state =
      _downloadTask->GetState() == web::DownloadTask::State::kCancelled
          ? DownloadToolbarState::kCancelled
          : DownloadToolbarState::kInProgress;
  [_consumer setDownloadState:state];
  [_consumer setDownloadProgress:_downloadTask->GetPercentComplete() / 100.0f];
}

@end
```
