---
name: ios/name-ui-interaction-handlers-after-user-actions
description: 'When declaring UI interaction handlers, action selectors, or delegate callbacks, use semantic actions, didTap..., or ...Tapped: rather than handle..., tapped..., didPress..., or state verbs'
rubric: readability
severity: should_fix
---

**Rule.** Name UI interaction handler methods, target-action selectors, and
delegate callbacks after the user action, intent, or specific UI element tapped
(e.g., `exitEditMode`, `promoCloseButtonTapped`, `didTapScanCardButton`,
`didTapNavigationBarDoneButton`, `didTapDeselectAll`, or
`pageActionMenuTapped:`). Use standard iOS touch terminology with did-phrasing
(`didTap...`) or `<control>Tapped:` rather than redundant `handle...` prefixes
on action handlers (e.g., `handlePageActionMenuTapped:`), past-participle
prefixes (`tapped...` like `tappedDeselectAll`), physical press naming
(`didPress...`), ambiguous state verbs (`promoClosed`), visual glyphs
(`didTapNavigationBarCheckmarkButton`), or generic button roles
(`didTapOnPrimaryButton`). A `handle...` name is still appropriate for a method
that processes a generic result or payload rather than a user action (e.g.,
`-handleResult:`). When a view reports a tap through a `*Delegate` protocol,
pass the delegating object as the first parameter
(`-paymentsScanViewDidTapScanCardButton:`); when the tap requests a model change
or a UI transition, prefer a `*Mutator` or local `*Commands` method named after
the intent over a generic delegate. Ensure method names in implementation files
match the exact selector declared in the corresponding header or protocol.

**Why.** State and completion verbs obscure whether an event was triggered by
user interaction, timeouts, or programmatic cleanup. Touch terminology
(`didTap...` or `...Tapped:`) aligns with Cocoa and Chromium iOS conventions
over redundant `handle...` prefixes, past-participle prefixes (`tapped...`), or
physical press phrasing (`didPress...`), while naming semantic intent or
specific elements avoids coupling interfaces to transient visual styling, button
roles, or premature action assumptions. Passing the delegating object first
keeps delegate callbacks consistent with UIKit delegate grammar, and routing
taps through mutators or command handlers keeps UI logic out of coordinators.

## Examples

Bad:

```cpp
// Ambiguous state verb conflates user action with programmatic dismissal:
- (void)promoClosed {
  [self endPromoSession];
}

// Redundant 'handle...' prefix or past-participle prefix 'tapped...':
- (void)tappedDeselectAll {
  [self deselectAllRows];
}
[_menuButton addTarget:self
                action:@selector(handleMenuButtonTapped:)
      forControlEvents:UIControlEventTouchUpInside];

// Generic button roles, missing sender, and physical press naming in delegates:
@protocol PaymentsScanViewDelegate
- (void)didTapOnPrimaryButton;
@end
- (void)tabGroupHeaderDidPressEditionButton:(TabGroupHeader*)header;
```

Good:

```cpp
// Explicit user action clarifies the trigger:
- (void)promoCloseButtonTapped {
  [self endPromoSession];
}

// Idiomatic didTap... prefix or ...Tapped: target-action selector:
- (void)didTapDeselectAll {
  [self deselectAllRows];
}
[_menuButton addTarget:self
                action:@selector(menuButtonTapped:)
      forControlEvents:UIControlEventTouchUpInside];

// Delegate callbacks name the specific element and pass the delegating object
// as the first parameter:
@protocol PaymentsScanViewDelegate
- (void)paymentsScanViewDidTapScanCardButton:(PaymentsScanView*)view;
@end
- (void)tabGroupHeaderDidTapTitle:(TabGroupHeader*)header;

// Better when the tap starts a UI transition: the view controller's action
// handler invokes a local command named after the intent:
- (void)scanCardButtonTapped:(UIButton*)sender {
  [self.paymentsScanHandler showCardScanner];
}
```
