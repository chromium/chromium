---
name: ios/choose-smallest-crash-key-buffer-size
description: When declaring or logging crash keys with SCOPED_CRASH_KEY_STRING or CrashKeyString, choose the smallest buffer size variant accommodating expected values
rubric: safety
severity: nit
citations:
  - path: docs/debugging_with_crash_keys.md
    heading: High-Level Overview
    quote: Every crash key has an associated value maximum length and a string name to identify it. The maximum length is specified as a template parameter in order to allocate that amount of space for the value up-front.
---

**Rule.** When declaring or logging crash keys (e.g., using
`SCOPED_CRASH_KEY_STRING<N>` or `crash_reporter::CrashKeyString<N>`), select the
smallest size variant (32, 64, 256, or 1024) that accommodates the expected
value length. Do not default to unnecessarily large buffers (such as 256 or 1024
bytes) for short identifiers, status keys, or enum strings.

**Why.** Crash keys pre-allocate static storage matching their maximum size to
record crash diagnostic values safely without dynamic heap allocation. Choosing
oversized buffers wastes static memory and consumes finite minidump annotation
storage.

## Examples

Bad:

```cpp
// Bad: 256 bytes is excessively large for a short key or identifier.
SCOPED_CRASH_KEY_STRING256("WKContentRuleListProvider", "key", key);

static crash_reporter::CrashKeyString<256> rule_id_key("rule-id");
```

Good:

```cpp
// Good: Choose 32 or 64 bytes to accommodate expected short values without waste.
SCOPED_CRASH_KEY_STRING64("WKContentRuleListProvider", "key", key);

static crash_reporter::CrashKeyString<32> rule_id_key("rule-id");
```
