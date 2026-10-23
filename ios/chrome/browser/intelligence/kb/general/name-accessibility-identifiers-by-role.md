---
name: ios/name-accessibility-identifiers-by-role
description: When defining iOS accessibility identifiers and string constants, reuse shared constants, prefix feature scopes, name semantic roles, and mirror symbols in literals without 'k'
rubric: readability
severity: should_fix
---

**Rule.** When declaring, defining, or referencing UI and accessibility
identifier constants and related named string constants in iOS
(`NSString* const k...`, `const char k...[]`, `std::string_view`, or
`base::cstring_view`):

1. **Reuse canonical shared constants**: Before declaring new identifiers, reuse
   existing canonical constants provided by shared UI components (such as
   `kButtonStackPrimaryActionAccessibilityIdentifier` and
   `kButtonStackSecondaryActionAccessibilityIdentifier` from
   `ios/chrome/common/ui/button_stack/button_stack_constants.h` for controls
   managed by `ButtonStackViewController`) across UI setup and EarlGrey test
   matchers instead of creating redundant feature-specific constants.
2. **Feature prefix without stutter**: Prefix custom constants with the feature
   or screen name (`k<FeatureName>...`), but do not redundantly repeat enclosing
   component, screen, or directory names (e.g., use
   `kGeminiSettings...Identifier`, not `kSettingsGeminiSettings...`). Ensure
   identifiers remain build- and configuration-neutral across branded and
   unbranded builds.
3. **Name by semantic role**: Name identifiers after the element's primary
   semantic role or action (such as `RootTitle`, `SearchBar`, or `SaveButton`)
   rather than concrete view classes (e.g., avoid `ImageView`) or transient
   visual states (such as loading spinners like `SaveOrLoading`).
4. **Match UI label wording, number, and compound titles**: Align the
   identifier's wording and grammatical number (plural vs. singular) with the
   user-visible UI element it represents. When an element represents a compound
   feature or multi-item concept, fully reflect all items (e.g.,
   `PasswordsAndPasskeysIdentifier`) rather than truncating to a single keyword
   (e.g., `PasswordsIdentifier`).
5. **Suffix with `Identifier`**: Append `Identifier` (or
   `AccessibilityIdentifier`) to clearly distinguish identifier constants from
   display strings, configuration values, or preference keys.
6. **Mirror symbol in string literal without 'k'**: For named string constants
   whose symbol names mirror their underlying literal value (such as custom
   views, sheets, custom detents with `customDetentWithIdentifier:`, UI test
   identifiers, preference keys, or notification names), ensure the symbol name
   and string literal value match exactly without omitted terms, missing
   qualifiers, spelling errors, or grammatical number discrepancies, while
   omitting the leading 'k' prefix (e.g., assign `@"FeatureElementIdentifier"`,
   not `@"kFeatureElementIdentifier"`). Avoid generic literals (e.g.,
   `@"card_view"`) and synchronize literals when renaming to prevent stale
   subsystem prefixes.

**Why.** Reusing canonical shared component identifiers (such as ButtonStack
action identifiers) prevents duplicate symbol definitions and avoids EarlGrey
test matcher divergence against the component hierarchy. Prefixing constants
with feature names prevents symbol collisions in the global namespace and groups
related constants in symbol listings and autocomplete, while avoiding prefix
stutter keeps names concise. Naming identifiers by primary semantic role rather
than concrete view classes or transient visual states prevents brittle test
matchers when view hierarchies refactor or controls temporarily display progress
indicators. Aligning identifier wording and grammatical number with user-facing
UI copy—including compound titles—ensures consistent navigation, prevents
mismatch bugs, and preserves searchability across test suites. Suffixing
constant symbols with `Identifier` prevents confusion with preference keys or
user-visible copy. Finally, mirroring the symbol name in the string literal
without the leading `k` variable prefix and ensuring symbol names and literal
values match without omitted qualifiers, spelling errors, or number
discrepancies keeps accessibility identifiers clean and collision-free across
view hierarchies, preserves codebase searchability and cross-referencing, avoids
incorrect test matcher assumptions, and prevents stale subsystem prefixes after
renames.

## Examples

Bad:

```cpp
// BAD: Redundant ButtonStack constant; view class used; prefix stutter:
extern NSString* const kFeaturePrimaryButtonAccessibilityIdentifier;
extern NSString* const kSettingsGeminiSettingsImageView;

// BAD: Truncates compound UI label ("Passwords and Passkeys") or transient state:
extern NSString* const kQuickDeleteOtherDataPasswordIdentifier;
extern NSString* const kAutofillAISaveEntitySaveOrLoadingButtonId;

// BAD: String literal has leading 'k', generic value, or stale prefix:
NSString* const kQuickDeleteManageOtherDataCellIdentifier =
    @"kQuickDeleteManageOtherDataCellIdentifier";
NSString* const kGeminiPromoConsentFullDetentIdentifier =
    @"FooCodenamePromoConsentFullDetentIdentifier";

// BAD: Mismatched grammatical number or omitted qualifiers in string literal:
const char kDownloadedItemIdentifier[] = "DownloadedItemsIdentifier";
NSString* const kQuickDeleteOtherDataPasswordsAndPasskeysIdentifier =
    @"QuickDeleteOtherDataPasswordsIdentifier";
```

Good:

```cpp
// GOOD: Reuse shared component constant; clean feature scope with semantic role:
[button setAccessibilityIdentifier:kButtonStackPrimaryActionAccessibilityIdentifier];
extern NSString* const kGeminiSettingsRootTitleIdentifier;

// GOOD: Matches full compound UI label ("Passwords and Passkeys") and semantic action:
extern NSString* const kQuickDeleteOtherDataPasswordsAndPasskeysIdentifier;
extern NSString* const kAutofillAISaveEntitySaveButtonIdentifier;

// GOOD: String literal mirrors symbol name without 'k' and with current prefix:
NSString* const kQuickDeleteManageOtherDataCellIdentifier =
    @"QuickDeleteManageOtherDataCellIdentifier";
NSString* const kGeminiPromoConsentFullDetentIdentifier =
    @"GeminiPromoConsentFullDetentIdentifier";

// GOOD: Symbol name and literal match exactly without omitted terms or number discrepancies:
const char kDownloadedItemsIdentifier[] = "DownloadedItemsIdentifier";
NSString* const kQuickDeleteOtherDataPasswordsAndPasskeysIdentifier =
    @"QuickDeleteOtherDataPasswordsAndPasskeysIdentifier";
```
