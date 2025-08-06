# Chromium Source Tree Overview

This document provides a high-level map to the Chromium source code, explaining
the purpose of the major top-level directories.

### Core Application Logic

*   `//chrome`: The code for the Chrome browser application itself. This layer
    integrates all the underlying components into the final product. It
    includes UI, browser-specific features, and application logic.
*   `//components`: Reusable modules that provide specific features (e.g.,
    autofill, bookmarks, signin, policy). They are designed to be layered on
    top of `//content` and have minimal dependencies on each other.

### Core Abstraction Layers

*   `//content`: The core multi-process rendering engine abstraction. It
    encapsulates the sandbox and the browser/renderer process model. Most
    features are built on top of the Content API. It is the layer below
    `//chrome`.
*   `//third_party/blink`: The Blink rendering engine. This is where the open
    web platform is implemented (e.g., DOM, CSS, JavaScript APIs). It runs
    inside the sandboxed renderer process.

### Foundational Libraries

*   `//base`: The fundamental building blocks of Chromium. Contains essential
    utilities for C++, data structures, threading primitives (`base::Callback`,
    `base::TaskRunner`), and platform abstractions. Most other code depends on
    `//base`.
*   `//net`: The networking stack. Implements everything from HTTP to QUIC and
    provides abstractions for network requests.
*   `//mojo`: The core IPC (Inter-Process Communication) library used for
    communication between processes and services.
*   `//services`: A collection of standalone services that are often run in their
    own processes and communicate via Mojo interfaces.

### UI Toolkits

*   `//ui`: The foundational toolkit for building user interfaces.
*   `//ui/views`: The primary framework for building cross-platform desktop UI
    (Windows, Linux, ChromeOS). It provides a widget-based system for creating
    native-feeling interfaces.