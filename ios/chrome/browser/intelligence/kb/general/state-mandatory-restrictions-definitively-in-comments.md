---
name: ios/state-mandatory-restrictions-definitively-in-comments
description: When documenting constraints, preconditions, or control flow in comments, state strict invariants definitively while avoiding absolute negative assertions or ambiguous 'may not' for conditional behavior
rubric: readability
severity: nit
citations:
  - path: styleguide/objective-c/objective-c.md
    heading: Nullability annotations in the `ios/` directory
    quote: Nullability annotations, such as `nonnull` and `nullable`, may not be used in the `ios/` directory. This differs from the Google Objective-C Style Guide, which permits these annotations.
---

**Rule.** When documenting platform constraints, preconditions, or unconditional
execution in code comments, state restrictions definitively (e.g., 'cannot
access', 'only re-triggers') rather than using ambiguous phrasing ('are not
supposed to') or hedging qualifiers ('typically', 'usually'). Conversely, when
documenting conditional paths, edge cases, or non-guaranteed behavior, avoid
absolute negative statements (e.g., asserting code 'doesn't run' or 'never
invokes' a callback) and avoid ambiguous 'may not': state mandatory
preconditions as 'must not be nil' or 'cannot', and qualify non-guaranteed
behavior explicitly using 'is not necessarily', 'doesn't necessarily', or 'might
not be', or detail the governing conditions.

**Why.** Ambiguous phrasing like 'not supposed to' obscures whether an
architectural restriction is mandatory, while hedging qualifiers like
'typically' imply non-existent exceptions for unconditional code. Conversely,
asserting absolute negatives when conditional paths or edge cases exist misleads
callers about control flow and lifecycle contracts. Avoiding ambiguous 'may not'
in favor of explicit non-guaranteed phrasing ('is not necessarily', 'doesn't
necessarily') prevents confusion between mandatory prohibition and optionality.

## Examples

Bad:

```cpp
// App extension are not supposed to access -sharedApplication, so fall
// back to using UIScreen's -mainScreen to get the preferred content
// size category.
if (IsRunningInAppExtension()) {
  return [UIScreen mainScreen].traitCollection.preferredContentSizeCategory;
}

// We typically only re-trigger this automatically on iPhone (where it is an
// input view).
if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
  return;
}

// `name` may be nil, `email` may not be nil.
- (void)updateIdentityDiscAccessibilityLabelWithName:(NSString*)name
                                               email:(NSString*)email;

// `target_web_state` may not be the active web state and calling this method
// will not make it active.
virtual void Load(const UrlLoadParams& params,
                  web::WebState* target_web_state);

// Therefore, this function doesn't run the response callback provided from
// `PolicyDecider::ShouldAllowResponse()`.
void HandlePolicyDecision();
```

Good:

```cpp
// App extensions cannot access -sharedApplication, so fall back to using
// UIScreen's -mainScreen to get the preferred content size category.
if (IsRunningInAppExtension()) {
  return [UIScreen mainScreen].traitCollection.preferredContentSizeCategory;
}

// We only re-trigger this automatically on iPhone (where it is an input
// view).
if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
  return;
}

// `name` may be nil, `email` must not be nil.
- (void)updateIdentityDiscAccessibilityLabelWithName:(NSString*)name
                                               email:(NSString*)email;

// `target_web_state` is not necessarily the active web state and calling
// this method will not make it active.
virtual void Load(const UrlLoadParams& params,
                  web::WebState* target_web_state);

// Therefore, this function doesn't necessarily run the response callback
// provided from `PolicyDecider::ShouldAllowResponse()`.
void HandlePolicyDecision();
```
