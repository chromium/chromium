# `fuchsia.web` - Fuchsia WebEngine and Runners

This directory contains code related to the
[`fuchsia.web`](https://fuchsia.dev/reference/fidl/fuchsia.web) FIDL API.
Specifically, it contains the implementation of Fuchsia WebEngine and code
related to it, including the Runners that use it. Code in this directory must
not be used outside it and its subdirectories.

WebEngine on Fuchsia officially supports (debug and release) x (ARM64 and X64) x
(size optimization and perf optimization) combinations.

It is also possible to run using Address Sanitizer like
[fuchsia-fyi-x64-asan builder](https://ci.chromium.org/p/chromium/builders/luci.chromium.ci/fuchsia-fyi-x64-asan)

General information about Chromium on Fuchsia is
[here](../docs/fuchsia/README.md).

[TOC]

## Reasoning about Fuchsia Support

When developing new features or modifying existing ones in Chromium, developers may wonder about the level of support required for Fuchsia, particularly when encountering platform-specific limitations or less capable APIs. The following guidelines outline the purpose of Chromium on Fuchsia and how to approach feature integration.

### Understanding the Fuchsia Web Environment

*   **WebEngine vs. Full Browser:** There is no general-purpose "Chrome browser" on Fuchsia (no address bar, bookmarks, extensions, downloads, or standard browser UI). Instead, Chromium on Fuchsia is distributed as **WebEngine** (a WebView-like system service) and **Runners** (e.g., `CastRunner`).
*   **Specialized Smart Display Target:** WebEngine is primarily used to render web content on **Smart Displays** (such as Nest Hub, Nest Hub Max, and Nest Hub 2nd Gen). It powers Cast applications, communication apps, Flutter WebViews, Assistant web content, and the Out-of-Box Experience (OOBE).
*   **Visual Representation:** While WebEngine runs on embedded-class hardware, it is **not headless**. It is responsible for rendering the primary web-based user interface on these devices.

### Feature Development Guidelines

The primary focus of Fuchsia support is maintaining **stability, security, and privacy** on smart displays, with limited new platform-specific features planned.

When designing or integrating features, please adhere to the following principles:

1.  **Web Standards Compliance:** Standard web features and APIs should be fully supported to ensure compatibility with web content.
    *   *Exception:* Hardware-specific APIs (e.g., WebUSB, WebBluetooth) that are not supported or relevant on smart displays. In these cases, providing a fake or empty implementation is acceptable to maintain compilation compatibility without adding complex code paths.
2.  **Code Maintainability and Platform Consolidation:** Minimizing Fuchsia-specific code divergence is critical for long-term maintainability. Day-to-day maintenance is managed primarily by the Chrome-Fuchsia team (EngProd), focusing on security and stability.
    *   Avoid introducing `is_fuchsia` (in GN build files) or `BUILDFLAG(IS_FUCHSIA)` (in C++ code) conditional blocks unless it is absolutely infeasible to implement the feature on Fuchsia.
    *   Including unused code on Fuchsia is acceptable to minimize platform-specific divergence and keep the codebase unified.
    *   If a feature must be excluded from Fuchsia due to fundamental platform incompatibilities, please consult and CC [fuchsia-dev@chromium.org](mailto:fuchsia-dev@chromium.org).
3.  **Handling Platform Limitations:** If a feature is required but difficult to support due to Fuchsia platform limitations:
    *   **Prioritize simplicity and maintainability** over optimal performance or feature completeness.
    *   Using a fallback implementation or **accepting reduced performance** is preferred over introducing complex, Fuchsia-specific workarounds.
    *   If supporting a feature on Fuchsia requires disproportionate engineering effort (e.g., significant architectural changes to accommodate less capable platform APIs), it is acceptable to exclude the feature from the Fuchsia build entirely.

## Code organization

Each of the following subdirectories contain code for a specific Fuchsia
service:

*   `./common` contains code shared by both WebEngine and Runners.

*   `./runners`contains implementations of Fuchsia `sys.runner`.

    *   `./runners/cast` Enables the Fuchsia system to launch Cast applications.

*   `./shell` contains WebEngineShell, a simple wrapper for launching URLs in
    WebEngine from the command line.

*   `./webengine` contains the WebEngine implementation. WebEngine is an
    implementation of
    [`fuchsia.web`](https://fuchsia.dev/reference/fidl/fuchsia.web) that enables
    Fuchsia Components to render web content using Chrome's Content layer.

*   `./webinstance_host` contains code for WebEngine clients to directly
    instantiate a WebInstance Component (`web_instance.cm`) using the WebEngine
    package.

### Test code

There are 3 major types of tests within this directory:

*   Unit tests: Exercise a single class in isolation, allowing full control over
    the external environment of this class.

*   Browser tests: Spawn a full browser process and its child processes. The
    test code is run inside the browser process, allowing for full access to the
    browser code - but not other processes.

*   Integration tests: Exercise the published FIDL API of a Fuchsia Component.
    For instance, `//fuchsia_web/webengine:web_engine_integration_tests` make
    use of the `//fuchsia_web/webengine:web_engine` component. The test code
    runs in a separate process in a separate Fuchsia Component, allowing only
    access to the published API of the component under test.

Integration tests are more resource-intensive than browser tests, which are in
turn more expensive than unit tests. Therefore, when writing new tests, it is
preferred to write unit tests over browser tests over integration tests.

As a general rule, test-only code should live in the same directory as the code
under test with an explicit file name, either `fake_*`, `test_*`,
`*_unittest.cc`, `*_ browsertest.cc` or `*_integration_test.cc`.

Test code that is shared across Components should live in `a dedicated``test`
directory. For example, the `//fuchsia_web/webengine/test` directory, which
contains code shared by all browser tests, and `//fuchsia_web/common/test`,
which contains code shared by tests for both WebEngine and Runners.
