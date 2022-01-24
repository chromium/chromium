# Cross-Origin Read Blocking (CORB)

Cross-Origin Read Blocking (CORB) and Opaque Response Blocking (ORB) are
security features that prevent disclosing the body of cross-origin "no-cors"
responses outside of a limited set of legacy scenarios (images, scripts,
etc.).  Outside of these legacy scenarios, the body of a cross-origin,
"no-cors" response doesn't ever need to (and shouldn't) reach the renderer process
(e.g. `fetch` returns an opaque response body for "no-cors" requests).
Robust Site Isolation requires either CORB or ORB to prevent a renderer
process from reading opaque responses from other sites (e.g. reading the
opaque responses via Spectre, or after compromising the renderer process).

Good starting points for learning more:

- https://www.chromium.org/Home/chromium-security/corb-for-developers
- https://github.com/annevk/orb
