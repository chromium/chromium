---
name: ios/capitalize-all-words-in-pascal-case-names
description: When naming GoogleTest tests or PascalCase C++ functions, methods, and classes, capitalize each constituent word, treat acronyms as words, and avoid all-caps tokens
rubric: readability
severity: nit
citations:
  - path: styleguide/c++/c++.md
    heading: Chromium C++ style guide
    quote: Chromium follows the Google C++ Style Guide unless an exception is listed below.
  - path: styleguide/objective-c/objective-c.md
    heading: Objective-C++ style matches the language
    quote: For code in an `@implementation` block, use the Objective-C naming rules. For code in a method of a C++ class, use the C++ naming rules.
---

**Rule.** In GoogleTest test definitions (e.g., `TEST`, `TEST_F`, `TEST_P`),
PascalCase C++ functions, methods, and type declarations, capitalize the first
letter of every constituent non-acronym word—including short prepositions,
conjunctions, and verbs (`On`, `In`, `To`, `For`, `With`, `By`, `From`, `And`,
`Or`, `Is`) and the second half of compound words (`DataSource`, not
`Datasource`; `WebState`, not `Webstate`). Leave existing acronym and initialism
casing untouched (`URL`, `DSE`, `NTP`, `Id`): acronym casing is owned by
`ios/use-cpp-naming-in-objective-cpp`, so do not request `URL` -> `Url` or `Url`
-> `URL` renames under this rule. Do not copy enum constants or macros verbatim
as all-caps tokens into test names (e.g., `PHYSICAL_`), as all-caps prefixes in
GoogleTest are reserved for framework controls (`DISABLED_`, `MAYBE_`).

**Why.** Per the Google C++ Style Guide and GoogleTest conventions, PascalCase
identifiers begin every constituent word with an uppercase letter. Lowercasing a
preposition, verb, or the second half of a compound word hides word boundaries
(`DeactivatesonMatch`, `LoadDatafromDisk`) and produces inconsistent names for
the same concept (`Datasource` vs. `DataSource`), while verbatim all-caps enum
or macro tokens in test names collide with GoogleTest's reserved framework
control prefixes (such as `DISABLED_` and `MAYBE_`). Acronym casing is
deliberately left to the surrounding code because `//ios` C++ mixes
`URLLoadingBrowserAgent`-style and `Id`-style spellings, so renaming acronyms
under this rule would be noisy and is handled by the naming rule that owns it.

## Examples

Bad:

```cpp
// Bad: Lowercased verbs, prepositions, or compound-word halves in names.
TEST_F(QuickDeleteUtilTest, TestDSEStateWhenDSEisGoogle) {
  // ...
}

TEST_F(URLLoadingBrowserAgentTest, InterceptorDeactivatesonMatch) {
  // ...
}

TEST_F(ClickToolJavascriptTest, PHYSICAL_SingleClick_OnButton) {
  // ...
}

void LoadDatafromDisk();
void RegisterDatasourceForProfile(ProfileIOS* profile);
```

Good:

```cpp
// Good: Every constituent word capitalized; existing acronym casing left alone.
TEST_F(QuickDeleteUtilTest, TestDSEStateWhenDSEIsGoogle) {
  // ...
}

TEST_F(URLLoadingBrowserAgentTest, InterceptorDeactivatesOnMatch) {
  // ...
}

TEST_F(ClickToolJavascriptTest, PhysicalPixels_SingleClick_OnButton) {
  // ...
}

void LoadDataFromDisk();
void RegisterDataSourceForProfile(ProfileIOS* profile);
```
