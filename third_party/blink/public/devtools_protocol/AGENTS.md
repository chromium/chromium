# DevTools Protocol Definitions (`third_party/blink/public/devtools_protocol`)

This directory defines the Chrome DevTools Protocol (CDP) API via PDL files (`browser_protocol.pdl` and `domains/*.pdl`).

## Canonical Guidelines

For complete CDP testing guidelines, see the canonical guide:
- **[`/content/browser/devtools/protocol/AGENTS.md`](/content/browser/devtools/protocol/AGENTS.md)**

## Testing Summary

- **Inspector Protocol tests in JavaScript:** When adding or modifying protocol domain definitions, author new Inspector Protocol tests in `third_party/blink/web_tests/inspector-protocol/` using **JavaScript**, not C++. See [`/third_party/blink/web_tests/inspector-protocol/AGENTS.md`](/third_party/blink/web_tests/inspector-protocol/AGENTS.md).
- **C++ Tests (Last Resort Only):** C++ unit tests or browser tests should only be used as a last resort to exercise low-level logic that cannot be fully covered by an inspector-protocol level JavaScript test (such as protocol parsing/serializing logic or internal helper functions).

## Related Documentation
- [Canonical CDP Testing Guide (`/content/browser/devtools/protocol/AGENTS.md`)](/content/browser/devtools/protocol/AGENTS.md)
- [Authoring Inspector Protocol Tests (`/third_party/blink/web_tests/inspector-protocol/AGENTS.md`)](/third_party/blink/web_tests/inspector-protocol/AGENTS.md)
- [Blink Protocol Handlers (`/third_party/blink/renderer/core/inspector/AGENTS.md`)](/third_party/blink/renderer/core/inspector/AGENTS.md)
- [Chrome DevTools (`/chrome/browser/devtools/AGENTS.md`)](/chrome/browser/devtools/AGENTS.md)
- [README.md](README.md)
