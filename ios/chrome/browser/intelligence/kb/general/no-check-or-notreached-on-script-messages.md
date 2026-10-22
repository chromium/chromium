---
name: ios/no-check-or-notreached-on-script-messages
rubric: safety
severity: blocking
---

**Rule.** Do not use `.value_or(...)` default fallbacks for required message
keys, and do not use `NOTREACHED()`, `CHECK()`, or `DCHECK()` on unexpected or
unrecognized message data in `JavaScriptFeature::ScriptMessageReceived` or
WebKit script message handlers. Validate that required dictionary keys are
present and correctly typed, and return early to drop or ignore malformed
messages gracefully without terminating the browser process.

**Why.** Messages from JavaScript WebContent processes represent untrusted input
from potentially compromised renderers. Using `.value_or(...)` defaults for
expected fields allows malformed messages to execute under unintended default
assumptions, while asserting invariants with `NOTREACHED()` or `CHECK()` allows
malicious web content to trivially crash the browser process (denial of
service).

## Examples

Bad:

```cpp
// Bad: Using default fallback for required keys allows malformed messages
// to proceed under default assumptions.
bool is_settled = script_dict->FindBool("settled").value_or(false);

// Bad: Crashing the browser via `NOTREACHED()` or `CHECK()` on unexpected
// input.
const std::string* api = script_dict->FindString("api");
if (!api || *api != kExpectedApi) {
  NOTREACHED();
}
```

Good:

```cpp
// Good: Validate required keys are present and typed; drop malformed input.
std::optional<bool> is_settled = script_dict->FindBool("settled");
if (!is_settled) {
  return;
}

// Good: Safely ignore unexpected or unrecognized values without crashing.
const std::string* api = script_dict->FindString("api");
if (!api || *api != kExpectedApi) {
  return;
}
```
