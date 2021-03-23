if (window.testRunner) {
  // In Chromium we need to change the setting to disallow displaying
  // insecure contents.

  // By default, LayoutTest content_shell returns allowed for
  // "Should fetching request be blocked as mixed content?" at Step 5:
  // https://w3c.github.io/webappsec/specs/mixedcontent/#should-block-fetch
  // > If the user agent has been instructed to allow mixed content
  // Turning the switch below to false makes this condition above false, and
  // make content_shell to run into Step 7 to test mixed content blocking.
  testRunner.overridePreference('WebKitAllowRunningInsecureContent', false);

  // Accept all cookies.
  testRunner.setBlockThirdPartyCookies(false);
}

// There must be an |options| object in scope before running this script, which
// is obtained via either get_thorough_test_options() or
// get_fetch_test_options().
//
// How tests starts:
// 1. http://127.0.0.1:8000/.../X.html is loaded.
// 2. init(): Do initialization.
//    In thorough/* tests
//    (see init() in each test category's TEMPLATE file):
//    - Login to HTTP pages.
//      This is done first from HTTP origin to avoid mixed content blocking.
//    - Login to HTTPS pages.
// If the test is not base-https:
//   3a. start(): Start tests.
// Otherwise:
//   3b. Redirect to https://127.0.0.1:8443/.../X.html.
//   4b. start(): Start tests.

var t = async_test('Startup');
if (location.protocol != 'https:') {
  init(t)
    .then(function() {
        // Initialization done. In thorough/* tests, login done.
        if (location.pathname.indexOf('base-https') >= 0) {
          // Step 3b. For base-https tests, redirect to HTTPS page here.
          location = options["BASE_ORIGIN"] + location.pathname;
        } else {
          // Step 3a. For non-base-https tests, start tests here.
          start(t);
        }
      });
} else {
  // Step 4b. Already redirected to the HTTPS page.
  // Start tests for base-https tests here.
  start(t);
}
