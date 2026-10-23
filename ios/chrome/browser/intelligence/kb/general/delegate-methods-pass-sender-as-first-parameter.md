---
name: ios/delegate-methods-pass-sender-as-first-parameter
description: When declaring Objective-C delegate/observer protocols or coordinator callbacks, conform to NSObject, qualify selectors with class names and event verbs, and pass the sender or coordinator first
rubric: readability
severity: should_fix
citations:
  - path: styleguide/objective-c/objective-c.md
    heading: Objective-C++ style matches the language
    quote: Within an Objective-C++ source file, follow the style for the language of the function or method you're implementing.
---

**Rule.** When declaring Objective-C delegate or observer protocols (including
coordinator delegate protocols), adhere to standard Cocoa delegate conventions:

1. Protocol definition: Name delegate protocols `<Sender>Delegate` (or
   `<Service>Observing` for observer bridges), declare explicit conformance to
   `<NSObject>`, and document the protocol and methods.
2. First parameter: Pass the delegating sender, observed provider, or initiating
   instance (such as `self`, a coordinator, or a provider) as the first
   parameter (e.g., `(Sender*)sender`). Pass the delegating instance itself
   (`self`) rather than leaking internal UI controls or event senders (such as
   `(id)sender`), and do not omit the initiating instance in favor of passing
   raw state values alone.
3. Coordinator delegates: Coordinator delegate methods use the same grammar as
   any other delegate: the delegating coordinator is the first parameter,
   introduced by the class-name prefix (`-signinCoordinatorDidComplete:` or
   `-signinCoordinator:didCompleteWithResult:`) rather than by a bare noun
   phrase (such as `...Completion:`), and auxiliary parameters (such as results)
   never precede the coordinator.
4. Method naming and qualification: Prefix callback method names with the
   specific delegating class, feature, or service name in lowerCamelCase
   followed by Cocoa event or decision verbs (`did...`, `will...`, `should...`,
   such as `assistantAIMViewControllerDidTapClose:`,
   `lensOverlayConsentPresenterWillDismiss:`, or
   `safetyCheckManagerWillShutdown`). Do not use bare generic class prefixes
   (such as `viewControllerDidTapClose:`) or bare UIKit lifecycle names (such as
   `viewDidDisappear`).
5. Event notifications vs. commands: Do not use imperative command names (such
   as `displayEditionMenu` or `requestDismissal...`) in delegate protocols.
   Reserve command protocols (suffixed with `Commands`, e.g., `*Commands`) for
   imperative action requests, ensuring delegate protocols communicate state
   transitions, lifecycle events, or query decisions.

**Why.** In Objective-C, method dispatch relies entirely on selector names
within a shared global namespace, ignoring parameter types. Qualifying callback
selectors with the specific delegating class or service name prevents selector
collisions when a single coordinator or mediator implements multiple delegate or
observer protocols. Conforming to `<NSObject>` ensures support for standard
runtime introspection methods such as `respondsToSelector:`. Passing the
delegating or initiating object (`self`, coordinator, or provider) as the first
parameter adheres to Cocoa conventions, allowing delegates and observers
managing multiple instances to disambiguate the event source directly and query
current state without redundant parameters, while preventing internal UI control
details from leaking. Leading with the delegating coordinator in the standard
`-fooCoordinator:didFoo:` grammar keeps coordinator delegate selectors
consistent with every other delegate protocol and self-describing at the call
site. Finally, naming methods with Cocoa event or decision verbs (`did...`,
`will...`, `should...`) rather than imperative commands maintains a clear
architectural boundary between event notifications and command dispatch (which
should be isolated in dedicated `*Commands` protocols).

## Examples

Bad:

```cpp
// Imperative command, missing <NSObject>, or colliding with UIKit lifecycle:
@protocol TabGroupEditionDelegate
- (void)displayEditionMenu;
- (void)viewDidDisappear;
@end

// Generic class prefix and leaking internal UI control sender:
@protocol ScannerViewDelegate <NSObject>
- (void)viewControllerDidTapClose:(id)sender;
@end

// Coordinator delegate with bare keyword or trailing coordinator:
@protocol SigninCoordinatorDelegate <NSObject>
- (void)signinCoordinatorCompletion:(SigninCoordinator*)coordinator;
- (void)signinDidCompleteWithResult:(SigninCoordinatorResult)result
                        coordinator:(SigninCoordinator*)coordinator;
@end
```

Good:

```cpp
// Conforms to <NSObject>, prefixed with delegating class, passes sender:
@protocol TabGroupHeaderDelegate <NSObject>
- (void)tabGroupHeaderDidTapTitle:(TabGroupHeader*)header;
- (void)tabGroupHeaderViewDidDisappear:(TabGroupHeader*)header;
@end

// Specific class prefix and passes delegating instance self:
@protocol ScannerViewDelegate <NSObject>
- (void)scannerViewDidTapClose:(ScannerView*)scannerView;
@end
- (void)didTapClose:(id)sender {
  [self.delegate scannerViewDidTapClose:self];
}

// Coordinator delegate in standard delegate grammar, coordinator first:
@protocol SigninCoordinatorDelegate <NSObject>
- (void)signinCoordinatorDidComplete:(SigninCoordinator*)coordinator;
- (void)signinCoordinator:(SigninCoordinator*)coordinator
    didCompleteWithResult:(SigninCoordinatorResult)result;
@end
```
