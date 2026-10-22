---
name: ios/avoid-tautological-flag-descriptions
rubric: readability
severity: should_fix
---

**Rule.** When authoring flag descriptions for `chrome://flags` in flag
description files (e.g., `flag_descriptions.h` or `*_flag_descriptions.cc`),
describe user-visible functionality, interactions, and concrete UI context
(conventionally starting with 'When enabled, ...') rather than tautologically
repeating the flag name or stating generic phrases. Avoid internal engineering,
rollout, or architecture jargon (such as 'dark launch', 'backend no-op', or
internal framework names like 'ExperienceKit'); for infrastructure flags without
user-facing changes, explain the capability enabled and explicitly note '(no
user-visible effect)' when applicable. Additionally, explicitly document any
prerequisite flags, capabilities, or dependencies required for the feature to
function. Format all descriptions as complete grammatical sentences ending with
a period.

**Why.** `chrome://flags` descriptions are surfaced directly to end users,
external testers, QA, and developers to explain the real-world effect of
toggling a feature. Repeating the flag name, using internal rollout jargon, or
referencing internal framework and architecture names obscures what the feature
actually does. Clearly explaining UI context and documenting prerequisite flags
or capabilities prevents confusion when toggling a flag fails to produce the
expected behavior, while terminal punctuation maintains grammatical consistency
across UI copy.

## Examples

Bad:

```cpp
// Bad: Tautological, lacks UI context, and misses terminal period.
inline constexpr char kDefaultBrowserPictureInPictureDescription[] =
    "When enabled, the promo will be displayed with picture in picture";

// Bad: Uses internal framework/rollout jargon ('ExperienceKit', 'dark launch').
const char kParentalControlsDescription[] =
    "Merges parental controls in pref store (dark launch of no-op feature).";

// Bad: Omits prerequisite capability and controlling flag.
const char kBuildExternalPrivacyContextDescription[] =
    "When enabled, checks if the account can be signed in on the device.";
```

Good:

```cpp
// Good: Concrete UI context, grammatically complete sentence with period.
inline constexpr char kDefaultBrowserPictureInPictureDescription[] =
    "When enabled, default browser instructions are shown in "
    "picture-in-picture format over iOS settings.";

// Good: Plain language explaining effect, noting '(no user-visible effect)'.
const char kParentalControlsDescription[] =
    "Merges device parental controls with Family Link settings in the "
    "SupervisedUserPrefStore (no user-visible effect).";

// Good: Explicitly documents prerequisite capability and controlling flag.
const char kBuildExternalPrivacyContextDescription[] =
    "When enabled, checks if the account can be signed in. Requires "
    "`can_sign_in_to_chrome` capability (controlled by "
    "kEnforceCanSignInToChromeCapability).";
```
