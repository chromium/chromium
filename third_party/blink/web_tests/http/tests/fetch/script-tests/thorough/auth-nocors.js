if (self.importScripts) {
  importScripts('/fetch/resources/fetch-test-helpers.js');
  importScripts('/fetch/resources/thorough-util.js');
}

var TEST_TARGETS = [
  // Auth check
  // Test that default mode is no-cors in serviceworker-proxied tests.
  onlyOnServiceWorkerProxiedTest(
    [BASE_URL + 'Auth',
     [fetchResolved, hasBody], [authCheck1]]),
  onlyOnServiceWorkerProxiedTest(
    [BASE_URL + 'Auth&credentials=omit',
     [fetchResolved, hasBody], [checkJsonpError]]),
  onlyOnServiceWorkerProxiedTest(
    [BASE_URL + 'Auth&credentials=include',
     [fetchResolved, hasBody], [authCheck1]]),
  onlyOnServiceWorkerProxiedTest(
    [BASE_URL + 'Auth&credentials=same-origin',
     [fetchResolved, hasBody], [authCheck1]]),

  // Tests for same-origin requests.
  [BASE_URL + 'Auth&mode=no-cors&credentials=omit',
   [fetchResolved, hasBody], [checkJsonpError]],
  [BASE_URL + 'Auth&mode=no-cors&credentials=include',
   [fetchResolved, hasBody], [authCheck1]],
  [BASE_URL + 'Auth&mode=no-cors&credentials=same-origin',
   [fetchResolved, hasBody], [authCheck1]],

  // Test that default mode is no-cors in serviceworker-proxied tests.
  onlyOnServiceWorkerProxiedTest(
    [OTHER_BASE_URL + 'Auth',
     [fetchResolved, noBody, typeOpaque],
     [onlyForCrossSiteCookieTest(authCheck2)]]),
  onlyOnServiceWorkerProxiedTest(
    [OTHER_BASE_URL + 'Auth&credentials=omit',
     [fetchResolved, noBody, typeOpaque],
     [checkJsonpError]]),
  onlyOnServiceWorkerProxiedTest(
    [OTHER_BASE_URL + 'Auth&credentials=include',
     [fetchResolved, noBody, typeOpaque],
     [onlyForCrossSiteCookieTest(authCheck2)]]),
  onlyOnServiceWorkerProxiedTest(
    [OTHER_BASE_URL + 'Auth&credentials=same-origin',
     [fetchResolved, noBody, typeOpaque],
     [checkJsonpError]]),

  // Tests for cross-origin requests.
  [OTHER_BASE_URL + 'Auth&mode=no-cors&credentials=omit',
   [fetchResolved, noBody, typeOpaque],
   onlyOnServiceWorkerProxiedTest([checkJsonpError])],
  [OTHER_BASE_URL + 'Auth&mode=no-cors&credentials=include',
   [fetchResolved, noBody, typeOpaque],
   onlyOnServiceWorkerProxiedTest([onlyForCrossSiteCookieTest(authCheck2)])],
  [OTHER_BASE_URL + 'Auth&mode=no-cors&credentials=same-origin',
   [fetchResolved, noBody, typeOpaque],
   onlyOnServiceWorkerProxiedTest([checkJsonpError])],
];

if (self.importScripts) {
  executeTests(TEST_TARGETS);
  done();
}
