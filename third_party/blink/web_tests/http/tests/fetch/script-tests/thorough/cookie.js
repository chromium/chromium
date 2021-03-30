if (self.importScripts) {
  importScripts('/fetch/resources/fetch-test-helpers.js');
  importScripts('/fetch/resources/thorough-util.js');
}

// This test assumes TEST_TARGETS are executed in order and sequentially.
var TEST_TARGETS = [];
var {BASE_ORIGIN, BASE_URL, OTHER_BASE_URL} = get_thorough_test_options();

// cookieCheckX checks the cookies sent in the request.
// SetCookie=cookieX indicates to set cookies in the response.
// So a SetCookie=cookieX indication may affect the next cookieCheckX,
// but not the cookieCheckX in the same request.

// Test same-origin requests.
// The same set of requests are also in cookie-nocors.js,
// with different mode (no-cors).
['same-origin', 'cors'].forEach(function(mode) {
    // At first, cookie is cookie=cookie1.
    TEST_TARGETS.push(
      // Set cookie=cookieA by credentials=same-origin.
      [BASE_URL + 'mode=' + mode + '&credentials=same-origin&SetCookie=cookieA',
       [fetchResolved, hasBody], [cookieCheck1]],

      // Set cookie=cookieB by credentials=include.
      [BASE_URL + 'mode=' + mode + '&credentials=include&SetCookie=cookieB',
       [fetchResolved, hasBody], [cookieCheckA]],
      // Check cookie.
      [BASE_URL + 'mode=' + mode + '&credentials=same-origin',
       [fetchResolved, hasBody], [cookieCheckB]],

      // Try to set cookie=cookieC by credentials=omit, but
      // cookie is not sent/updated if credentials flag is unset.
      [BASE_URL + 'mode=' + mode + '&credentials=omit&SetCookie=cookieC',
       [fetchResolved, hasBody], [cookieCheckNone]],

      // Set-Cookie2 header is ignored.
      [BASE_URL + 'mode=' + mode +
       '&credentials=same-origin&SetCookie2=cookieC',
       [fetchResolved, hasBody], [cookieCheckB]],

      // Reset cookie to cookie1.
      [BASE_URL + 'mode=' + mode + '&credentials=same-origin&SetCookie=cookie1',
       [fetchResolved, hasBody], [cookieCheckB]]);
  });

// Test cross-origin requests.

// URL to check current cookie.
var OTHER_CHECK_URL =
  OTHER_BASE_URL +
  'mode=cors&credentials=include&method=POST&ACAOrigin=' + BASE_ORIGIN +
  '&ACACredentials=true&label=';

