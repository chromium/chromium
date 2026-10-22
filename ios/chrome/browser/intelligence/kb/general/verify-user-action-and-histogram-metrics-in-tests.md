---
name: ios/verify-user-action-and-histogram-metrics-in-tests
rubric: testing
severity: should_fix
---

**Rule.** When testing component workflows, user actions, or lifecycle events
that emit UMA metrics, directly assert actual functional outcomes or state
changes (such as service or model state) rather than relying solely on histogram
or metric emission to prove program correctness. Additionally, when introducing,
modifying, or testing metric emissions, verify telemetry using
base::HistogramTester or MetricsAppInterface across all relevant execution
paths—including initial and subsequent interactions or distinct conditions.
Assert specific bucket counts using ExpectBucketCount or ExpectUniqueSample
(verifying unreached buckets remain zero) rather than omitting metric checks or
relying solely on aggregate counts like ExpectTotalCount.

**Why.** Relying solely on metrics to validate program logic fails to guarantee
that underlying state mutations succeeded and creates fragile tests that lose
test coverage if telemetry is later modified or expires. Conversely, omitting
metric checks, logging only initial events, or checking only aggregate total
counts leaves telemetry unverified and risks silent regressions where metrics
fail to record on subsequent operations, emit duplicate samples, or record to
incorrect buckets.

## Examples

Bad:

```cpp
// Bad: Relies solely on histogram emission to verify logic without asserting state.
EXPECT_EQ(1, histogram_tester.GetBucketCount("Signin.SignoutProfile",
                                             ProfileSignout::kSignoutFromCapability));

// Bad: Tests only aggregate count or initial emission, omitting subsequent metrics.
histogram_tester.ExpectTotalCount("IOSOpenExtensionOutcome", 23);
histogram_tester.ExpectBucketCount(kFirstInteractionHistogram,
                                   InteractionType::kText, 1);
```

Good:

```cpp
// Good: Directly verifies functional state alongside histogram emission.
EXPECT_EQ(1, histogram_tester.GetBucketCount("Signin.SignoutProfile",
                                             ProfileSignout::kSignoutFromCapability));
EXPECT_FALSE(auth_service->HasPrimaryIdentity(signin::ConsentLevel::kSignin));

// Good: Verifies exact bucket counts across all initial and repeated interactions.
histogram_tester.ExpectBucketCount("IOSOpenExtensionOutcome", Outcome::kSuccess, 23);
histogram_tester.ExpectBucketCount("IOSOpenExtensionOutcome", Outcome::kFailure, 0);
histogram_tester.ExpectBucketCount(kFirstInteractionHistogram, InteractionType::kText, 1);
histogram_tester.ExpectBucketCount(kInteractionHistogram, InteractionType::kText, 1);
```
