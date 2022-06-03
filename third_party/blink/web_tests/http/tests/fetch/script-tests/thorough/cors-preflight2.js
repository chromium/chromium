if (self.importScripts) {
  importScripts('/fetch/resources/fetch-test-helpers.js');
  importScripts('/fetch/resources/thorough-util.js');
}

// Tests for CORS preflight fetch (non-simple methods).
// Spec: https://fetch.spec.whatwg.org/#cors-preflight-fetch

var TEST_TARGETS = [];
var {BASE_ORIGIN, OTHER_BASE_URL} = get_thorough_test_options();

['PUT', 'XXX'].forEach(function(method) {
    var checkMethod = checkJsonpMethod.bind(this, method);
    TEST_TARGETS.push(
      // CORS check
      // https://fetch.spec.whatwg.org/#concept-cors-check
      // Tests for Access-Control-Allow-Origin header.
      // CORS preflight fetch
      // https://fetch.spec.whatwg.org/#cors-preflight-fetch
      // Tests for Access-Control-Allow-Methods header.
      // Tests for Access-Control-Allow-Headers header.
      [OTHER_BASE_URL + 'mode=cors&credentials=same-origin&method=' + method,
       [fetchRejected]],
      [OTHER_BASE_URL + 'mode=cors&credentials=same-origin&method=' + method +
       '&ACAMethods=' + method,
       [fetchRejected]],
      [OTHER_BASE_URL + 'mode=cors&credentials=same-origin&method=' + method +
       '&ACAOrigin=*',
       [fetchRejected]],
      [OTHER_BASE_URL + 'mode=cors&credentials=same-origin&method=' + method +
       '&ACAOrigin=*&ACAMethods=' + method,
       [fetchResolved, hasContentLength, noServerHeader, hasBody, typeCors],
       [checkMethod]],
      [OTHER_BASE_URL + 'mode=cors&credentials=same-origin&method=' + method +
       '&ACAOrigin=*&headers=CUSTOM&ACAMethods=' + method,
       [fetchRejected]],
      [OTHER_BASE_URL + 'mode=cors&credentials=same-origin&method=' + method +
       '&ACAOrigin=*&headers=CUSTOM&ACAMethods=' + method +
       '&ACAHeaders=x-serviceworker-test',
       [fetchResolved, hasContentLength, noServerHeader, hasBody, typeCors],
       [checkMethod, hasCustomHeader]],
      [OTHER_BASE_URL + 'mode=cors&credentials=same-origin&method=' + method +
       '&ACAOrigin=*&headers=CUSTOM&ACAMethods=' + method +
       '&ACAHeaders=x-serviceworker-test' +
       '&ACEHeaders=Content-Length, X-ServiceWorker-ServerHeader',
       [fetchResolved, hasContentLength, hasServerHeader, hasBody, typeCors],
       [checkMethod, hasCustomHeader]],
      [OTHER_BASE_URL + 'mode=cors&credentials=same-origin&method=' + method +
       '&ACAOrigin=*&headers=CUSTOM&ACAMethods=PUT, XXX',
       [fetchRejected]],
      [OTHER_BASE_URL + 'mode=cors&credentials=same-origin&method=' + method +
       '&ACAOrigin=*&headers=CUSTOM&ACAMethods=PUT, XXX' +
       '&ACAHeaders=x-serviceworker-test',
       [fetchResolved, hasContentLength, noServerHeader, hasBody, typeCors],
       [checkMethod, hasCustomHeader]],
      [OTHER_BASE_URL + 'mode=cors&credentials=same-origin&method=' + method +
       '&ACAOrigin=*&headers=CUSTOM&ACAMethods=PUT, XXX' +
       '&ACAHeaders=x-serviceworker-test&ACEHeaders=Content-Length, X-ServiceWorker-ServerHeader',
       [fetchResolved, hasContentLength, hasServerHeader, hasBody, typeCors],
       [checkMethod, hasCustomHeader]],
      [OTHER_BASE_URL + 'mode=cors&credentials=same-origin&method=' + method +
       '&ACAOrigin=' + BASE_ORIGIN,
       [fetchRejected]],
      [OTHER_BASE_URL + 'mode=cors&credentials=same-origin&method=' + method +
       '&ACAOrigin=' + BASE_ORIGIN + '&ACAMethods=' + method,
       [fetchResolved, hasContentLength, noServerHeader, hasBody, typeCors],
       [checkMethod]],
      [OTHER_BASE_URL + 'mode=cors&credentials=same-origin&method=' + method +
       '&ACAOrigin=' + BASE_ORIGIN + '&headers=CUSTOM&ACAMethods=' + method,
       [fetchRejected]],
      [OTHER_BASE_URL + 'mode=cors&credentials=same-origin&method=' + method +
       '&ACAOrigin=' + BASE_ORIGIN + '&headers=CUSTOM&ACAMethods=' + method +
       '&ACAHeaders=x-serviceworker-test',
       [fetchResolved, hasContentLength, noServerHeader, hasBody, typeCors],
       [checkMethod, hasCustomHeader]],
      [OTHER_BASE_URL + 'mode=cors&credentials=same-origin&method=' + method +
       '&ACAOrigin=' + BASE_ORIGIN + '&headers=CUSTOM&ACAMethods=' + method +
       '&ACAHeaders=x-serviceworker-test' +
       '&ACEHeaders=Content-Length, X-ServiceWorker-ServerHeader',
       [fetchResolved, hasContentLength, hasServerHeader, hasBody, typeCors],
       [checkMethod, hasCustomHeader]],
      [OTHER_BASE_URL + 'mode=cors&credentials=same-origin&method=' + method +
       '&ACAOrigin=' + BASE_ORIGIN + '&headers=CUSTOM&ACAMethods=PUT, XXX',
       [fetchRejected]],
      [OTHER_BASE_URL + 'mode=cors&credentials=same-origin&method=' + method +
       '&ACAOrigin=' + BASE_ORIGIN +
       '&headers=CUSTOM&ACAMethods=PUT, XXX&ACAHeaders=x-serviceworker-test',
       [fetchResolved, hasContentLength, noServerHeader, hasBody, typeCors],
       [checkMethod, hasCustomHeader]],
      [OTHER_BASE_URL + 'mode=cors&credentials=same-origin&method=' + method +
       '&ACAOrigin=' + BASE_ORIGIN +
       '&headers=CUSTOM&ACAMethods=PUT, XXX&ACAHeaders=x-serviceworker-test' +
       '&ACEHeaders=Content-Length, X-ServiceWorker-ServerHeader',
       [fetchResolved, hasContentLength, hasServerHeader, hasBody, typeCors],
       [checkMethod, hasCustomHeader]],

      // Test that Access-Control-Allow-Methods is checked in
      // CORS preflight fetch.
      [OTHER_BASE_URL + 'mode=cors&credentials=same-origin&method=' + method +
       '&ACAOrigin=*&PACAOrigin=*&PACAMethods=' + method + '&PreflightTest=200',
       [fetchResolved, hasContentLength, noServerHeader, hasBody, typeCors],
       [checkMethod]],
      [OTHER_BASE_URL + 'mode=cors&credentials=same-origin&method=' + method +
       '&ACAOrigin=*&PACAOrigin=*&ACAMethods=' + method + '&PreflightTest=200',
       [fetchRejected]],

      // Test that Access-Control-Allow-Headers is checked in
      // CORS preflight fetch.
      [OTHER_BASE_URL + 'mode=cors&credentials=same-origin&method=' + method +
       '&headers=CUSTOM&ACAOrigin=*&PACAOrigin=*&PACAMethods=' + method +
       '&PACAHeaders=x-serviceworker-test&PreflightTest=200',
       [fetchResolved, hasContentLength, noServerHeader, hasBody, typeCors],
       [checkMethod, hasCustomHeader]],
      [OTHER_BASE_URL + 'mode=cors&credentials=same-origin&method=' + method +
       '&headers=CUSTOM&ACAOrigin=*&PACAOrigin=*&PACAMethods=' + method +
       '&ACAHeaders=x-serviceworker-test&PreflightTest=200',
       [fetchRejected]],

      // Test that CORS check is done in both preflight and main fetch.
      [OTHER_BASE_URL + 'mode=cors&credentials=same-origin&method=' + method +
       '&ACAOrigin=*&PACAMethods=' + method + '&PreflightTest=200',
       [fetchRejected]],
      [OTHER_BASE_URL + 'mode=cors&credentials=same-origin&method=' + method +
       '&PACAOrigin=*&PACAMethods=' + method + '&PreflightTest=200',
       [fetchRejected]],

      // Test that Access-Control-Expose-Headers of CORS preflight is ignored.
      [OTHER_BASE_URL + 'mode=cors&credentials=same-origin&method=' + method +
       '&ACAOrigin=*&PACAOrigin=*&PACAMethods=' + method +
       '&PACEHeaders=Content-Length, X-ServiceWorker-ServerHeader' +
       '&PreflightTest=200',
       [fetchResolved, hasContentLength, noServerHeader, hasBody, typeCors],
       [checkMethod]],

      // Test that CORS preflight with Status 2XX succeeds.
      [OTHER_BASE_URL + 'mode=cors&credentials=same-origin&method=' + method +
       '&ACAOrigin=*&PACAOrigin=*&PACAMethods=' + method + '&PreflightTest=201',
       [fetchResolved, hasContentLength, noServerHeader, hasBody, typeCors],
       [checkMethod]],

      // Test that CORS preflight with Status other than 2XX fails.
      // https://crbug.com/452394
      [OTHER_BASE_URL + 'mode=cors&credentials=same-origin&method=' + method +
       '&ACAOrigin=*&PACAOrigin=*&PACAMethods=' + method + '&PreflightTest=301',
       [fetchRejected]],
      [OTHER_BASE_URL + 'mode=cors&credentials=same-origin&method=' + method +
       '&ACAOrigin=*&PACAOrigin=*&PACAMethods=' + method + '&PreflightTest=401',
       [fetchRejected]],
      [OTHER_BASE_URL + 'mode=cors&credentials=same-origin&method=' + method +
       '&ACAOrigin=*&PACAOrigin=*&PACAMethods=' + method + '&PreflightTest=500',
       [fetchRejected]],

      // Test CORS preflight with multiple request headers.
      [OTHER_BASE_URL + 'mode=cors&credentials=same-origin&method=' + method +
       '&headers=CUSTOM2&ACAOrigin=*&PACAOrigin=*&PACAMethods=' + method +
       '&PACAHeaders=x-servicEworker-u,x-servicEworker-ua,x-servicewOrker-test,x-sErviceworker-s,x-sErviceworker-v&PreflightTest=200',
       [fetchResolved, hasContentLength, noServerHeader, hasBody, typeCors],
       [checkMethod, hasCustomHeader2]],
      [OTHER_BASE_URL + 'mode=cors&credentials=same-origin&method=' + method +
       '&headers=CUSTOM2&ACAOrigin=*&PACAOrigin=*&PACAMethods=' + method +
       '&PACAHeaders=x-servicewOrker-test&PreflightTest=200',
       [fetchRejected]],

      // Test request headers sent in CORS preflight requests.
      [OTHER_BASE_URL + 'mode=cors&credentials=same-origin&method=' + method +
       '&headers=CUSTOM&ACAOrigin=*&PACAOrigin=*&PACAMethods=' + method +
       '&PACAHeaders=x-serviceworker-test&PACRMethod=' + method +
       '&PACRHeaders=x-serviceworker-test&PreflightTest=200',
       [fetchResolved, hasContentLength, noServerHeader, hasBody, typeCors],
       [checkMethod, hasCustomHeader]],

      // Verify that Access-Control-Request-Headers: is not present in preflight
      // if its value is the empty list - https://crbug.com/633729
      [OTHER_BASE_URL + 'mode=cors&credentials=same-origin&method=' + method +
       '&headers=SAFE&ACAOrigin=*&PACAOrigin=*&PACAMethods=' + method +
       '&PACRHeaders=missing&PACRMethod=' + method + '&PreflightTest=200',
       [fetchResolved, hasContentLength, noServerHeader, hasBody, typeCors],
       [checkMethod]],

      // Test Access-Control-Request-Headers is sorted https://crbug.com/452391
      [OTHER_BASE_URL + 'mode=cors&credentials=same-origin&method=' + method +
       '&headers=CUSTOM2&ACAOrigin=*&PACAOrigin=*&PACAMethods=' + method +
       '&PACAHeaders=x-servicEworker-u,x-servicEworker-ua,x-servicewOrker-test,x-sErviceworker-s,x-sErviceworker-v&PACRMethod=' + method +
       '&PACRHeaders=x-serviceworker-s,x-serviceworker-test,x-serviceworker-u,x-serviceworker-ua,x-serviceworker-v&PreflightTest=200',
       [fetchResolved, hasContentLength, noServerHeader, hasBody, typeCors],
       [checkMethod, hasCustomHeader2]]);
  });

if (self.importScripts) {
  executeTests(TEST_TARGETS);
  done();
}
