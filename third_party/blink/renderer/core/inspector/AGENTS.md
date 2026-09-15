# Blink DevTools Protocol / Inspector (`third_party/blink/renderer/core/inspector`)

This directory contains the Blink renderer-side implementation of Chrome DevTools Protocol (CDP) domains and agents (e.g., `InspectorPageAgent`, `InspectorDOMAgent`, `InspectorNetworkAgent`).

## Canonical Guidelines

For comprehensive CDP testing guidelines, see the canonical guide:
- **[`/content/browser/devtools/protocol/AGENTS.md`](/content/browser/devtools/protocol/AGENTS.md)**

## Testing Summary

- **Inspector Protocol tests in JavaScript:** All CDP Inspector Protocol tests should be authored in the `inspector-protocol` folder (`third_party/blink/web_tests/inspector-protocol/`) using **JavaScript**, not C++. See [`/third_party/blink/web_tests/inspector-protocol/AGENTS.md`](/third_party/blink/web_tests/inspector-protocol/AGENTS.md).
- **C++ Tests (Last Resort Only):** Writing C++ unit tests (e.g., `*_unittest.cc`, `*_test.cc`) should only be used as a last resort to exercise low-level logic that cannot be fully covered by an inspector-protocol level JavaScript test, such as isolated helper functions, protocol parsers/serializers, or internal Blink state.

## Related Documentation
- [Canonical CDP Testing Guide (`/content/browser/devtools/protocol/AGENTS.md`)](/content/browser/devtools/protocol/AGENTS.md)
- [Authoring Inspector Protocol Tests (`/third_party/blink/web_tests/inspector-protocol/AGENTS.md`)](/third_party/blink/web_tests/inspector-protocol/AGENTS.md)
- [Blink `renderer/core` Development Guidelines (`/third_party/blink/renderer/core/AGENTS.md`)](/third_party/blink/renderer/core/AGENTS.md)
- [Protocol Definitions (PDL) (`/third_party/blink/public/devtools_protocol/AGENTS.md`)](/third_party/blink/public/devtools_protocol/AGENTS.md)
- [Chrome DevTools (`/chrome/browser/devtools/AGENTS.md`)](/chrome/browser/devtools/AGENTS.md)
- [README.md](README.md)
