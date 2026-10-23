---
name: ios/extract-repeated-test-setup-into-fixture-helpers
description: In unit tests, centralize invariant initialization in fixture SetUp() or constructors and encapsulate repetitive or scenario-specific setup in dedicated fixture helper methods
rubric: design
severity: should_fix
---

**Rule.** In unit tests using fixtures (`PlatformTest` or `testing::Test`),
avoid duplicating multi-line initialization or state configuration inline across
test cases:

1. **Invariant shared setup**: Extract shared service initialization, test
   environment configuration, and invariant test double creation (such as
   `FakePasskeyClient` or tracing initialization) into the fixture constructor
   or `SetUp()` method.
2. **Repetitive or scenario-specific setup**: Encapsulate multi-step setup
   routines—such as configuring search engine providers (`TemplateURLData`),
   assembling fake contexts (e.g., `CreateNavigationContext(...)`), stubbing
   mock responses, or serializing protobuf metadata—into reusable fixture helper
   or factory methods (or reuse existing fixture helpers like
   `AddZeroStateSuggestionsHint(...)`).
3. **Dependent state encapsulation**: When collaborator state or feature
   availability depends directly on a setup condition (e.g., DSE configuration
   driving Lens availability), encapsulate that dependent state within the
   fixture setup helper (such as `SetGoogleDSE()` / `SetNonGoogleDSE()`) rather
   than querying or toggling state dynamically inside test bodies, allowing
   tests to assert deterministic outcomes (`EXPECT_TRUE`, `EXPECT_FALSE`).

**Why.** Duplicating initialization routines, protobuf serialization, or mock
setup across individual `TEST_F` test bodies adds visual clutter, obscures the
specific scenario under test, and increases maintenance churn when dependency
schemas change. Centralizing invariant environment setup in `SetUp()` ensures
consistent state across tests. Factoring scenario variations and dependent
collaborator states into parameterized fixture helpers keeps test cases concise,
minimizes boilerplate, and allows tests to verify clear, deterministic
expectations.

## Examples

Bad:

```cpp
// Bad: Invariant setup duplicated, boilerplate inlined, dependent state queried dynamically.
TEST_F(PasskeyTest, UserVerification) {
  PasskeyTabHelper::CreateForWebState(web_state_, std::make_unique<FakePasskeyClient>());
  // ...
}
TEST_F(SearchTest, NonGoogleDSE) {
  TemplateURLData data;
  data.SetURL("https://nongoogle.com/?q={searchTerms}");
  template_url_service_->SetUserSelectedDefaultSearchProvider(
      template_url_service_->Add(std::make_unique<TemplateURL>(data)));
}
TEST_F(ComposeTest, ShowsControls) {
  SetGoogleDSE();
  EXPECT_EQ([consumer_ showsControls:kLens], CheckLensAvailability(true));
}
```

Good:

```cpp
// Good: Invariant setup in SetUp(); variations and dependent state in helpers.
void SetUp() override {
  PlatformTest::SetUp();
  PasskeyTabHelper::CreateForWebState(web_state_, std::make_unique<FakePasskeyClient>());
}
void SetNonGoogleDSE() {
  TemplateURLData data;
  data.SetURL("https://nongoogle.com/?q={searchTerms}");
  template_url_service_->SetUserSelectedDefaultSearchProvider(
      template_url_service_->Add(std::make_unique<TemplateURL>(data)));
  SetLensAvailable(false);
}
void SetGoogleDSE() {
  template_url_service_->SetUserSelectedDefaultSearchProvider(google_template_url_);
  SetLensAvailable(true);
}
TEST_F(SearchTest, NonGoogleDSE) {
  SetNonGoogleDSE();
}
TEST_F(ComposeTest, ShowsControls) {
  SetGoogleDSE();
  EXPECT_TRUE([consumer_ showsControls:kLens]);
}
```
