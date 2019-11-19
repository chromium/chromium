spec: https://w3c.github.io/clipboard-apis/#async-clipboard-api

This directory contains async clipboard tests automated through use of
Chrome-specific test helper `permissions-helper.js`. Related tests not requiring
`permissions-helper.js` can be found in
[`web_tests/external/wpt/clipboard-apis/`](https://cs.chromium.org/chromium/src/third_party/blink/web_tests/external/wpt/clipboard-apis/).
Please note that these tests cannot be run manually, as testRunner (needed by
`permissions-helper.js`) isn't exposed outside of `content_shell
--run-web-tests`.

Whenever tests here or there are updated, please be sure to update corresponding
tests, so that the web platform and automated buildbots can both keep updated.
