# Route Matching

This directory contains code for matching URLs against a set of routes.

The code here is responsible for parsing a set of routing rules defined in a
JSON format, storing them, and matching them against URLs.

Active routes can be matched against an `@route` CSS rule, allowing different
styles to be applied based on the current route(s).

The active routes will be exposed through the `navigator.routes` JavaScript API
(exact naming TBD).

Tracking bug: https://issues.chromium.org/issues/436805487
