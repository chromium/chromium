# Chrome on iOS (`//ios/`)

This directory holds code related to Chrome on iOS (`//ios/chrome/`), the iOS
web layer (`//ios/web/`), and shared iOS components (`//ios/components/`).

## Documentation

- **[Chrome on iOS Architecture](architecture/README.md):** The architectural
  principles and design patterns used in Chrome on iOS, and how features are
  structured.
- **[Chrome on iOS Best Practices](best_practices.md):** Tactical coding
  guidelines, language conventions and engineering rules for `//ios/`. All new
  code is expected to follow this guidance.
- **[Structure of Layered Components](https://www.chromium.org/developers/design-documents/structure-of-layered-components-within-the-chromium-codebase/):**
  Overview of how layered components are structured in the Chromium codebase.
- **Additional iOS documentation in [`//docs/ios/`](/docs/ios/):** Other iOS
  build, testing, and workflow documents live in `//docs/ios/`, though some are
  outdated and in the process of being revised. Where there are contradictions,
  the documentation here in `//ios/` takes precedence. Key documents include:
  - **[Checking Out and Building Chromium for iOS](/docs/ios/build_instructions.md):**
    System requirements, checkout setup and build instructions for iOS targets.
  - **[Automated Testing for Chrome on iOS](/docs/ios/testing.md):** How to
    build and run unit tests and EarlGrey integration tests on iOS.
