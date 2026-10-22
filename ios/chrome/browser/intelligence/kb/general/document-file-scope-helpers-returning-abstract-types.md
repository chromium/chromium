---
name: ios/document-file-scope-helpers-returning-abstract-types
rubric: readability
severity: nit
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
