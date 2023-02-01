# Origins that support WebAuthn

WebAuthn is [only available](https://www.w3.org/TR/webauthn-2/#sctn-api) to pages which are in a [secure context](https://w3c.github.io/webappsec-secure-contexts/#intro). Specifically, pages served from the following origins can use WebAuthn:

1. HTTPS pages with a valid certificate. These pages can assert an RP ID that follows [the standard rules](https://www.w3.org/TR/webauthn-2/#rp-id), i.e. labels can be removed from the left of the domain until an eTLD+1 is hit.
2. HTTP pages served from `localhost` or a domain ending in `.localhost`. These pages follow the same rules for asserting an RP, where `localhost` is considered a TLD.
3. Pages served from an extension, e.g. with the `chrome-extension` scheme in Chrome or similar schemes in other Chromium-based browsers. These pages should leave the RP ID fields in WebAuthn structures blank to accept the default.
