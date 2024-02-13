# The `extensions/` directory

The `extensions/` directory contains embedder-specific, not-web-exposed APIs (e.g., not-web-exposed APIs for Chromium OS etc).
The directory is useful to implement embedder-specific, not-web-exposed APIs
using Blink technologies for web-exposed APIs like WebIDL, V8 bindings and Oilpan.

Web-exposed has a definition [here](https://www.chromium.org/blink/guidelines/web-exposed/). In some very
special OS-specific cases, non-HTTP or HTTPS-scheme web pages *may* be considered non-web-exposed. At present,
the only one is ChromeOS Kiosk mode, where a single page is in the mode, but cannot be navigated to, and
navigating away from it loses access to any extension APIs. Contact the OWNERS in this directory for any questions.

Remember that you should not implement any web-exposed APIs in `extensions/`. Web-exposed APIs should go through the standardization process and be implemented in `core/` or `modules/`. Also, per [the Chromium contributor guideline](https://chromium.googlesource.com/chromium/src/+/main/docs/contributing.md#code-guidelines), code that is not used by Chromium should not be added to `extensions/`.

In terms of dependencies, `extensions/` can depend on `modules/`, `core/` and `platform/`, but not vice versa.

The `extensions/` directory contains sub-directories for individual embedders (e.g., `extensions/chromeos/`). Each sub-directory is linked into the Blink link unit only when the embedder is built.
