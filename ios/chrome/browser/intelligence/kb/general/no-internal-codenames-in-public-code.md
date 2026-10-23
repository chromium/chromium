---
name: ios/no-internal-codenames-in-public-code
description: When writing identifiers, command handlers, action selectors, tests, UI text, or comments, use public canonical feature names instead of internal team names or legacy codenames
rubric: readability
severity: should_fix
---

**Rule.** Do not introduce new code identifiers, UI strings, comments, or
documentation derived from Google-internal team names, project codenames (such
as FooCodename), or confidential nicknames. Refer to official public product and
feature names (such as Gemini), localized string resources (via l10n_util or GRD
IDs), or descriptive technical terminology instead. Legacy classes, factories,
or protocols that still carry a codename or acronym pending refactoring (such as
`FooCodenameService` or `FooCodenameCommands`) keep their names, but new
variables, properties, and parameter labels referring to them use the canonical
public feature name (e.g., geminiService, geminiHandler). Additionally, in iOS
components, name action selectors after the canonical feature (e.g.,
@selector(didTapGeminiButton:) rather than @selector(didTapFooCodenameButton:)),
name command handler variables retrieved via HandlerForProtocol after the
controlled feature (e.g., id<FooCodenameCommands> geminiHandler =
HandlerForProtocol(...)), and align companion unit test cases (e.g.,
GeminiButtonTapped rather than FooCodenameButtonTapped) with canonical feature
terminology.

**Why.** Chromium is an open-source project with external contributors and
downstream embedders; internal team names and project codenames are opaque,
unsearchable, and inappropriate for public UI or codebase interfaces. Using
official public feature names and localized strings prevents confusing cognitive
mismatches between features and their dispatchers, ensures consistent
terminology across coordinators, mediators, UI components, action selectors,
unit test suites, and documentation, and stops transitional or legacy codenames
from proliferating into new public interfaces and variable names.

## Examples

Bad:

```cpp
// Bad: Internal team name in UI text, codename in comments, and codename parameter labels.
label.text = @"FooCodename Prototyping";
// Only add Explain with Gemini if FooCodename is enabled.
_mediator = [[AppBarMediator alloc]
    initWithPrefService:prefs
    fooCodenameService:FooCodenameServiceFactory::GetForProfile(profile)];

// Bad: Stale acronyms for command handlers, action selectors, and unit test cases.
id<FooCodenameCommands> fooCodenameHandler = HandlerForProtocol(
    self.browser->GetCommandDispatcher(), FooCodenameCommands);
[_geminiButton addTarget:self
                  action:@selector(didTapFooCodenameButton:)
        forControlEvents:UIControlEventTouchUpInside];
TEST_F(PageActionMenuViewControllerTest, FooCodenameButtonTapped) { ... }
```

Good:

```cpp
// Good: Localized strings, canonical names in comments, and 'gemini' parameter labels.
label.text = l10n_util::GetNSString(IDS_IOS_AI_PROTOTYPING_HEADER);
// Only add Explain with Gemini if Gemini for this user is enabled.
_mediator = [[AppBarMediator alloc]
    initWithPrefService:prefs
         geminiService:FooCodenameServiceFactory::GetForProfile(profile)];

// Good: Canonical feature names for command handlers, selectors, and test cases.
id<FooCodenameCommands> geminiHandler = HandlerForProtocol(
    self.browser->GetCommandDispatcher(), FooCodenameCommands);
[_geminiButton addTarget:self
                  action:@selector(didTapGeminiButton:)
        forControlEvents:UIControlEventTouchUpInside];
TEST_F(PageActionMenuViewControllerTest, GeminiButtonTapped) { ... }
```
