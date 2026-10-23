---
name: ios/hook-up-coordinator-presentation-methods
description: When introducing, updating, or refactoring methods, accessors, coordinator routines, helpers, stub handlers, or test utilities, wire them to active callers or delete uncalled code
rubric: readability
severity: should_fix
citations:
  - path: docs/cl_tips.md
    heading: Encapsulate complexity, but don’t over-abstract
    quote: However, do not over-design abstractions for an unknown future. Allowing for extensibility when it’s not necessary, creating abstractions where something concrete would suffice, or reaching for a design pattern when something simpler would work equally well adds unnecessary complexity to the codebase.
  - path: agents/projects/code-health/hub/references/automated_review.md
    heading: Shared Automated Review Protocol > Step by Step Instructions > Functionality & Verification (CRITICAL)
    quote: Ensure any unused methods, variables, or imports resulting from the change are also removed. Check callers, headers, and tests for completeness.
  - path: agents/projects/code-health/hub/references/automated_review.md
    heading: Review Prompt
    quote: Ensure any unused methods, variables, or imports resulting from the change are also removed.
---

**Rule.** When introducing, updating, or refactoring methods, accessors,
property overrides, helper routines, coordinator presentation methods, stub
protocol commands, or test utilities (such as EarlGrey `AppInterface` endpoints,
matchers like `GREYMatcher`, or test fixtures and support files), ensure they
are actively wired to callers, consumed, and tested, or required by framework
contracts. Do not introduce speculative, uncalled, or anticipatory functions,
methods, accessors, types, utility constants, empty event handlers, or stubs
ahead of actual use; land them only alongside active callers. When splitting a
feature across stacked changes, start from the integration point in existing
code and build inward (for example, land the coordinator wired into its parent
before its view controller and mediator) instead of landing precursor APIs that
nothing calls yet. Additionally, when refactoring, removing call sites, or
adding feature support, do not modernize unused methods or leave prototyping
leftovers behind. Verify active callers and delete orphaned or unreferenced
methods, test utilities, and test accessors (such as `*ForTesting()` or
`*_for_testing()` methods) along with their companion header declarations rather
than retaining dead code that escapes compiler diagnostics.

**Why.** In Objective-C and C++ class definitions, compiler diagnostics rarely
flag unreferenced member methods, property overrides, test endpoints, or
accessors due to dynamic dispatch, external linkage, or out-of-line test
exposure. Uncalled presentation methods or property setters cause broken feature
routing or silent dead ends. Introducing helpers, accessors, stubs, matchers, or
test utilities in advance of their call sites adds dead, unexercised code,
clutters public headers and test interfaces, and increases review burden without
caller context. Furthermore, updating or retaining orphaned accessors, dead
`*_for_testing` methods, prototyping leftovers, or obsolete routines bloats
binaries, increases maintenance overhead, and misleads maintainers about active
capabilities.

## Examples

Bad:

```cpp
// Variant 1: Presentation method defined but never wired into routing logic:
// In sign_in_flow_coordinator.mm:
- (void)showAccountMenu {
  [_accountMenuCoordinator start];
}
// In sign_in_flow_mediator.mm:
- (void)signInDidComplete {
  // Bug: nothing ever invokes -showAccountMenu.
  [self.signInFlowHandler startSession];
}

// Variant 2: Speculative C++ accessor or helper added ahead of callers:
class AutofillSaveDialogDelegate {
 public:
  const AccountInfo& account() const { return account_; }
  ResultCallback TakeCallback() { return std::move(callback_); }
};

// Variant 3: Uncalled EarlGrey AppInterface helper or stub ahead of consumers:
@interface AutofillAppInterface : NSObject
+ (BOOL)isAutofillUndoEnabled;  // No test callers in the change.
@end
- (void)cancelPasskeyRequest:(RequestInfo)info {
  // TODO: Stubbed out ahead of integration.
}

// Variant 4: Retaining dead test accessor after removing its last caller:
// In address_data_manager.h (call site migrated to test_api):
AlternativeStateNameMapUpdater*
get_alternative_state_name_map_updater_for_testing() {
  return alternative_state_name_map_updater_.get();
}
```

Good:

```cpp
// Variant 1: The mediator makes the decision and dispatches a local command
// (SignInFlowCommands, declared in ui/) that the coordinator implements:
// In sign_in_flow_mediator.mm:
- (void)signInDidComplete {
  if ([self isAccountSwitchable]) {
    [self.signInFlowHandler showAccountMenu];
    return;
  }
  [self.signInFlowHandler startSession];
}
// In sign_in_flow_coordinator.mm:
#pragma mark - SignInFlowCommands
- (void)showAccountMenu {
  [_accountMenuCoordinator start];
}

// Variant 2: Only expose accessors and helpers actively consumed in the change:
class AutofillSaveDialogDelegate {
 public:
  SaveDialogParams ExtractParams() { return std::move(params_); }
};

// Variant 3: Land test APIs and protocol handlers alongside active callers:
@interface AutofillAppInterface : NSObject
+ (void)clearProfilePasswordStore;  // Actively called by tests in this change.
@end
- (void)cancelPasskeyRequest:(RequestInfo)info {
  [self.passkeyCoordinator dismissPasskeyBottomSheetForRequest:info];
}

// Variant 4: Delete dead test accessors when their last call site is eliminated:
// (Removed get_alternative_state_name_map_updater_for_testing() from header
// and implementation rather than modernizing or retaining dead code.)
```
