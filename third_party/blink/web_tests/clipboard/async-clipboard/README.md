spec: https://w3c.github.io/clipboard-apis/#async-clipboard-api

This directory contains async clipboard tests automated through use of
Chrome-specific test helper `permissions-helper.js`. Related tests not using
`permissions-helper.js` can be found in
[`web_tests/external/wpt/clipboard-apis/`](https://cs.chromium.org/chromium/src/third_party/blink/web_tests/external/wpt/clipboard-apis/).
These tests previously could not be run manually, as there
was no exposed way to allow a permission in automated tests.

TODO(https://crbug.com/1076691): Since the
[WPT Issue](https://github.com/web-platform-tests/wpt/issues/5671) was fixed,
this is now possible in WPT tests, so migrate all these tests over to WPT to
avoid duplication.

Whenever tests here are updated, please be sure to update corresponding
tests, so that the web platform and automated buildbots can both keep updated.
