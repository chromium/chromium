---
name: ios/spell-out-objective-c-parameter-names
description: In Objective-C, Objective-C++, and iOS code, spell out identifiers in descriptive lowerCamelCase reflecting type or role instead of ad-hoc abbreviations, generic suffixes, or truncated names
rubric: readability
severity: should_fix
citations:
  - path: styleguide/objective-c/objective-c.md
    heading: Chromium Objective-C and Objective-C++ style guide
    quote: Chromium follows the Google Objective-C style guide unless an exception is listed below.
  - path: styleguide/objective-c/objective-c.md
    heading: Objective-C++ style matches the language
    quote: For code in an @implementation block, use the Objective-C naming rules.
---

**Rule.** In Objective-C, Objective-C++, and iOS C++ code, spell out identifiers
completely in descriptive lowerCamelCase (or Google C++ style for C++ member
methods) across function, method, property, parameter, instance variable, member
field, and local variable declarations to prioritize clarity and readability
over brevity. Do not use informal or ad-hoc abbreviations (such as `Nav` for
`Navigation`, `Doc` for `Document`, or `midButton` for `middleButton`),
truncated words (such as `openedForUserEd` for `openedForUserEducation`), or
non-standard acronyms and initialisms (such as `bvc` for
`browserViewController`, `dse` for `defaultSearchEngine`, or `rectInWindowIA`
for `rectInWindow`). When naming variables, properties, or fields holding
services, managers, agents, or coordinators, spell out their specific type and
role (such as `geminiBrowserAgent`, `_authenticationService`, or
`_actionAfterReauth`) rather than truncating it (such as `_authService`). The
canonical single-collaborator names (`_mediator`, `_viewController`, `consumer`,
`mutator`, `dataSource`, `<name>Handler`) are not abbreviations and are fine,
and within the narrow scope of a single method parameter or local variable the
short name `handler` is acceptable for a command handler when unambiguous. Match
parameter names in single-argument setters and accessors directly to the
corresponding property or selector fragment, and align member identifiers with
underlying API types and conventions (such as
`web::NavigationContext::GetNavigationId()`). Additionally, when naming a
variable after its class type (such as after an `alloc init` instantiation or a
`base::apple::ObjCCast` / `base::apple::ObjCCastStrict` downcast), strip
subsystem or namespace prefixes but preserve all remaining descriptive nouns; do
not arbitrarily drop intermediate words from the class name (e.g., prefer
`AIEntityItem` over `aiItem` for `AutofillAIEntityItem`; acronyms stay fully
capitalized even at the start of the name, as in `URLString` or `_URL`).
Abbreviations are strictly reserved for universally recognized acronyms and
established vocabulary types (such as `URL`, `ID`, `rect`, or `min`/`max`).

**Why.** Cocoa, Chromium, and Google Objective-C style guidelines prioritize
readability, consistency, and self-documenting code over brevity. Arbitrary
truncations, ad-hoc abbreviations, and cryptic initialisms save negligible
horizontal space while creating ambiguity, increasing cognitive load, and
obscuring underlying types, lifecycles, and roles—especially in coordinators and
mediators interacting with multiple services. Fully spelled-out, descriptive
lowerCamelCase identifiers make component roles immediately clear at call sites,
improve codebase searchability, maintain consistency with underlying APIs and
type hierarchies, and avoid inconsistent naming when downcasting objects with
`base::apple::ObjCCast`.

## Examples

Bad:

```cpp
// Bad: Ad-hoc abbreviations, truncated names, and cryptic acronyms:
int64_t GetNavId(web::NavigationContext* context) {
  return context ? context->GetNavigationId() : 0;
}
@property(nonatomic, assign) BOOL openedForUserEd;
const TemplateURL* dse = service->GetDefaultSearchProvider();
- (void)setBrowserViewController:(UIViewController*)bvc {
  _browserViewController = bvc;
}

// Bad: Truncated names, dropped nouns and lowercased acronym after ObjCCast:
AutofillAIEntityItem* aiItem =
    base::apple::ObjCCast<AutofillAIEntityItem>(item);
raw_ptr<AuthenticationService> _authService;
ActionAfterReauth _action;
```

Good:

```cpp
// Good: Fully spelled-out identifiers matching API conventions:
int64_t GetNavigationId(web::NavigationContext* context) {
  return context ? context->GetNavigationId() : 0;
}
@property(nonatomic, assign) BOOL openedForUserEducation;
const TemplateURL* defaultSearchEngine = service->GetDefaultSearchProvider();
- (void)setBrowserViewController:(UIViewController*)browserViewController {
  _browserViewController = browserViewController;
}

// Good: Descriptive names preserving descriptive nouns and acronym casing:
AutofillAIEntityItem* AIEntityItem =
    base::apple::ObjCCast<AutofillAIEntityItem>(item);
raw_ptr<AuthenticationService> _authenticationService;
ActionAfterReauth _actionAfterReauth;
```
