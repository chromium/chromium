---
name: ios/update-web-view-bridge-implementations-for-shared-protocols
description: When modifying or extending shared iOS component bridge protocols, update all conforming embedder classes across both ios/chrome and ios/web_view
rubric: correctness
severity: should_fix
citations:
  - path: ios/web_view/README.md
    quote: 'NOTE: This code is not used by Chrome for iOS (`//ios/chrome`), but is rather a separate product/embedder of the `//ios/web` rendering layer.'
---

**Rule.** When modifying or adding methods to shared iOS component bridge
protocols (such as `AutofillDriverIOSBridge` or password manager driver
bridges), update and implement or forward the new methods across all conforming
embedder classes in both `ios/chrome` and `ios/web_view` (e.g.,
`CWVAutofillController`). Ensure all embedder implementations are updated in the
same change to prevent compilation failures or runtime missing selector crashes.

**Why.** `ios/web_view` is an independent embedder alongside `ios/chrome` that
consumes shared iOS components like Autofill and Password Manager. Adding or
modifying methods on shared Objective-C bridge protocols without updating
conforming classes in `ios/web_view` causes build breaks or runtime unrecognized
selector crashes.

## Examples

Bad:

```cpp
// In `components/autofill/ios/browser/autofill_driver_ios_bridge.h`:
@protocol AutofillDriverIOSBridge <NSObject>
- (void)notifyFormsSeen:(const std::vector<autofill::FormData>&)updatedForms
                inFrame:(web::WebFrame*)frame;
@end

// In `ios/web_view/internal/autofill/cwv_autofill_controller.mm`:
// Conforms to `AutofillDriverIOSBridge`, but missing
// `-notifyFormsSeen:inFrame:` implementation.
```

Good:

```cpp
// In `ios/web_view/internal/autofill/cwv_autofill_controller.mm`:
- (void)notifyFormsSeen:(const std::vector<autofill::FormData>&)updatedForms
                inFrame:(web::WebFrame*)frame {
  [_autofillAgent notifyFormsSeen:updatedForms inFrame:frame];
}
```