TEST_TARGETS.push(
  // At first, cookie is cookie=cookie2.

  // Tests for mode=cors.

  // Set cookieA by a successful CORS.
  [OTHER_BASE_URL + 'mode=cors&credentials=include&ACAOrigin=' + BASE_ORIGIN +
    '&ACACredentials=true&SetCookie=cookieA',
    [fetchResolved, hasBody, typeCors], [cookieCheck2]],
  // Check that cookie is set.
  [OTHER_CHECK_URL + 'otherCheck1', [fetchResolved], [cookieCheckA]],

  // Set cookieB by a rejected CORS. Fetch is rejected, but cookie is set.
  // Spec: https://fetch.spec.whatwg.org/
  //   Cookie is set in Step 13 of HTTP network or cache fetch
  //   (called from Step 3.5 of HTTP fetch),
  //   which is before CORS check in Step 3.6 of HTTP fetch.
  [OTHER_BASE_URL + 'mode=cors&credentials=include&ACAOrigin=*&SetCookie=cookieB',
    [fetchRejected]],
  [OTHER_CHECK_URL + 'otherCheck2', [fetchResolved], [cookieCheckB]],

  // Set cookieC by a rejected CORS. Fetch is rejected, but cookie is set.
  [OTHER_BASE_URL + 'mode=cors&credentials=include&ACAOrigin=' + BASE_ORIGIN +
    '&SetCookie=cookieC',
    [fetchRejected]],
  [OTHER_CHECK_URL + 'otherCheck3', [fetchResolved], [cookieCheckC]],

  // Set cookieA by a rejected CORS. Fetch is rejected, but cookie is set.
  [OTHER_BASE_URL + 'mode=cors&credentials=include&ACAOrigin=*&ACACredentials=true&SetCookie=cookieA',
    [fetchRejected]],
  [OTHER_CHECK_URL + 'otherCheck4', [fetchResolved], [cookieCheckA]],

  // Try to set cookieB, but
  // cookie is not sent/updated because credentials flag is not set.
  [OTHER_BASE_URL + 'mode=cors&credentials=omit&ACAOrigin=' + BASE_ORIGIN +
    '&ACACredentials=true&SetCookie=cookieB',
    [fetchResolved, hasBody, typeCors], [cookieCheckNone]],
  [OTHER_CHECK_URL + 'otherCheck5', [fetchResolved], [cookieCheckA]],

  // Try to set cookieB, but
  // cookie is not sent/updated because credentials flag is not set.
  [OTHER_BASE_URL + 'mode=cors&credentials=same-origin&ACAOrigin=' +
    BASE_ORIGIN + '&ACACredentials=true&SetCookie=cookieB',
    [fetchResolved, hasBody, typeCors], [cookieCheckNone]],
  [OTHER_CHECK_URL + 'otherCheck6', [fetchResolved], [cookieCheckA]],

  // Tests for CORS preflight.

  // Set cookieB by a successful CORS with CORS preflight.
  [OTHER_BASE_URL + 'mode=cors&credentials=include&ACAOrigin=' + BASE_ORIGIN +
    '&PACAOrigin=' + BASE_ORIGIN +
    '&ACACredentials=true&PACACredentials=true&method=PUT&PACAMethods=PUT&SetCookie=cookieB&PreflightTest=200',
    [fetchResolved, hasBody, typeCors], [cookieCheckA]],
  [OTHER_CHECK_URL + 'otherCheck7', [fetchResolved], [cookieCheckB]],
  // Set-Cookie2 should be ignored for CORS.
  [OTHER_BASE_URL + 'mode=cors&credentials=include&ACAOrigin=' + BASE_ORIGIN +
    '&PACAOrigin=' + BASE_ORIGIN +
    '&ACACredentials=true&PACACredentials=true&method=PUT&PACAMethods=PUT&SetCookie2=cookieC&PreflightTest=200',
    [fetchResolved, hasBody, typeCors], [cookieCheckB]],
  [OTHER_CHECK_URL + 'otherCheck8', [fetchResolved], [cookieCheckB]],

  // Test that no Cookie header is sent in CORS preflight.
  // Test that Set-Cookie in CORS preflight is ignored.

  // Set-Cookie=cookieC is sent in CORS preflight, but this should be ignored.
  [OTHER_BASE_URL + 'mode=cors&credentials=include&ACAOrigin=' + BASE_ORIGIN +
    '&PACAOrigin=' + BASE_ORIGIN +
    '&ACACredentials=true&PACACredentials=true&method=PUT&PACAMethods=PUT&PSetCookie=cookieC&PreflightTest=200',
    [fetchResolved, hasBody, typeCors], [cookieCheckB]],
  [OTHER_CHECK_URL + 'otherCheck9', [fetchResolved], [cookieCheckB]],

  // Set-Cookie2=cookieC is sent in CORS preflight, but this should be ignored.
  [OTHER_BASE_URL + 'mode=cors&credentials=include&ACAOrigin=' + BASE_ORIGIN +
    '&PACAOrigin=' + BASE_ORIGIN +
    '&ACACredentials=true&PACACredentials=true&method=PUT&PACAMethods=PUT&PSetCookie2=cookieC&PreflightTest=200',
    [fetchResolved, hasBody, typeCors], [cookieCheckB]],
  [OTHER_CHECK_URL + 'otherCheck10', [fetchResolved], [cookieCheckB]],

  // Tests for mode=same-origin.
  // Rejected as Network Error before entering basic fetch or HTTP fetch,
  // so no cookies are set.

  // Try to set cookieC.
  [OTHER_BASE_URL + 'mode=same-origin&credentials=omit&SetCookie=cookieC',
    [fetchRejected]],
  [OTHER_CHECK_URL + 'otherCheck11', [fetchResolved], [cookieCheckB]],

  // Try to set cookieC.
  [OTHER_BASE_URL + 'mode=same-origin&credentials=include&SetCookie=cookieC',
    [fetchRejected]],
  [OTHER_CHECK_URL + 'otherCheck12', [fetchResolved], [cookieCheckB]],

  // Try to set cookieC.
  [OTHER_BASE_URL +
    'mode=same-origin&credentials=same-origin&SetCookie=cookieC',
    [fetchRejected]],
  [OTHER_CHECK_URL + 'otherCheck13', [fetchResolved], [cookieCheckB]]
);

if (self.importScripts) {
  executeTests(TEST_TARGETS);
  done();
}
