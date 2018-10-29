This directory contains the renderer-specific portions of the [Feature
Policy API](https://wicg.github.io/feature-policy/).

This includes:

* The parser for the HTTP Feature-Policy header and the iframe allow attribute.

* Helpers for manipulating the parsed declarations.

* Implementation of the `document.policy` and `iframe.policy` interfaces.


## Other feature policy resources

* The core feature policy algorithms can be found in `/common/feature\_policy/`.

* The feature list enum is found in `/public/mojom/feature\_policy/`.

* The recommended API for checking whether features should be enabled or not is
Document::IsFeatureEnabled() (or SecurityContext::IsFeatureEnabled in a non-
document context).
