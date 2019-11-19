if (self.importScripts) {
  importScripts('/fetch/resources/fetch-test-helpers.js');
  importScripts('/fetch/resources/thorough-util.js');
}

// This test assumes TEST_TARGETS are executed in order and sequentially.
var TEST_TARGETS = [];

// cookieCheckX checks the cookies sent in the request.
// SetCookie=cookieX indicates to set cookies in the response.
// So a SetCookie=cookieX indication may affect the next cookieCheckX,
// but not the cookieCheckX in the same request.

// Test same-origin requests.
// The same set of requests are also in cookie.js,
// with different modes (same-origin and cors).
// forEach structure is left unchanged here to keep
// cookie.js and cookie-nocors.js parallel with small diffs.
['no-cors'].forEach(function(mode) {
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

if (shouldIncludeCrossSiteCookieTests()) {
  // URL to check current cookie.
  var OTHER_CHECK_URL =
    OTHER_BASE_URL +
    'mode=cors&credentials=include&method=POST&ACAOrigin=' + BASE_ORIGIN +
    '&ACACredentials=true&label=';

  TEST_TARGETS.push(
    // At first, cookie is cookie=cookie2.
    // Tests for mode=no-cors.

    // Try to set cookieC, but
    // cookie is not sent/updated because credentials flag is not set.
    [OTHER_BASE_URL + 'mode=no-cors&credentials=omit&SetCookie=cookieC&SameSiteNone',
     [fetchResolved, noBody, typeOpaque],
     onlyOnServiceWorkerProxiedTest([cookieCheckNone])],
    [OTHER_CHECK_URL + 'otherCheck1', [fetchResolved], [cookieCheck2]],

    // Set cookieC with opaque response. Response is opaque, but cookie is set.
    [OTHER_BASE_URL + 'mode=no-cors&credentials=include&SetCookie=cookieC&SameSiteNone',
     [fetchResolved, noBody, typeOpaque],
     onlyOnServiceWorkerProxiedTest([cookieCheck2])],
    [OTHER_CHECK_URL + 'otherCheck2', [fetchResolved], [cookieCheckC]],

    // Set cookieA with opaque response. Response is opaque and cookie is not set.
    [OTHER_BASE_URL + 'mode=no-cors&credentials=same-origin&SetCookie=cookieA&SameSiteNone',
     [fetchResolved, noBody, typeOpaque],
     onlyOnServiceWorkerProxiedTest([cookieCheckNone])],
    [OTHER_CHECK_URL + 'otherCheck3', [fetchResolved], [cookieCheckC]]
  );
}

if (self.importScripts) {
  executeTests(TEST_TARGETS);
  done();
}
