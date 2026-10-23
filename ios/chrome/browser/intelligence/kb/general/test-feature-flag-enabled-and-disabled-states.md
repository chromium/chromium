---
name: ios/test-feature-flag-enabled-and-disabled-states
description: When testing feature-flagged code, verify both enabled and disabled states using separate tests, parameterized suites, or EarlGrey subclasses rather than testing sequentially or branching dynamically
rubric: testing
severity: should_fix
citations:
  - path: net/docs/code-reviews.md
    heading: New features and behavior changes
    quote: Code whose behavior is influenced by the feature flag should be tested both with and without the flag enabled, using [INSTANTIATE_TEST_SUITE_P](https://google.github.io/googletest/reference/testing.html#INSTANTIATE_TEST_SUITE_P).
---

**Rule.** When introducing or modifying code influenced by a feature flag (such
as factory methods returning nullptr or fallback services when disabled), test
both enabled and disabled states deterministically; do not test only one state,
leaving fallback or enabled paths unexercised. Do not branch dynamically on
feature flags inside test bodies (such as with
`if (base::FeatureList::IsEnabled(...))` or AppInterface queries), and do not
test both states sequentially in a single test case by resetting
`base::test::ScopedFeatureList` mid-test. In unit tests, verify both states
using separate dedicated test cases (`TEST_F` with `InitAndEnableFeature` /
`InitAndDisableFeature`) or value-parameterized test suites (`TEST_P` with
`testing::WithParamInterface<bool>` or `testing::TestWithParam<bool>` and
`INSTANTIATE_TEST_SUITE_P(..., testing::Bool())`). In EarlGrey integration
tests, subclass the test case and configure `config.features_enabled` or
`config.features_disabled` in `-appConfigurationForTestCase`.

**Why.** Testing only one flag state leaves either the new feature logic or
baseline fallback behavior (such as nullptr handling or fallback service
creation) unverified, risking silent regressions, crashes, or broken kill
switches when the flag default flips. Branching dynamically inside a test
executes only whichever branch matches the bot configuration, leaving the
alternate branch unexercised. Testing opposing states sequentially within a
single test case (such as resetting `ScopedFeatureList` mid-test) couples
independent scenarios, risks state leakage across the toggle, and masks
downstream failures if the first assertion fails. Dedicated test cases and
value-parameterized suites ensure clean test isolation and independent failure
reporting, while EarlGrey test case subclasses allow standard CQ builders to
exercise integration tests under both flag configurations.

## Examples

Bad:

```cpp
// 1. Factory method gated by a flag without disabled-state tests:
std::unique_ptr<CertificateProvisioningService>
ControllerIOS::CreateCertificateProvisioningService() {
  if (!features::IsCertificateProvisioningEnabled()) {
    return nullptr;
  }
  return CreateService();
}

// 2. Branching dynamically inside a test or querying AppInterface:
TEST_F(BrowserCoordinatorTest, ShowFolder) {
  if (base::FeatureList::IsEnabled(kDownloadList)) {
    [handler showDownloadsFolder];
  }
}

// 3. Sequentially testing both states in one test via mid-test Reset():
TEST_F(HistoryTabHelperTest, DidFinishNavigationRecords404) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(history::kVisitedLinksOn404);
  EXPECT_NE(url, latest_row_result_.url());

  feature_list.Reset();
  feature_list.InitAndEnableFeature(history::kVisitedLinksOn404);
  EXPECT_EQ(url, latest_row_result_.url());
}
```

Good:

```cpp
// 1. Dedicated test verifying nullptr or fallback return when feature is disabled:
TEST_F(ControllerIOSTest, CreateService_FeatureDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(features::kCertificateProvisioning);
  EXPECT_EQ(delegate.CreateCertificateProvisioningService(), nullptr);
}

// 2. Value-parameterized test suite verifying both enabled and disabled states:
class QueryManagerTest : public PlatformTest,
                         public testing::WithParamInterface<bool> {
 protected:
  QueryManagerTest() {
    feature_list_.InitWithFeatureState(kSomeFeature, GetParam());
  }
  base::test::ScopedFeatureList feature_list_;
};
INSTANTIATE_TEST_SUITE_P(All, QueryManagerTest, testing::Bool());

// 3. Subclass EarlGrey test cases and configure appConfigurationForTestCase:
- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config = [super appConfigurationForTestCase];
  config.features_enabled.push_back(kPasswordRemoval);
  return config;
}
```
