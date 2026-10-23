---
name: ios/mediator-pushes-state-to-consumer-no-polling
description: In iOS Coordinator-Mediator architectures, push state to view controllers via consumer protocols instead of pulling via delegate getters/providers or passing via initializers/properties
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

**Rule.** In Chrome iOS Coordinator-Mediator-Consumer architectures, push
model-derived state (policy checks, validation limits, active options, menus)
from mediators to view controllers via dedicated consumer protocols
(`<FooConsumer>`), and consume pushed state directly in views. Do not declare
getter methods on view controller delegates (`<FooViewControllerDelegate>`),
provider protocols, or authorizer interfaces to pull state on demand; a view
controller may pull only transient, interaction-time decisions from a consumer
data source. Follow the mutation-observation cycle: when a mutator method
changes the model, do not call the consumer directly from the mutator; let the
model change notification reach the mediator's observer, then push the resulting
UI-ready state to the consumer. Any state that can change while the UI is
visible must be pushed through the consumer protocol (even if only once) rather
than passed into `UIViewController` subclasses via custom initializers (e.g.,
`-initWithInputState:`) or public state properties; configuring a view
controller once at presentation time with data that cannot change while it is
visible (a static prompt or action-only flow) is acceptable. When a view
controller reports a user action that depends on UI state, pass the stored UI
state as an argument rather than having the coordinator query the mediator.

**Why.** In the Coordinator-Mediator-Consumer pattern, state flows
unidirectionally from the model, through the mediator, to passive view
controllers, and the UI changes only when the mediator observes the resulting
model change, never in anticipation of one. Pushing to the consumer directly
from a mutator method bypasses the model as the single source of truth and lets
the UI drift when the mutation fails, completes asynchronously, or is also
triggered elsewhere. Passing changeable state into view controllers via
initializers or public properties bypasses mediator-driven data flow and fails
to handle runtime updates, while having views pull model, policy, option, or
menu state on demand through provider protocols, delegate getters, or
coordinator round-trips couples presentation layers to backend data retrieval
and risks acting on stale state. Pushing state via `<FooConsumer>` ensures
consistent, synchronized UI updates while keeping view controllers decoupled
from state management. See `ios/architecture/mediators.md` ("The
Mutation-Observation Cycle") and `ios/architecture/mediators.md`
("Interaction-Time Queries").

## Examples

Bad:

```cpp
// Anti-pattern 1: View controller takes changeable state in an initializer or
// public property.
@interface ComposeboxMenuViewController : UIViewController
- (instancetype)initWithInputState:(ComposeboxUIInputState*)inputState;
@property(nonatomic, strong) ComposeboxUIInputState* inputState;
@end
_viewController = [[ComposeboxMenuViewController alloc] initWithInputState:state];

// Anti-pattern 2: View pulls state or menus via delegate getters or providers.
@protocol ComposeboxInputPlateViewControllerDelegate
- (ComposeboxModelOption)activeModelOption;
@end
@protocol AppBarMenusProvider
- (UIMenu*)menuForButtonOfType:(AppBarButtonType)buttonType;
@end

// Anti-pattern 3: Coordinator queries the mediator for UI state on a delegate
// callback.
- (void)composeboxViewControllerDidTapPlusButton:
    (ComposeboxViewController*)viewController {
  [self startMenuWithInputState:[_mediator currentUIInputState]];
}

// Anti-pattern 4: Mutator pushes to the consumer directly, anticipating the
// model change instead of observing it.
- (void)saveEntityInstance:(autofill::EntityInstance)entityInstance {
  _entityDataManager->AddOrUpdateEntityInstance(entityInstance);
  [self.consumer setEntityNames:[self entityNamesIncluding:entityInstance]];
}
```

Good:

```cpp
// Pattern 1: View controller implements the consumer; the mediator pushes state.
@protocol ComposeboxMenuConsumer <NSObject>
- (void)setUIInputState:(ComposeboxUIInputState*)state;
- (void)setModelOption:(ComposeboxModelOption)modelOption;
- (void)setMenu:(UIMenu*)menu forButtonType:(AppBarButtonType)buttonType;
@end
@interface ComposeboxMenuViewController : UIViewController <ComposeboxMenuConsumer>
@end
_viewController = [[ComposeboxMenuViewController alloc] init];
_mediator.consumer = _viewController;

// Pattern 2: Mutation-observation cycle: the mutator only mutates the model;
// the observer callback pushes UI-ready values to the consumer.
- (void)saveEntityInstance:(autofill::EntityInstance)entityInstance {
  // Mutate the model; do not call `_consumer` directly here.
  _entityDataManager->AddOrUpdateEntityInstance(entityInstance);
}

// EntityDataManagerObserving
- (void)entityInstancesDidChange {
  [self.consumer setEntityNames:[self entityNamesFromModel]];
}

// Pattern 3: View controller passes its stored UI state in the callback.
[self.delegate composeboxViewController:self
    didTapPlusButtonWithUIInputState:_state];
```
