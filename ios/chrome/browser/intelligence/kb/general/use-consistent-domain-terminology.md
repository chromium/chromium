---
name: ios/use-consistent-domain-terminology
description: When defining types, methods, parameters, variables, configuration fields, or test cases, use consistent domain and specification terminology rather than ad-hoc descriptions or synonyms
rubric: readability
severity: should_fix
---

**Rule.** When designing or extending types, methods, enums, parameters,
properties, variables, configuration fields, or test cases, use established
subsystem domain and specification terminology rather than introducing divergent
synonyms or ad-hoc descriptions. Name variables after domain semantics (e.g.,
`password_data_ref`) rather than generic API prefixes (e.g., `sec_data_ref`),
keep naming consistent across counterpart operations and paired parameters,
align cache properties with consumer variables, name local aliases after
concrete domain properties (e.g., `clickabilityReasons` over `reasons`), ensure
configuration flags reflect concrete concepts (e.g., `large_entrypoint` over
`show_message`), and align test cases and parameter identifiers with terms
established in design documents or specifications (e.g.,
`ChoiceScreenEverywhere` over `ChoiceScreenEnabledForAllSurfaces`).

**Why.** Generic names derived from system APIs or ad-hoc descriptions obscure
underlying domain semantics, break architectural consistency, and make test
failures harder to correlate with specifications. Using established,
self-documenting domain and specification terminology across types, parameters,
variables, configuration flags, and test cases ensures clarity and improves
codebase searchability.

## Examples

Bad:

```cpp
// Bad: Generic system API prefix obscures domain semantics.
CFDataRef sec_data_ref = base::apple::GetValueFromDictionary<CFDataRef>(
    result_dict, kSecValueData);

// Bad: Mixing synonymous 'Entrypoint' with established 'Surface' terminology.
if (!search_engine_choice_service_->IsSurfaceEligible(is_first_run_entrypoint)) {
  return search_engines::SearchEngineChoiceScreenConditions::kIneligibleSurface;
}

// Bad: Inconsistent naming across paired method parameters.
+ (instancetype)startupParametersForCommand:(NSString*)command
                      fromSourceApplication:(NSString*)appID
                fromSecureSourceApplication:(NSString*)secureSourceApp;

// Bad: Divergent synonym between cache property and consumer variable.
@property(nonatomic, assign) CGFloat rootFontSize;
CGFloat documentFontSize = self.styleCache.rootFontSize;

// Bad: Ambiguous generic alias when accessing a domain property collection.
NSMutableArray<NSString*>* reasons =
    [interactionInfo.clickabilityReasons mutableCopy];
[reasons addObject:kClickableControlReason];

// Bad: Generic description in field name and comment obscures subsystem concept.
bool entrypoint_always_show_message = false;

// Bad: Ad-hoc test parameter name ignores design specification terminology.
INSTANTIATE_TEST_SUITE_P(
    , SearchEngineChoiceUtilShouldDisplayChoiceScreenTest,
    ::testing::ValuesIn<ChoiceScreenTestParam>({
        {.test_name = "ChoiceScreenEnabledForAllSurfaces_FirstRunSurface",
         .expected_result = true},
    }));
```

Good:

```cpp
// Good: Named after domain concept, matching counterpart read/write operations.
CFDataRef password_data_ref = base::apple::GetValueFromDictionary<CFDataRef>(
    result_dict, kSecValueData);

// Good: Consistent 'Surface' domain terminology across methods and arguments.
if (!search_engine_choice_service_->IsSurfaceEligible(is_first_run_surface)) {
  return search_engines::SearchEngineChoiceScreenConditions::kIneligibleSurface;
}

// Good: Consistent naming across paired method parameters.
+ (instancetype)startupParametersForCommand:(NSString*)command
                      fromSourceApplication:(NSString*)appID
                fromSecureSourceApplication:(NSString*)secureAppID;

// Good: Consistent 'document' domain terminology on both property and consumer.
@property(nonatomic, assign) CGFloat documentFontSize;
CGFloat documentFontSize = self.styleCache.documentFontSize;

// Good: Matching descriptive domain name for local property alias.
NSMutableArray<NSString*>* clickabilityReasons =
    [interactionInfo.clickabilityReasons mutableCopy];
[clickabilityReasons addObject:kClickableControlReason];

// Good: Aligns configuration field and comment with 'large entrypoint' concept.
bool entrypoint_message_large_entrypoint_always_shown = false;

// Good: Aligns test case name and parameter with design document terminology.
INSTANTIATE_TEST_SUITE_P(
    , SearchEngineChoiceUtilShouldDisplayChoiceScreenTest,
    ::testing::ValuesIn<ChoiceScreenTestParam>({
        {.test_name = "ChoiceScreenEverywhere_FirstRunSurface",
         .expected_should_display_choice_screen_output = true},
    }));
```
