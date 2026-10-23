---
name: ios/document-non-obvious-function-parameters
description: When declaring functions, constructors, methods, protocols, or callbacks with non-obvious or formatted parameters, document purpose, expected values, conditions, effects, units, and formats
rubric: readability
severity: should_fix
citations:
  - path: docs/documentation_best_practices.md
    heading: Method and class comments
    quote: This documentation is **the contract of how your code must behave**. The intended audience is future programmers who will use and modify your code. It is often reasonable to say that any behavior documented here should have a test verifying it. This documentation details what arguments the method takes, what it returns, any "gotchas" or restrictions, and what exceptions it can throw or errors it can return.
  - path: docs/documentation_guidelines.md
    heading: Interface / class documentation
    quote: Non-trivial classes or sets of function declarations in header files should include comments with a brief description of the purpose of the class or set of functions as well as examples of expected usage.
  - path: styleguide/c++/c++.md
    heading: Chromium C++ style guide
    quote: Chromium follows the Google C++ Style Guide unless an exception is listed below.
  - path: styleguide/objective-c/objective-c.md
    heading: Disambiguating Symbols
    quote: Where needed to avoid ambiguity, use backticks to quote variable names and symbols in comments in preference to using quotation marks or naming the symbols inline.
---

**Rule.** In declarations for functions, constructors, methods, callbacks,
Objective-C protocols, interfaces, and parameter structs, document any parameter
or struct field whose semantic role, expected values, conditions, effects,
units, coordinate spaces, bounds, or formats are not self-evident from type and
name alone.

- For broad types (such as `GURL`, `std::string`, `std::string_view`, or
  `base::FilePath`) or generic identifiers/tokens/tags (e.g.,
  `application_tag`), clarify the expected values, scope, and semantic
  distinction (such as distinguishing a page URL from a downloaded content URL,
  or specifying browser-level constants like `kBrowserLevelApplicationTag`
  versus profile names).
- For enum or mode parameters affecting return values or error codes (such as
  `policy_source`), explain how values branch execution; when reusing standard
  enums with counterintuitive semantics (such as `GREYDirection` for
  non-deflected swipe actions), explain how each value alters behavior.
- For booleans, describe what the flag represents and under what conditions it
  is set or true (e.g., `userInitiated` or autofill state).
- For numeric values or geometry, specify units, coordinate systems (such as
  unit coordinate space `[0, 1]`), and valid bounds or count meanings (e.g.,
  successful vs. failed counts).
- For serialized or structured strings (e.g., Base64 proto payloads, serialized
  JSON, or text fragments), document delimiter prefixes (e.g., omitting the
  `text=` prefix), encoding schemas, and relevant specification links (such as
  WICG spec URLs), avoiding vague qualifiers like 'internal'.
- Place explanatory comments directly above the parameter declaration in the
  header, in preceding doc comments, or—for complex Objective-C protocols and
  interface methods with sub-flow branching, metrics access points (such as
  `signin_metrics::AccessPoint`), or completion callbacks—use bulleted lines
  with backtick-quoted parameter names (e.g., `// - \`param\`: description\`).

**Why.** Declaration comments serve as the public contract for callers and
implementers. Generic parameter names and broad types (like `std::string`,
`GURL`, or raw numbers) leave valid input formats, coordinate spaces, units, and
caller contexts ambiguous. Omitting parameter purpose, policy semantics, boolean
conditions, delimiter rules, or lifecycle expectations—or leaving reused enums
with non-obvious semantics unexplained—leads to caller misuse, passing the wrong
resource or context, unhandled error cases, coordinate mismatches, and confusion
during maintenance. Clear comments, including backtick-quoted bulleted
descriptions for complex method arguments, eliminate ambiguity and adhere to
style guidelines for symbol disambiguation.

## Examples

Bad:

```cpp
// Variant 1: Ambiguous parameter semantic role and expected values
explicit ContentAnalysisInfo(const GURL& url, AnalysisSettings settings);
std::unique_ptr<PrivateKeyFactory> CreatePrivateKeyFactory(
    std::string_view application_tag);

// Variant 2: Undocumented count units and structured fragment format
void TapInfoButtonForInvalidPasswords(int imported, int failed);
virtual void SetInternalScrollToTextFragment(
    const std::optional<std::string>& fragment) = 0;

// Variant 3: Unexplained protocol parameters and access points
- (void)startGeminiEntryFlowWithStartupState:(GeminiStartupState*)startupState
                                 accessPoint:
                                     (signin_metrics::AccessPoint)accessPoint
                                  completion:(GeminiEntryFlowCompletion)completion;
```

Good:

```cpp
// Variant 1: Documented parameter roles and expected calling values
explicit ContentAnalysisInfo(
    // The URL containing the contents for the analysis request.
    const GURL& url,
    AnalysisSettings settings);
// For browser-level usage, pass `kBrowserLevelApplicationTag`; for
// profile-level usage, pass the profile name.
std::unique_ptr<PrivateKeyFactory> CreatePrivateKeyFactory(
    std::string_view application_tag);

// Variant 2: Documented count meanings and explicit delimiter rules
/// `imported` is successful count; `failed` is failed count.
void TapInfoButtonForInvalidPasswords(int imported, int failed);
// The text fragment to scroll to (omits "text=" prefix; see WICG spec).
virtual void SetInternalScrollToTextFragment(
    const std::optional<std::string>& fragment) = 0;

// Variant 3: Bulleted descriptions disambiguating protocol parameters
// - `startupState`: Entry configuration for the session.
// - `accessPoint`: Metrics source; used only when signing in signed-out users.
// - `completion`: Called with the final outcome; may be nil.
- (void)startGeminiEntryFlowWithStartupState:(GeminiStartupState*)startupState
                                 accessPoint:
                                     (signin_metrics::AccessPoint)accessPoint
                                  completion:(GeminiEntryFlowCompletion)completion;
```
