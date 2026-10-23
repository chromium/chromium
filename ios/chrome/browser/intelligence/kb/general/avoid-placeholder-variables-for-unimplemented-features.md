---
name: ios/avoid-placeholder-variables-for-unimplemented-features
description: When authoring feature or preparatory CLs, avoid speculative branching, placeholder variables, uncalled helpers, and unready UI models; implement only supported behavior directly
rubric: readability
severity: should_fix
citations:
  - path: docs/cl_tips.md
    heading: Encapsulate complexity, but don’t over-abstract
    quote: However, do not over-design abstractions for an unknown future. Allowing for extensibility when it’s not necessary, creating abstractions where something concrete would suffice, or reaching for a design pattern when something simpler would work equally well adds unnecessary complexity to the codebase. The codebase is inherently mutable and additional abstractions can be added _if and when_ they are needed.
  - path: docs/ios/style.md
    heading: TODO comments
    quote: Every TODO must have a bug
---

**Rule.** Do not introduce speculative or anticipatory code—such as unused
fallback branches, speculative multi-case branching, unready feature-flag
checks, dead placeholder variables (e.g., `BOOL flag = NO;`), incomplete UI
models with empty placeholder strings, uncalled helpers, injected dependencies
or services that nothing consumes yet, or empty conditional blocks containing
only TODOs—for features deferred or not yet finalized. Implement only currently
supported behavior directly; leave unready optional UI submodels `nil` (or
omitted) with an attributed `// TODO(crbug.com/<id>)`. Defer preparatory logic,
helpers, and dependency injection to the change that consumes them; when a
feature is split across stacked changes, start from the integration point in
existing code and build inward so that no change submits dead code.

**Why.** Speculative branches, unready feature flags, placeholder variables,
incomplete UI models, uncalled helpers, and dependencies staged ahead of their
use bloat change scope, introduce dead code, and confuse reviewers who cannot
verify unexercised paths in isolation. Instantiating incomplete UI models with
empty placeholder strings can also produce broken or blank visual states if
triggered. Keeping implementations minimal and scoped to finalized designs
ensures every change is reviewable and testable, and ordering stacked changes
from the integration point inward keeps each one exercised by the code it lands
in.

## Examples

Bad:

```cpp
// Variant 1: Speculative branching and unready feature flags for unfinalized mocks.
switch ([self.dataSource logoType]) {
  case kGoogleWalletLogo:
    return MakeSymbolMulticolor(SymbolWithPointSize(
        base::FeatureList::IsEnabled(features::kWalletBranding)
            ? SymbolGoogleWallet : SymbolGooglePay, kHeight));
  default:
    NOTREACHED();
}

// Variant 2: Placeholder variables, dead conditions, or unready UI models.
BOOL enableNewWorkflow = NO;
if (enableNewWorkflow && [self hasActiveSession]) { ... }
// Bad: Empty placeholder text in incomplete UI model.
entryPoint.footerItem = [[ContentEntryPointUnavailabilityItem alloc]
    initWithText:@"" andIcon:enterpriseIcon andActionIdentifier:nil];

// Variant 3: Anticipatory fallback branches or empty blocks containing only TODOs.
if (_verifier) {
  [_verifier loadRiskData:std::move(callback)];
} else {
  std::move(callback).Run([_delegate loadRiskData]);
}
if (_shouldGetSnapshot && !_hasScreenshot) {
  // TODO(crbug.com/123456): Block failure once snackbar is fixed.
}

// Variant 4: Uncalled helpers, unused headers, or dependencies staged ahead of
// the change that uses them (a TODO does not make them acceptable).
#import "components/autofill/core/browser/autofill_format_string.h"
autofill::AutofillFormatString GetAttributeFormatString() { ... }
// TODO(crbug.com/465421681): Use `_templateURLService` to determine default engine.
_templateURLService = templateURLService;
```

Good:

```cpp
// Variant 1: Directly provide the single supported asset without speculative branching.
return MakeSymbolMulticolor(SymbolWithPointSize(SymbolGooglePay, kHeight));

// Variant 2: Omit placeholder variables and leave unready UI models nil with a TODO.
// TODO(crbug.com/123456): Enable new workflow once session helpers land.
// TODO(crbug.com/485297147): Add footer item when footer UI is implemented.
entryPoint.footerItem = nil;

// Variant 3: Implement only the active branch (the callback is always invoked)
// and place the TODO in the enclosing scope.
CHECK(_verifier);
[_verifier loadRiskData:std::move(callback)];
// TODO(crbug.com/123456): Block failure once snackbar is fixed.

// Variant 4: Nothing to stage here; the helper, its header, and the
// `_templateURLService` injection land in the change that consumes them.
```
