---
name: ios/assert-injected-dependencies-in-constructors
description: When constructors or initializers receive required or feature-gated injected dependencies, assert non-nullness with CHECK upfront rather than deferring to member methods or redundantly re-checking.
rubric: readability
severity: should_fix
citations:
  - path: styleguide/c++/checks.md
    heading: CHECK(), DCHECK() and NOTREACHED()
    quote: CHECK(), DCHECK() and NOTREACHED() are all used to ensure that invariants hold. They document (and verify) programmer expectations that either some statement always holds true at the point of (D)CHECKing or that a piece of code is unreachable (for NOTREACHED).
---

**Rule.** When a class constructor or initializer receives injected
dependencies, required services, delegates, or context pointers (such as
ProfileIOS, SceneState, or WebState) expected to never be null—or dependencies
conditionally required when a feature flag is enabled (via
base::FeatureList::IsEnabled)—validate them with CHECK() directly in the
constructor or initializer body to establish the invariant upfront. Use a bare
`CHECK(cond)` when continuing without the dependency cannot be made safe (such
as when member methods unconditionally dereference the pointer with no recovery
path); when execution can safely recover (for example, by returning `nil` from a
failable initializer or early-returning at use sites), use
`CHECK(cond, base::NotFatalUntil::M<n>)` (with `M<n>` set to the `MAJOR`
milestone in `//chrome/VERSION` plus 3 or 4, never copied from surrounding code)
followed by a graceful fallback. Do not defer dependency assertions or
null-checks to member methods that consume the dependency or to later lifecycle
methods (such as -start). Apart from that fallback, do not rely on defensive
call-site checks (such as if (\_tracker)), and do not redundantly re-check the
invariant with CHECK() inside every member method that accesses the dependency.
When asserting non-nullness of held dependencies at usage sites (such as weakly
held delegates), also assert them in the constructor and document outlive
invariants in comments.

**Why.** Asserting dependency preconditions directly in constructors or
initializers fails fast and produces a crash stack attributing contract
violations directly to the instantiating caller, rather than obscuring the root
cause in later lifecycle or downstream member method calls where initialization
context is lost. When dependencies are conditionally required under a feature
flag, asserting them in the constructor when the feature is enabled establishes
the invariant early. Once established at initialization, repeating assertions
across member methods adds clutter without improving safety, while omitting
initialization checks encourages defensive coding or masks initialization-time
misconfiguration.

## Examples

Bad:

```cpp
// Variant 1: Deferring feature-gated dependency CHECK to member method
UserPolicySigninService::UserPolicySigninService(ProfileIdService* service)
    : profile_id_service_(service) {}

std::string UserPolicySigninService::GetProfileId() {
  if (base::FeatureList::IsEnabled(kCloudProfileReporting)) {
    CHECK(profile_id_service_);  // Should be asserted in constructor.
    return profile_id_service_->GetProfileId().value_or(std::string());
  }
  return std::string();
}

// Variant 2: Redundant member assertion or deferred lifecycle check
ReminderNotificationClient::ReminderNotificationClient(ProfileIOS* profile)
    : PushNotificationClient(PushNotificationClientId::kReminders, profile) {}
bool ReminderNotificationClient::IsPermitted() {
  ProfileIOS* profile = GetProfile();
  CHECK(profile);  // Redundant; should be asserted in constructor.
  return profile->GetPrefs()->GetBoolean(...);
}
- (void)start {
  DCHECK(_sceneState);  // Deferred check; should be CHECK(sceneState) in init.
  [_sceneState activateUI];
}
```

Good:

```cpp
// Variant 1: Feature-gated dependency validated upfront in constructor
UserPolicySigninService::UserPolicySigninService(ProfileIdService* service)
    : profile_id_service_(service) {
  if (base::FeatureList::IsEnabled(kCloudProfileReporting)) {
    CHECK(profile_id_service_);
  }
}
std::string UserPolicySigninService::GetProfileId() {
  if (base::FeatureList::IsEnabled(kCloudProfileReporting)) {
    return profile_id_service_->GetProfileId().value_or(std::string());
  }
  return std::string();
}

// Variant 2: Validated once upfront at initialization; used directly
ReminderNotificationClient::ReminderNotificationClient(ProfileIOS* profile)
    : PushNotificationClient(PushNotificationClientId::kReminders, profile) {
  CHECK(profile);
}
bool ReminderNotificationClient::IsPermitted() {
  return GetProfile()->GetPrefs()->GetBoolean(...);
}
- (instancetype)initWithSceneState:(SceneState*)sceneState {
  CHECK(sceneState);
  if ((self = [super init])) {
    _sceneState = sceneState;
  }
  return self;
}
```
