---
name: ios/document-file-scope-helpers-returning-abstract-types
description: Document implementation-file helpers, anonymous-namespace functions, private interfaces, and test fixtures with their purpose, parameter nullability, concrete return types, and fallback contracts.
rubric: readability
severity: nit
citations:
  - path: docs/documentation_guidelines.md
    heading: Implementation documentation
    quote: Code should be as self documenting as possible, but sometimes that isn't enough. Maybe there's a tricky bit of implementation that isn't obvious. Maybe there's a clever algorithm that has a good reason it needs to be clever. A good rule of thumb is that if a code reviewer had a clarifying question about something during review and the code couldn't be simplified to answer the question then a comment is probably appropriate.
  - path: styleguide/c++/c++.md
    heading: Chromium C++ style guide
    quote: Chromium follows the Google C++ Style Guide unless an exception is listed below.
  - path: styleguide/objective-c/objective-c.md
    heading: Delineate interface implementations with `#pragma mark -`
    quote: Remember that any method which isn't declared in an interface needs a full method comment.
  - path: styleguide/objective-c/objective-c.md
    heading: Disambiguating Symbols
    quote: Where needed to avoid ambiguity, use backticks to quote variable names and symbols in comments in preference to using quotation marks or naming the symbols inline.
  - path: docs/documentation_guidelines.md
    heading: Interface / class documentation
    quote: Non-trivial classes or sets of function declarations in header files should include comments with a brief description of the purpose of the class or set of functions as well as examples of expected usage.
  - path: styleguide/objective-c/objective-c.md
    heading: Objective-C++ style matches the language
    quote: For C functions and constants defined in a namespace, use C++ style, even if most of the file is Objective-C.
  - path: styleguide/objective-c/objective-c.md
    heading: Nullability annotations in the `ios/` directory
    quote: Nullability annotations, such as `nonnull` and `nullable`, may not be used in the `ios/` directory. This differs from the Google Objective-C Style Guide, which permits these annotations.
  - path: styleguide/objective-c/objective-c.md
    heading: Chromium Objective-C and Objective-C++ style guide
    quote: Chromium follows the Google Objective-C style guide unless an exception is listed below.
---

**Rule.** Precede anonymous-namespace free functions and test fixture helpers
(such as `Create*()`, `Build*()`, or `SetUp*()`) in implementation files with
doc comments describing their purpose and return contract (omitting comments
only when strictly trivial and self-explanatory). In particular:

- When returning an abstract base interface (such as
  `std::unique_ptr<KeyedService>` or `scoped_refptr<RefcountedKeyedService>`),
  explicitly name the concrete constructed class in the comment.
- When functions handle nullable inputs or have fallback/sentinel return
  branches, document the edge-case return behavior (such as returning `false`,
  `0`, `nullptr`, `nil`, `std::nullopt`, or empty).
- For convenience overloads forwarding default arguments or fixture member
  variables (such as `member_`), explicitly document the default values
  supplied.
- For test fixture helpers returning hardcoded constants, enums, or
  platform-specific defaults, explain the rationale behind the chosen value,
  test scope, and future extensibility. Additionally, for helpers bridging
  runtime boundaries (such as executing injected scripts via
  `ExecuteJavaScript`), document the execution mechanism, expected return
  schema, and failure assertions.

**Why.** Anonymous-namespace helper and mapping functions and test fixture
utilities lack public header declarations where contracts and API semantics
typically reside. Without explicit doc comments, maintainers are forced to
reverse-engineer implementation bodies to deduce which concrete type hides
behind an abstract return type, which sentinel value signals a missing input,
which defaults an overload forwards, or which test setup policy a fixture helper
encodes. Clearly documenting fallback return values (like `0` or `nullptr`),
forwarded defaults, concrete constructed types under abstract interfaces, and
runtime bridge mechanisms (such as JavaScript execution) prevents null
dereferences, clarifies test scope and platform constraints, and maintains clear
interface contracts across translation units.

## Examples

Bad:

```cpp
namespace {
// Bad: Missing doc comment naming the concrete class and explaining the
// return/null contract.
scoped_refptr<RefcountedKeyedService> BuildInstance(ProfileIOS* profile) {
  return base::MakeRefCounted<IOSChromePasswordCheckManager>(profile->GetPrefs());
}
int64_t GetNavigationId(web::NavigationContext* context) {
  return context ? context->GetNavigationId() : 0;
}
}  // namespace

// Bad: Fixture overload silently forwards a default.
std::unique_ptr<FooService> FooServiceTest::CreateService() {
  return CreateService(/*is_incognito=*/false);
}
```

Good:

```cpp
namespace {
// Factory returning an `IOSChromePasswordCheckManager` instance for `profile`.
scoped_refptr<RefcountedKeyedService> BuildInstance(ProfileIOS* profile) {
  return base::MakeRefCounted<IOSChromePasswordCheckManager>(profile->GetPrefs());
}
// Returns the navigation ID for `context`, or 0 if `context` is null.
int64_t GetNavigationId(web::NavigationContext* context) {
  return context ? context->GetNavigationId() : 0;
}
}  // namespace

// Creates a service for a regular (non-incognito) profile.
std::unique_ptr<FooService> FooServiceTest::CreateService() {
  return CreateService(/*is_incognito=*/false);
}
```
