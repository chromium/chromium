---
name: ios/check-gurl-is-valid-before-spec
description: When accessing or serializing a GURL's string representation, verify is_valid() before calling spec() to prevent assertion crashes on invalid URLs
rubric: correctness
severity: should_fix
---

**Rule.** Check `GURL::is_valid()` before calling `GURL::spec()`, as calling
`spec()` on an invalid, non-empty URL violates its precondition and triggers an
assertion failure (`DUMP_WILL_BE_NOTREACHED()`). If serializing, storing, or
passing URL strings where the URL might be invalid, guard the call with
`is_valid()` or handle the invalid case explicitly. If a string representation
of an invalid URL is needed specifically for error display or debugging, use
`GURL::possibly_invalid_spec()` instead.

**Why.** In `GURL`, calling `spec()` on an invalid, non-empty URL triggers
`DUMP_WILL_BE_NOTREACHED()` and returns an empty string to protect against
accidental security vulnerabilities and bugs. Unchecked `spec()` calls cause
fatal crashes in developer and test builds, while silently returning empty
strings in production. `possibly_invalid_spec()` provides the uncanonicalized
raw string safely for error reporting without asserting.

## Examples

Bad:

```cpp
// Bad: Calling spec() on an invalid URL triggers DUMP_WILL_BE_NOTREACHED().
void SerializeNavigationItem(NSCoder* coder, const GURL& url) {
  web::nscoder_util::EncodeString(coder, kStorageURLKey, url.spec());
}

std::string GetDiagnosticURL(const GURL& url) {
  return url.spec();  // Bad: Asserts if url is invalid.
}
```

Good:

```cpp
// Good: Verify validity before calling spec() when serializing or persisting.
void SerializeNavigationItem(NSCoder* coder, const GURL& url) {
  if (url.is_valid()) {
    web::nscoder_util::EncodeString(coder, kStorageURLKey, url.spec());
  }
}

// Good: Use possibly_invalid_spec() when formatting debug or error output.
std::string GetDiagnosticURL(const GURL& url) {
  return url.possibly_invalid_spec();
}
```
