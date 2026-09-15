# Content DevTools Protocol (`content/browser/devtools/protocol`)

This directory contains the browser-side implementation of Chrome DevTools Protocol (CDP) domains in the `content` layer.

This document serves as the canonical guide for CDP testing conventions across Content, Blink, and Chrome protocol implementations.

## Testing Guidelines

### Prefer JavaScript Inspector Protocol Tests

Do **NOT** add new Inspector Protocol tests in C++ (such as in `devtools_protocol_browsertest.cc`).

Inspector Protocol tests for Chrome DevTools Protocol should be authored in the `inspector-protocol` folder (`third_party/blink/web_tests/inspector-protocol/`) using **JavaScript**, not C++.

For instructions on authoring tests, directory locations, and test structure, see:
- [Authoring Inspector Protocol Tests (`/third_party/blink/web_tests/inspector-protocol/AGENTS.md`)](/third_party/blink/web_tests/inspector-protocol/AGENTS.md)

For instructions on building and running Blink web tests (`blink_tests` and `run_web_tests.py`), see:
- [Blink `renderer/core` Development Guidelines (`/third_party/blink/renderer/core/AGENTS.md`)](/third_party/blink/renderer/core/AGENTS.md)

### Why JavaScript for Inspector Protocol Tests?
- **Fast execution and iteration:** Tests run in `content_shell` without needing to recompile large C++ test targets or link heavy browser test binaries.
- **Natural protocol interaction:** CDP is a JSON-RPC based protocol. Authoring tests in JavaScript using `dp.<Domain>.<command>()` is idiomatic, concise, and significantly easier to read and maintain than constructing `base::Value::Dict` structures in C++.
- **Preventing test suite bloat:** `devtools_protocol_browsertest.cc` is already thousands of lines long. Adding new tests there increases compilation time and maintenance overhead.
- **Dedicated harness:** The `inspector-protocol` runner provides robust utilities for target management, event logging, and deterministic expectation matching (`-expected.txt`).

### When to Use C++ Tests (Last Resort Only)

Writing C++ tests (such as `*_unittest.cc` or C++ browser tests) should be used **only as a last resort** to exercise low-level logic that cannot be fully covered by an inspector-protocol level JavaScript test, such as:
- Protocol parsing and serializing logic
- Isolated C++ helper functions and data structures
- Internal browser-process or renderer-process state that cannot be observed, mocked, or triggered via CDP

## Related Documentation
- [Authoring Inspector Protocol Tests (`/third_party/blink/web_tests/inspector-protocol/AGENTS.md`)](/third_party/blink/web_tests/inspector-protocol/AGENTS.md)
- [Blink `renderer/core` Development Guidelines (`/third_party/blink/renderer/core/AGENTS.md`)](/third_party/blink/renderer/core/AGENTS.md)
- [Chrome DevTools & Protocol Handlers (`/chrome/browser/devtools/AGENTS.md`)](/chrome/browser/devtools/AGENTS.md)
- [Blink Protocol Handlers (`/third_party/blink/renderer/core/inspector/AGENTS.md`)](/third_party/blink/renderer/core/inspector/AGENTS.md)
- [Protocol Definitions (`/third_party/blink/public/devtools_protocol/AGENTS.md`)](/third_party/blink/public/devtools_protocol/AGENTS.md)
- [CDP Contribution Guide](https://goo.gle/devtools-contribution-guide-cdp)
