# iOS Web Layer

## Overview

`ios/web` performs a similar role on iOS as what `content/` does on other
platforms. It encapsulates the architecture for displaying web content,
managing navigation, and providing an API that embedders (like `ios/chrome`,
`ios/web_view`) can use to build a browser (or browser-like application).

On iOS, Blink is not available. Instead, `ios/web` wraps `WKWebView`
(provided by WebKit) and layers on top of it to provide features like:

*   **WebState**: The core class representing a tab/page (analogous to
`content::WebContents`).
*   **BrowserState**: Represents a user's session (analogous to
`content::BrowserContext`).
*   **JavaScript Messaging**: A channel for two-way communication between the
embedder and JavaScript running in the page content.
*   **Navigation**: Abstractions over `WKWebView`'s navigation stack.

## Minimum Deployment Version

`//ios/web` supports **iOS 16** and above. This is a lower minimum
iOS version than Chrome in order to support `//ios/web_view`
deployment to older versions of iOS.

## Layering & Dependencies

The `ios/web` layer sits above `base`, `net`, `components`, `ui`, and more.
It is the renderer equivalent on iOS.

### `ios/web` vs `ios/components` vs `components/`

*   **`ios/web`**: The renderer-equivalent on iOS. It does **not** depend on
    higher-level features found in `ios/chrome` or `ios/web_view`.
*   **`ios/components`**: Contains reusable feature code that is specific to
    iOS but shared across multiple embedders (e.g., `ios/chrome` and
    `ios/web_view`). Implementations in `ios/components` often depend on
    `ios/web/public`. See `//ios/components/README.md`.
*   **`components/`**: Generally cross-platform, sometimes excluding iOS.
    See `//components/README.md`.

**Dependency Notes**:
*   `ios/components` can depend on `ios/web`.
*   `ios/web` can depend on `ios/components`, so long as the component does not
    depend on `ios/web`.

## Usage

Embedders (like `ios/chrome` or `ios/web_view`) must consume `ios/web` through
the **Public API**, located in `ios/web/public`.
