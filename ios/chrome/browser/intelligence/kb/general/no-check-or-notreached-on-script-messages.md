---
name: ios/no-check-or-notreached-on-script-messages
description: When validating untrusted JavaScript messages in ScriptMessageReceived, do not use value_or defaults or CHECK/NOTREACHED; drop bad input gracefully
rubric: safety
severity: blocking
citations:
  - path: ios/web/public/js_messaging/README.md
    heading: Sanitize and validate messages
    quote: Do not assume that any part of the message is formatted in a particular way or is of a particular type. Instead, validate the format of each part of the message.
  - path: ios/web/public/js_messaging/README.md
    heading: Safely handle known-bad input
    quote: When validating a message, don’t use CHECK. We do not want the input validation mechanism to become an easy way for malicious JavaScript to kill the browser process. It is usually better to ignore the bad input, or when possible, to immediately destroy and re-create the WKWebView that sent invalid input.
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
