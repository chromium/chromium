This directory contains the renderer-specific portions of the
 [Permissions Policy API](https://w3c.github.io/webappsec-permissions-policy/),
 and [Document Policy API](https://github.com/w3c/webappsec-permissions-policy/blob/master/document-policy-explainer.md).

This includes:

* The parser for the HTTP `Permissions-Policy`/`Feature-Policy` header and the iframe `allow` attribute.

* The parser for the HTTP `Document-Policy` header and the iframe `policy` attribute.

* Helpers for manipulating the parsed declarations.

* Implementation of the `document.featurePolicy` and `iframe.featurePolicy` interfaces.


## Other policy resources

* The core algorithms can be found in `/common/feature\_policy/`.

* The feature list enum is found in `/public/mojom/feature\_policy/`.

* The recommended API for checking whether features should be enabled or not is
ExecutionContext::IsFeatureEnabled().
