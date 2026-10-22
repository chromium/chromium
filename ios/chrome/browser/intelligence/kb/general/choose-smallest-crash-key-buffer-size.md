---
name: ios/choose-smallest-crash-key-buffer-size
rubric: safety
severity: nit
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
