# Mediators

> **Core Principle:** To **encapsulate** the business logic that controls the
> user interface, create a **mediator** to **observe** the relevant **models**
> and **send updates** to the UI over a **consumer** interface.

______________________________________________________________________

## Motivating Ideas

One of the fundamental layering decisions in Chrome on iOS is to treat the model
and the user interface as separate **layers**. The **model** layer consists of
data structures, services, objects, and logic that are not directly part of the
user interface or application infrastructure. Bookmarks, user preferences, the
list of open tabs (`WebStateList`), the navigation history of a tab, and the
user's identity are all examples of model-layer concepts. Chrome on iOS also
treats **web page content as model data**—an entire layer unto itself
(`//ios/web/`).

Between these two layers is the coordinator layer, where mediators keep them
**decoupled**: view controllers have no direct model-layer dependencies, and
model-layer code is never coupled to specific UI implementations (see
[Patterns of Structure](README.md#patterns-of-structure)).

The Mediator pattern is one of the classic behavioral patterns from *Design
Patterns* (Gamma et al.), which defines a mediator as "an object that
encapsulates how a set of objects interact." In Chrome on iOS, a mediator
encapsulates the interactions between model objects and the user interface for a
**feature**, implementing what is typically called the application's **business
logic**.

This separation allows the UI of a feature to vary without changing the
underlying model interactions, and allows the behavior and structure of model
objects to evolve independently of the UI. For example, an experimental UI for a
feature like Bookmarks can be implemented without duplicating business logic,
and a new Bookmarks backend with a different API can be adopted without changing
any UI code.

______________________________________________________________________

## Architectural Overview

Mediators encapsulate all interactions between the model layer and the user
interface:

- They are created and owned by **coordinators**.
- They are initialized with the specific model-layer services and objects they
  need.
- They push UI configuration and model-driven updates into a **consumer**
  interface (`id<FooConsumer>`).
- When user actions in the UI depend on the model (such as changing model
  state), the UI calls back into the mediator through a **mutator** interface
  (`id<FooMutator>`).

The class name suffix `*Mediator` is reserved exclusively for classes that
mediate between the model layer and the UI layer as described in this document;
it must not be used for general coordinator helpers or model-layer classes.
Mediators do not inherit from a universal base class; they are simple subclasses
of `NSObject` written in Objective-C++ (`.mm`).

### The Consumer Relationship

The **consumer** is typically a `UIViewController` subclass that adopts a
feature-specific **consumer protocol** defined in the UI layer. The consumer
protocol is part of the public API of the view controller: it declares the
model-driven events and state updates that the view controller can present.

The mediator, however, does not depend on its consumer being a view
controller—it depends only on the consumer protocol. The owning coordinator,
which creates both the view controller and the mediator, is responsible for
connecting them. This decoupling makes unit testing straightforward, as either
end of the consumer protocol can be replaced with a fake or mock in tests.

### Directory Structure and Dependencies

As described in
[Directory Structure and Dependencies](coordinators.md#directory-structure-and-dependencies),
a feature's mediators live alongside its coordinators in the `coordinator/`
subdirectory, while views and view controllers live in `ui/`. Because the `ui/`
directory must not depend on `coordinator/` or `model/`, all protocol
definitions shared between a mediator and a view controller—including
**consumer**, **consumer data source**, and **mutator** protocols, as well as
any data classes they pass—must be declared in headers located in the `ui/`
directory. Both the view controller (in `ui/`) and the mediator (in
`coordinator/`) import these headers from `ui/`.

______________________________________________________________________

## Creating, Initializing, and Disconnecting Mediators

### Creation and Initialization

Mediators are created and owned by
[coordinators](coordinators.md#objects-created-and-connected-by-a-coordinator),
and a mediator's lifetime is strictly tied to an
**[active UI feature](coordinators.md#parent-ownership-of-lifecycle)**. A
feature's coordinator creates its mediator in `-start` (never in `-init`),
passes the required model-layer services into the mediator's initializer, and
assigns the view controller to the mediator's `consumer` property:

```objectivec
// reading_list_mediator.h (in coordinator/)
@interface ReadingListMediator : NSObject

@property(nonatomic, weak) id<ReadingListConsumer> consumer;

- (instancetype)initWithReadingListService:(ReadingListService*)service
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

- (void)disconnect;

@end
```

```objectivec
// reading_list_coordinator.mm (in coordinator/)
@implementation ReadingListCoordinator {
  ReadingListTableViewController* _viewController;
  ReadingListMediator* _mediator;
}

- (void)start {
  _viewController = [[ReadingListTableViewController alloc] init];
  ReadingListService* service =
      ReadingListServiceFactory::GetForProfile(self.browser->GetProfile());
  _mediator =
      [[ReadingListMediator alloc] initWithReadingListService:service];
  _mediator.consumer = _viewController;
  // Present or contain _viewController...
}

- (void)stop {
  [_mediator disconnect];
  _mediator = nil;
  // Dismiss or remove _viewController...
  _viewController = nil;
}

@end
```

### The "Line in the Sand" for Dependencies

Notice that in the example above, the coordinator passes the specific
`ReadingListService*` into the mediator rather than passing the `ProfileIOS*`
(or `Browser*`) from which the `ReadingListService` was retrieved.

This is the **[line in the sand](README.md#line-in-the-sand)** principle:
"backbone container" objects that can vend arbitrary services and
dependencies—such as `Browser`, `ProfileIOS`, `SceneState`, `ProfileState`,
`AppState`, `ApplicationContext`, or `CommandDispatcher`—are passed down through
the coordinator hierarchy, but are **never passed out of a coordinator** to its
mediator or view controller. For the same reason, a mediator must **never call
`GetApplicationContext()` directly** in its implementation; if a mediator needs
an application-scoped dependency (such as the `localState` `PrefService*`), the
coordinator must retrieve it from `GetApplicationContext()` and inject it into
the mediator. Narrowly specifying each of a mediator's dependencies in its
initializer or public properties keeps its responsibilities explicit, simplifies
unit testing, and supports multi-window and multi-identity execution
transparently (because the coordinator resolves the appropriate `Browser` or
`ProfileIOS` instance and injects the already-scoped services, leaving the
mediator agnostic to scene or profile routing).

### Dependencies Needed After Initialization

A mediator never asks its coordinator to construct objects, return values, or
report state at runtime. Everything a mediator needs from the model layer is
injected at initialization, even if it is only used later—including state owned
by other features, such as preferences or browser-scoped state. If a mediator
seems to need something only its coordinator can provide, the work in question
is usually a UI transition, which the mediator requests by invoking a command
(see
[Mediators Accessing Other Services and Commands](#mediators-accessing-other-services-and-commands)).

### Observing Model Objects

To push updates to its consumer when model state changes, an Objective-C++
mediator typically observes one or more C++ model objects. Because the mediator
is an Objective-C object and the model observer interface is usually a C++ pure
virtual class, mediators commonly use a C++ **observer bridge** (frequently
managed with `base::ScopedObservation`) that forwards C++ observer callbacks to
Objective-C methods on the mediator. Implementing an observer bridge is part of
the model layer's responsibility; mediators should use model-provided bridges
and must not implement C++-to-Objective-C bridging logic themselves.

When a mediator cares about the active `WebState` in a `Browser`'s
`WebStateList`, it should receive the `WebStateList*` at initialization and
observe it directly (or use an observation forwarder for the active `WebState`),
rather than having the active `WebState` repeatedly injected from the outside as
tabs change.

### Teardown and `-disconnect`

In Objective-C, object deallocation timing (`-dealloc`) is indeterminate because
UIKit, autorelease pools, or blocks may transiently retain objects after a
feature's UI is dismissed. Consequently, **`-dealloc` must not be relied upon to
perform cleanup required for correctness**.

Whenever a mediator holds references to unowned objects whose lifetimes may end
before the mediator is deallocated (such as C++ model services) or registers for
external callbacks that must stop when the feature closes (such as model
observers, notification center observers, or timers), it should expose a
`-disconnect` method. The owning coordinator must call `[_mediator disconnect]`
in its [`-stop` method](coordinators.md#deterministic-teardown-in-stop) before
releasing its reference to the mediator. Inside `-disconnect`, the mediator
unregisters all active observations, cancels pending callbacks, and clears its
pointers to C++ services and model objects.

If a simple mediator has no active observations or unowned references requiring
deterministic cleanup, `-disconnect` may not be needed.

### Unit Testing Mediators

Because mediators receive narrow, explicitly injected model dependencies and
communicate with the UI exclusively through lightweight protocols, they are
straightforward to unit-test in isolation without instantiating view controllers
or views. Every mediator should have a comprehensive unit test suite that
injects test or fake model objects, connects a fake or mock consumer, triggers
model changes or mutator calls, and verifies the resulting consumer updates and
model mutations.

The architecture's support for testing comes from its guiding principles of
encapsulation and decoupling: a mediator's narrowly injected dependencies *are*
its test seam. If writing a test seems to require exposing a new interface,
stretching an architectural pattern, routing a call through the coordinator, or
working around the limitations of a mocking library, then **the test design is
wrong**. Instead, inject fake or test model services, drive behavior through the
model and the mutator, and verify results on the consumer and the model; don't
substitute objects that the mediator creates for itself.

______________________________________________________________________

## Consumer Protocols

A **consumer protocol** (`<Feature>Consumer`) defines the updates that a
mediator sends to its consumer. Typical consumer methods push the initial state
to display and update that state when the model changes. For example, a toolbar
that shows the status of a download might have this consumer protocol:

```objectivec
// download_toolbar_consumer.h (in ui/)

// The state of a download, as presented by the toolbar.
enum class DownloadToolbarState {
  kInProgress,
  kPaused,
  kCancelled,
  kComplete,
};

@protocol DownloadToolbarConsumer <NSObject>

// Sets the fraction of the download that has completed, from 0.0 to 1.0.
- (void)setProgress:(float)progress;

// Sets the total number of bytes received so far.
- (void)setReceivedBytes:(int64_t)receivedBytes;

// Sets the state of the download.
- (void)setDownloadState:(DownloadToolbarState)state;

@end
```

Unlike UIKit delegate and data source protocols—which couple the two objects
together by passing the concrete delegating object (such as `UITableView*`) as
the first parameter of every method—a consumer protocol is a decoupled
interface. Consumer methods must never take the mediator as a UIKit-style sender
parameter (for example, `-readingListMediator:updateItems:`). Consumer protocols
must never be named `*Delegate` or `*Commands`.

Consumer protocols must be designed to be **UI-independent** and
**state-independent**, and must only pass **simple value types**.

### UI Independence

**UI independence** means the mediator has no knowledge of how the consumer
renders the data. The download progress pushed by a mediator might be displayed
by the consumer as a progress bar, a circular indicator, or a percentage label.
The consumer protocol simply provides the value.

### State Independence

**State independence** means that each update sent over the consumer protocol
should be self-contained and should not rely on the consumer's internal state
being implicitly synchronized with a specific prior model state:

```objectivec
@protocol DownloadToolbarConsumer <NSObject>

// BAD: Relies on the consumer having received every previous update.
- (void)addReceivedBytes:(int64_t)newlyReceivedBytes;

// GOOD: Each update carries the complete current value.
- (void)setReceivedBytes:(int64_t)receivedBytes;

@end
```

For the same reason, consumer updates identify items in a collection by stable
identifiers, not by position. An index identifies the right item only if the
consumer's items exactly match the model's at the moment of the update; an
identifier doesn't depend on the consumer's prior state:

```objectivec
@protocol BookmarkListConsumer <NSObject>

// BAD: Correct only if the consumer's list exactly matches the model's.
- (void)removeBookmarkAtIndex:(NSUInteger)index;

// GOOD: Identifies the bookmark regardless of the consumer's prior state.
- (void)removeBookmarkWithUUID:(const base::Uuid&)uuid;

@end
```

### Data Types Passed Over Consumer Protocols

Model-layer services often define rich C++ or Objective-C structs and classes
(such as `bookmarks::BookmarkNode`) that represent domain schemas or include
model-manipulation capabilities. **Consumer protocols must never pass
model-layer objects or structs.** Doing so leaks the model layer into the UI
layer and couples the view controller to the model.

Instead, the mediator is responsible for converting model-layer types into
simple value types suitable for direct consumption by the UI—typically
Foundation types (e.g., `NSString`), primitive integers/booleans, or
`enum class` values. Note that this boundary is between the model layer and the
UI layer, not between C++ and Objective-C. Because the UI layer is implemented
in Objective-C++, general-purpose C++ types such as `enum class` values,
`std::vector` containers, or lightweight identifier types (such as `base::Uuid`)
are appropriate in consumer protocols. However, **C++ strings (`std::string`,
`std::u16string`) should not be passed to a consumer**: if a string is meant to
be displayed, the mediator should convert it to `NSString*`, and if it
represents a state used to control the UI, it should be an `enum class` instead.

- **URLs:** URLs (`GURL` or `NSURL`) may be passed over consumer protocols and
  back through the mutator, rather than wrapped in another object just to
  round-trip them. A view controller must **never** load or fetch data from a
  URL.
- **Colors:** Pass `UIColor*` values over a consumer protocol only when the
  color is intrinsically associated with a specific model instance (for example,
  a tab group color or a color provided by a cross-platform model API). When
  color depends on the *category* or *state* of an object, pass an `enum class`
  value identifying that state and let the UI layer map it to the appropriate
  `UIColor`.
- **Localized strings:** In most cases, the UI layer is responsible for
  resolving static localized strings (including mapping an `enum class` state
  from the consumer protocol to a localized label). However, when the model
  layer dynamically selects string identifiers (such as specific error messages,
  warnings, or multi-step flow prompts) or when choosing the string involves
  business logic, the mediator should resolve the localized `NSString*` and pass
  the final string to the consumer. Passing raw string resource IDs across the
  consumer protocol is discouraged because it makes testing the consumer
  interface harder.
- **Images:** Mediators for features that display model-provided images (such as
  favicons or page thumbnails) may pass `UIImage*` instances directly over the
  consumer protocol.

______________________________________________________________________

## On-Demand Data from the Mediator

By default, data flow from the mediator to the consumer is **pushed**: the
mediator observes the model and pushes updates to the consumer without the
consumer requesting them.

Pushing a value does not require observing it. If a value cannot change while
the feature's UI is visible (such as a feature flag or a policy read when the
feature starts), the mediator simply pushes it once from `-setConsumer:`; no
observer is needed. If a value *can* change while the UI is visible but the
model provides no way to observe the change, having the UI pull the value does
not solve the problem; it only hides the resulting staleness. The fix belongs in
the model layer.

### Interaction-Time Queries

Sometimes the UI needs an answer from the model only at the moment of a user
interaction, and only for a decision that is used immediately and then
discarded: which actions to show in a context menu, whether a drag can be
dropped on a given target, or which options to offer in a just-presented action
sheet. Pre-pushing these answers is wasteful, and because the answer is never
left on screen, it cannot go stale.

Such a query is made once per interaction. A view controller must never
repeatedly poll the mediator (for example, on a timer or on every layout pass)
to detect model changes; detecting changes is the mediator's job, and changes
reach the UI only by being pushed over the consumer protocol.

To decide whether a value should be pushed or may be pulled, ask: **if this
value changed while the UI is visible, would the UI be wrong?** If so, the value
must be pushed over the consumer protocol (even if only once). If the value is
only used for an immediate, transient decision, the view controller may pull it
from a **consumer data source**: a protocol declared in
`<feature>_data_source.h` in `ui/` and implemented by the mediator. Unlike a
UIKit data source (such as `UITableViewDataSource`), a consumer data source
method must not take the requesting consumer (`self`) as a sender parameter.
Consumer data source methods are subject to these constraints:

- The method is declared on the **consumer data source** protocol, never on the
  mutator protocol.
- The method has no side effects.
- The method may take context that the view controller has only at the time of
  the interaction.
- The method returns a **decision or configuration**, not raw model facts. The
  view controller must not combine several pulled facts into a display decision;
  that combination is business logic and belongs in the mediator, where it can
  be unit-tested.

```objectivec
// download_toolbar_actions.h (in ui/)
typedef NS_OPTIONS(NSUInteger, DownloadToolbarActions) {
  DownloadToolbarActionNone = 0,
  DownloadToolbarActionOpenIn = 1 << 0,
  DownloadToolbarActionSaveToDrive = 1 << 1,
  DownloadToolbarActionCopyLink = 1 << 2,
};

// download_toolbar_data_source.h (in ui/)
@protocol DownloadToolbarDataSource <NSObject>

// BAD: Raw model facts that the view controller must combine into a decision.
- (BOOL)isUserSignedIn;
- (BOOL)isSaveToDriveAllowedByPolicy;

// GOOD: A decision for the transient menu shown when the user taps the
// toolbar's "more" button. The view controller maps each action to a
// `UIAction`.
- (DownloadToolbarActions)availableMenuActions;

@end
```

If the same decision also affects UI that remains visible (such as whether a
control is enabled), that state must be pushed over the consumer protocol
instead.

### Connecting the Consumer Data Source

The view controller exposes a weak `dataSource` property, and the coordinator
connects it to the mediator during `-start`:

```objectivec
// download_toolbar_view_controller.h (in ui/)
@interface DownloadToolbarViewController
    : UIViewController <DownloadToolbarConsumer>

@property(nonatomic, weak) id<DownloadToolbarDataSource> dataSource;

@end
```

______________________________________________________________________

## Timing of Mediator Updates

### Sequence Guarantees

Although a mediator may interact with model-layer services that perform work on
background sequences, **all interactions across the mediator/UI boundary
(consumer, consumer data source, and mutator calls) must occur on the main
sequence**.

### Initial Population and `-viewDidLoad`

When a coordinator assigns `_mediator.consumer = _viewController;` in `-start`,
the mediator typically pushes the initial model state to the consumer
synchronously inside its `-setConsumer:` setter. Because UIKit loads a view
controller's view lazily on presentation, this assignment almost always occurs
before `-viewDidLoad` has run. **View controllers must therefore be able to
correctly handle consumer calls before `-viewDidLoad` is called.**

There are two valid ways for a view controller to handle consumer calls that
arrive before `-viewDidLoad`:

- **Store incoming state in instance variables and apply it in `-viewDidLoad`:**
  Consumer methods store the incoming values in instance variables, and the view
  controller uses those stored values to populate its UI when the view loads.
- **Configure lazily instantiated subviews without accessing `self.view`:** If
  the view controller uses lazy property getters for its subviews, consumer
  methods can configure those subviews directly so they are ready when the view
  loads (taking care not to access `self.view`, which would prematurely force
  the view to load).

Consumer methods must not assume that `-viewDidLoad` has already run (for
example, by asserting that objects created in `-viewDidLoad` are non-nil).
Requiring a specific setup order forces the coordinator to be coupled to the
view controller's implementation details.

______________________________________________________________________

## Model Updates Through the Mediator

Many features allow the user to create, modify, or delete model data through UI
controls. Because the view controller does not have direct access to model
objects, it triggers model modifications through the mediator.

To keep the UI layer and the mediator decoupled, model-updating operations are
defined in an Objective-C **mutator protocol** (`<Feature>Mutator`) declared in
its own header in the `ui/` directory. While the view controller is the
*consumer* in relation to the mediator, the mediator is the *mutator* in
relation to the view controller.

Like consumer protocols, mutator protocols are decoupled from the sender: the
view controller must never pass itself (`self`) as a sender parameter to a
mutator method. Protocols implemented by a mediator to handle model updates from
a view controller should always be named `<Feature>Mutator` (with a
corresponding `mutator` property on the view controller), never `*Delegate`,
`*ActionHandler`, or `*Commands`.

The mutator's role is to take **actions that depend on the model**. Most
commonly this means changing model state, but it also includes triggering UI
changes that require model data the view controller does not have. For example,
if the user chooses "Edit Group" on a tab group in the tab grid, the view
controller has only the group's identifier: it calls a mutator method with that
identifier, and the mediator resolves the `TabGroup` and dispatches the
appropriate [command](commands.md). By contrast, user actions that affect only
the UI and that the view controller can fully specify on its own—such as
dismissing the feature, navigating between parts of its UI, or showing a QR code
for a URL it already has—do not go through the mediator; instead, the view
controller invokes a command handler directly, using either a shared command
protocol or a [local command protocol](coordinators.md#local-commands)
implemented by the coordinator (see
[Commands vs. Other Protocol Patterns](commands.md#commands-vs-other-protocol-patterns)).

One-way updates (`void` return type, with no completion blocks) are the norm for
mutator methods. Passing completion callbacks to a mutator or returning mutation
results directly is almost always an anti-pattern: the
[Mutation-Observation Cycle](#the-mutation-observation-cycle) (or a
[command](commands.md) dispatched by the mediator) achieves the same result
while maintaining a much clearer separation of layers.

Similarly, mutator protocols must not be used for the UI to ask questions about
current model state (such as adding an `-isFooEnabled` getter to a mutator).
Mediators push state to their consumers rather than having the UI pull it on
demand. Because the consumer protocol is part of the UI layer, the way a view
controller establishes that it needs to know whether `Foo` is enabled is by
declaring a `-setFooEnabled:` method on its consumer protocol, which the
mediator calls during initial configuration and whenever that state changes.
Transient decisions needed only at the moment of a user interaction are the one
exception, and they belong on a consumer data source (see
[Interaction-Time Queries](#interaction-time-queries)).

Non-`void` mutator methods should be reserved for very narrow cases where an
underlying system API requires an immediate synchronous return value on the same
call stack (for example, forwarding a `UITextFieldDelegate` character-change
check that the model evaluates synchronously).

Consider a download toolbar that allows users to pause or cancel a download:

```objectivec
// download_toolbar_mutator.h (in ui/)
@protocol DownloadToolbarMutator <NSObject>

// Pauses the download.
- (void)pauseDownload;

// Cancels the download.
- (void)cancelDownload;

@end
```

The mediator conforms to `DownloadToolbarDataSource` and
`DownloadToolbarMutator` and holds a weak reference to
`id<DownloadToolbarConsumer>`:

```objectivec
// download_toolbar_mediator.h (in coordinator/)
@interface DownloadToolbarMediator
    : NSObject <DownloadToolbarDataSource, DownloadToolbarMutator>

@property(nonatomic, weak) id<DownloadToolbarConsumer> consumer;

- (instancetype)initWithDownloadTask:(DownloadTask*)task
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

- (void)disconnect;

@end
```

The view controller conforms to `DownloadToolbarConsumer` and exposes weak
properties for its `dataSource` and `mutator`:

```objectivec
// download_toolbar_view_controller.h (in ui/)
@interface DownloadToolbarViewController
    : UIViewController <DownloadToolbarConsumer>

@property(nonatomic, weak) id<DownloadToolbarDataSource> dataSource;
@property(nonatomic, weak) id<DownloadToolbarMutator> mutator;

@end
```

The coordinator wires the mediator and view controller together in `-start`:

```objectivec
// download_toolbar_coordinator.mm (in coordinator/)
@implementation DownloadToolbarCoordinator {
  DownloadTask* _downloadTask;  // Passed to the coordinator's initializer.
  DownloadToolbarViewController* _viewController;
  DownloadToolbarMediator* _mediator;
}

- (void)start {
  _viewController = [[DownloadToolbarViewController alloc] init];
  _mediator =
      [[DownloadToolbarMediator alloc] initWithDownloadTask:_downloadTask];
  _mediator.consumer = _viewController;
  _viewController.dataSource = _mediator;
  _viewController.mutator = _mediator;
}

@end
```

### Keeping Data Source and Mutator Protocols Separate

Even when `dataSource` and `mutator` point to the same mediator instance, **the
consumer data source protocol and the mutator protocol must remain distinct**:

- The **data source protocol** defines how the UI pulls interaction-time
  decisions and is declared in its own header file (`<feature>_data_source.h`).
- The **mutator protocol** defines how the UI requests changes to the model and
  is declared in its own header file (`<feature>_mutator.h`). Read-only
  configuration getters do not belong on a mutator protocol; push configuration
  via the consumer protocol.

Keeping these interfaces separate preserves flexibility: for example, mutations
can be routed to a separate helper object without affecting how the UI queries
the mediator.

### The Mutation-Observation Cycle

When the view controller invokes a mutator method, the mediator updates the
underlying model, observes the resulting model change notification, and pushes a
corresponding update back to the view controller over the consumer protocol. The
view controller updates its views only in response to that consumer call.

This is the **Mutation-Observation Cycle**, and it is the default pattern for
all user-initiated model changes because it guarantees that the UI always
reflects the authoritative state of the model.

For example, when a user taps the pause button on a download toolbar:

**Avoid anticipating model changes in the UI:**

1. Handle the button tap in the view controller.
2. Immediately switch the toolbar to its paused appearance.
3. Call `[_mutator pauseDownload]`.
4. Ignore the subsequent consumer update from the mediator.

**Prefer the Mutation-Observation Cycle:**

1. Handle the button tap in the view controller.
2. Call `[_mutator pauseDownload]`.
3. In the mediator, pause the download and observe the state change.
4. Receive `-[DownloadToolbarConsumer setDownloadState:]` with
   `DownloadToolbarState::kPaused` in the view controller.
5. Switch the toolbar to its paused appearance.

If the model processes updates synchronously, step 4 occurs on the same call
stack before the mutator call in step 2 returns. If the model updates
asynchronously, step 4 occurs on a later turn of the main runloop. Even when an
asynchronous model operation is slow, the mediator—not the view
controller—should drive intermediate UI states (such as progress indicators) via
explicit consumer methods.

Putting these pieces together, a mediator implementation typically looks like
this:

```objectivec
// download_toolbar_mediator.mm (in coordinator/)
@interface DownloadToolbarMediator () <DownloadTaskObserving>
@end

@implementation DownloadToolbarMediator {
  DownloadTask* _task;
  std::unique_ptr<DownloadTaskObserverBridge> _observerBridge;
  std::unique_ptr<base::ScopedObservation<DownloadTask, DownloadTaskObserver>>
      _scopedObservation;
}

- (instancetype)initWithDownloadTask:(DownloadTask*)task {
  if ((self = [super init])) {
    _task = task;
    _observerBridge = std::make_unique<DownloadTaskObserverBridge>(self);
    _scopedObservation = std::make_unique<
        base::ScopedObservation<DownloadTask, DownloadTaskObserver>>(
        _observerBridge.get());
    _scopedObservation->Observe(_task);
  }
  return self;
}

- (void)disconnect {
  _scopedObservation.reset();
  _observerBridge.reset();
  _task = nullptr;
}

- (void)setConsumer:(id<DownloadToolbarConsumer>)consumer {
  _consumer = consumer;
  [self pushDownloadStatus];
}

#pragma mark - DownloadToolbarMutator

- (void)pauseDownload {
  // Mutate the model; do not call `_consumer` directly here.
  _task->Pause();
}

- (void)cancelDownload {
  _task->Cancel();
}

#pragma mark - DownloadTaskObserving

- (void)downloadTaskDidUpdate:(DownloadTask*)task {
  [self pushDownloadStatus];
}

#pragma mark - Private

- (void)pushDownloadStatus {
  [_consumer setProgress:_task->GetProgress()];
  [_consumer setReceivedBytes:_task->GetReceivedBytes()];
  [_consumer setDownloadState:ToolbarStateFromTaskState(_task->GetState())];
}

@end
```

______________________________________________________________________

## Mediators Accessing Other Services and Commands

A mediator's role is strictly to mediate between the model layer and a specific,
active UI feature. **Side effects that are unrelated to a feature's UI do not
belong in a mediator.** For example:

- A mediator that observes a `WebState` to update a loading progress bar should
  not also log general page-load metrics.
- Observing system or `UIApplication` lifecycle events to update a keyed
  service—with no feature UI or consumer involved—is model or application
  infrastructure logic, not UI mediation.

Non-UI logic belongs in a model-layer component such as a keyed service, tab
helper, or browser agent.

However, a mediator for an active UI feature may legitimately depend on
additional services or [command handlers](commands.md) when that feature
requires them:

- **Additional model services:** A feature's business logic may span multiple
  related services. For example, a mediator that closes tabs may need both the
  `WebStateList` and the tab restore service. The coordinator should pass all
  required services into the mediator at initialization (or via properties),
  never passing a `Browser` or `ProfileIOS` container.

- **[Command handlers](commands.md#type-safety-and-command-handlers):**
  Sometimes a model update while a feature is active requires triggering a
  broader application or feature UI change that depends on model state unknown
  to the view controller. (By contrast, if a button tap unconditionally opens
  Settings, the view controller should invoke its own command handler directly
  without involving the mediator.) For example, if closing the last tab in a
  group while the tab group UI is open must dismiss that presentation or open
  another UI surface, the mediator is the component that knows the last tab was
  closed. In such cases, the coordinator should inject the appropriate command
  handler into the mediator (see
  [Type Safety and Command Handlers](commands.md#type-safety-and-command-handlers)
  and [Naming Command Handlers](commands.md#naming-command-handlers)).

  Note that a mediator only dispatches commands while its own UI feature is
  already active. **Do not create a
  [dormant coordinator or mediator](coordinators.md#parent-ownership-of-lifecycle)
  in the background to observe a model service and dispatch commands (especially
  not a command to start its own feature).** Background model observation that
  triggers UI commands when a feature is not active belongs in a model-layer
  observer (such as a browser agent or tab helper), which dispatches a command
  handled by a parent coordinator that then creates and starts the child
  feature's coordinator.

- **Outcomes of UI flows:** When a mediator triggers a flow that presents UI
  (such as sign-in, a sign-out confirmation, or reauthentication), it does so by
  invoking a command; the coordinator that handles the command runs the flow.
  The mediator should learn the flow's outcome by **observing the model**—for
  example, a completed sign-in is visible to the mediator as a primary account
  change on the `IdentityManager` it already observes. Some outcomes are not
  reflected in the model, such as the user cancelling a confirmation; in those
  cases, the coordinator that ran the flow reports the outcome by calling the
  mediator's public interface (for example, `-signOutFlowDidEnd`), since the
  coordinator owns the mediator. When a model action must wait until a UI
  transition finishes (for example, dismissing a menu before switching
  profiles), the command may take a completion block, under the
  [last-resort exception](commands.md#characteristics-of-a-ui-command) for
  completion blocks.

- **Avoid mediator delegates:** Mediators should generally **not** define a
  delegate protocol (especially one implemented by their owning coordinator). A
  coordinator's role is to set up and tear down its feature and manage
  navigation via commands, not to execute ongoing business logic or state
  management on behalf of its mediator. If a mediator needs to trigger a UI
  transition or dismissal in response to model state, inject a command handler
  (for either a
  [shared command protocol or a feature-local command protocol](commands.md#shared-vs-local-command-protocols))
  into the mediator rather than delegating back to the coordinator. This applies
  to parent-agnostic coordinators too: when such a coordinator must notify
  whichever parent started it, that notification uses a
  [coordinator delegate protocol](coordinators.md#coordinator-delegates),
  reached from the mediator through a local command.

______________________________________________________________________

## Composing Mediators and Consumers

The standard mediator pattern pairs a single mediator with a single feature view
controller over feature-specific `Consumer` and `Mutator` protocols. In
practice, features frequently adapt this boundary to fit their UI structure
without violating the core separation between model and UI:

### Multiple Mediators or Multiple Consumers

- **Multiple mediators for a single view controller:** When a single UI surface
  interacts with multiple distinct model domains, its coordinator can create a
  separate mediator for each domain rather than combining unrelated model logic
  into a monolithic mediator. The view controller conforms to each mediator's
  consumer protocol (for example,
  `<DownloadToolbarConsumer, StorageSpaceConsumer>`) and exposes descriptively
  named mutator or data source properties (`downloadMutator`, `storageMutator`)
  wired up by the coordinator in `-start`.
- **Multiple consumers for a single mediator:** Conversely, when a feature's UI
  is decomposed into multiple coordinated view controllers (such as a main
  content view controller and a separate toolbar or header view controller) that
  are driven by the same model domain, a single mediator can expose multiple
  descriptively named weak consumer properties (such as `resultConsumer` and
  `toolbarConsumer`) and push relevant state updates to each surface.

### One-Directional Mediators

A mediator only needs the protocols required by the actual direction of data
flow in the feature:

- **Read-only UIs (Consumer only):** If the user interface only displays model
  state (or only triggers coordinator-level navigation commands without mutating
  feature models), the mediator only pushes to a `Consumer` or provides a
  `DataSource`, and does not need a `Mutator` protocol.
- **Static prompts and action-only flows (Mutator only):** If a feature's UI is
  configured once by the coordinator at presentation time and never changes in
  response to ongoing model observations while visible, the mediator may only
  conform to a `Mutator` protocol to handle the user's actions on the model,
  with no `Consumer` protocol needed.

### Mediators for Unusual UI Implementations

Not every UI surface is a `UIViewController` that accepts imperative method
calls over an Objective-C consumer protocol, for example:

- **SwiftUI views:** In SwiftUI features, views bind declaratively to an
  `ObservableObject` (or `@Observable`) model class defined in `ui/` with
  `@Published` properties. Instead of calling methods on an `id<FooConsumer>`
  protocol, the mediator holds a reference to that UI-layer observable model
  object and updates its properties on the main sequence as C++ model events
  arrive.
- **Composite container modules:** In composite UI surfaces (such as the Magic
  Stack on the New Tab Page), individual module mediators may populate and
  update a UI-layer configuration or item object (e.g., a `MagicStackModule`
  subclass) that is handed to the parent container, rather than driving a
  dedicated view controller directly.

Regardless of how the UI layer is implemented, the architectural pattern remains
the same: the mediator translates between the model layer and a decoupled
UI-layer interface or data object in `ui/` that contains only UI-ready data
types and has no knowledge of the mediator or model services.

______________________________________________________________________

## Guidelines Summary

- **Single-feature focus:** Keep mediators focused on a single model-to-UI
  relationship. Use multiple mediators when a feature bridges multiple distinct
  models.
- **Reserve the name "Mediator":** Only name a class `*Mediator` if it mediates
  between the model layer and the UI layer as described in this document. Do not
  use the name for general coordinator helpers or model-layer classes.
- **No backbone containers:** Never pass `Browser`, `ProfileIOS`, `SceneState`,
  `ProfileState`, `AppState`, `ApplicationContext`, or `CommandDispatcher` into
  a mediator, and never call `GetApplicationContext()` inside a mediator. Pass
  all (and only) the specific services, model objects, and command handlers the
  mediator needs.
- **Observe `WebStateList` directly:** Mediators that track the active tab
  should observe the `WebStateList` (or an active `WebState` observation
  forwarder) rather than having the active `WebState` repeatedly re-injected.
- **Gatekeep model data:** Mediators are the sole gatekeepers of model data for
  the UI. Never pass model objects across a consumer protocol; convert them into
  simple UI-ready value types in the mediator.
- **Push displayed state; pull only transient decisions:** Any state that
  remains visible in the UI is pushed over the consumer protocol, even if only
  once. A view controller may synchronously pull from a consumer data source
  only for decisions used immediately at the moment of an interaction, and such
  methods return decisions or configuration rather than raw model facts.
- **Consumer and Mutator protocols are not UIKit delegates:**
  - UIKit delegates and data sources are tightly coupled to the delegating
    object and take the delegating object (`self`) as their first parameter.
  - Consumer, consumer data source, and mutator protocols are decoupled by
    design. Never include the concrete calling object (such as
    `ReadingListMediator*` or `DownloadToolbarViewController*`) as a UIKit-style
    sender parameter in consumer, data source, or mutator methods.
  - Do not name consumer or mutator protocols `*Delegate`, `*ActionHandler`, or
    `*Commands`.
- **Deterministic teardown:** When a mediator requires deterministic
  cleanup—such as unregistering observers or clearing references to unowned
  model-layer objects—provide a `-disconnect` method and call it from the
  coordinator's `-stop` method rather than relying on `-dealloc`.
- **Unit testing:** Because mediators have narrow, injected dependencies and
  communicate with the UI exclusively through lightweight protocols, they are
  designed to be easily unit-tested. Every mediator should have a comprehensive
  unit test suite.

______________________________________________________________________

## Glossary

- **Business Logic**: The rules, transformations, and coordination that
  translate raw model state into user-facing presentation state and translate
  user intent into model mutations.
- **Consumer (`*Consumer`)**: An Objective-C protocol that is part of the UI
  layer and implemented by a view controller (or other UI-layer object) through
  which a mediator pushes model-driven state updates and UI configuration.
- **Consumer Data Source (`*DataSource`)**: An Objective-C protocol that is part
  of the UI layer and implemented by a mediator, allowing a consumer to obtain
  transient interaction-time decisions on demand.
- **Mediator (`*Mediator`)**: A coordinator-layer object created and owned by a
  [coordinator](coordinators.md) that observes and mutates model-layer objects
  and communicates with the UI layer over decoupled protocols.
- **Mutation-Observation Cycle**: The unidirectional data flow in which a view
  controller sends a user's intent to its mediator via a mutator protocol, the
  mediator updates the model, and the view controller updates its views only
  when the mediator observes the resulting model change and pushes it to the
  consumer.
- **Mutator (`*Mutator`)**: An Objective-C protocol that is part of the UI layer
  and implemented by a mediator, allowing a view controller to request actions
  that depend on the model—such as changing model state or triggering UI changes
  that require model data—in response to user actions.
- **Observer Bridge**: A C++ helper class (typically managed with
  `base::ScopedObservation`) that implements a C++ model observer interface and
  forwards observation callbacks to Objective-C methods on a mediator.
- **State Independence**: The design principle that each consumer update is
  self-contained and does not rely on the consumer's internal state being
  implicitly synchronized with prior model state.

______________________________________________________________________

## Connecting Patterns

See [Mediator](README.md#mediator) in the architecture overview for how
mediators relate to the other patterns.

*Motivated by [Layers](README.md#layers),
[Decoupling](README.md#problem-decomposition),
[Deep Model](README.md#deep-model),
[Isolated View Controller](README.md#isolated-view-controller),
[Coordinator](coordinators.md), and
[Mutation-Observation Cycle](#the-mutation-observation-cycle).*

*Motivates [State-Independent Consumer](#state-independence).*
