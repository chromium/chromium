if (self.importScripts) {
  importScripts('/fetch/resources/fetch-test-helpers.js');
  importScripts('/fetch/resources/thorough-util.js');
}

var {BASE_URL, OTHER_BASE_URL} = get_thorough_test_options();
var TEST_TARGETS = [
  // Test that default mode is no-cors in serviceworker-proxied tests.
  onlyOnServiceWorkerProxiedTest(
    [BASE_URL + 'method=GET',
     [fetchResolved, hasContentLength, hasServerHeader, hasBody, typeBasic],
     [methodIsGET, authCheck1]]),
  onlyOnServiceWorkerProxiedTest(
    [BASE_URL + 'method=GET&headers={}',
     [fetchResolved, hasContentLength, hasServerHeader, hasBody, typeBasic],
     [methodIsGET]]),
  onlyOnServiceWorkerProxiedTest(
    [BASE_URL + 'method=GET&headers=CUSTOM',
     [fetchResolved, hasContentLength, hasServerHeader, hasBody, typeBasic],
     [methodIsGET, noCustomHeader]]),
  onlyOnServiceWorkerProxiedTest(
    [BASE_URL + 'method=POST&headers=CUSTOM',
     [fetchResolved, hasContentLength, hasServerHeader, hasBody, typeBasic],
     [methodIsPOST, noCustomHeader]]),
  onlyOnServiceWorkerProxiedTest(
    [BASE_URL + 'method=PUT',
     [fetchError]]),
  onlyOnServiceWorkerProxiedTest(
    [BASE_URL + 'method=XXX',
     [fetchError]]),

  onlyOnServiceWorkerProxiedTest(
    [OTHER_BASE_URL + 'method=GET&headers=CUSTOM',
     [fetchResolved, noContentLength, noServerHeader, noBody, typeOpaque],
     [methodIsGET, noCustomHeader, authCheck2]]),
  onlyOnServiceWorkerProxiedTest(
    [OTHER_BASE_URL + 'method=POST&headers=CUSTOM',
     [fetchResolved, noContentLength, noServerHeader, noBody, typeOpaque],
     [methodIsPOST, noCustomHeader]]),
  onlyOnServiceWorkerProxiedTest(
    [OTHER_BASE_URL + 'method=PUT&headers=CUSTOM',
     [fetchError]]),
  onlyOnServiceWorkerProxiedTest(
    [OTHER_BASE_URL + 'method=XXX&headers=CUSTOM',
     [fetchError]]),

  // Test mode=no-cors.
  [BASE_URL + 'mode=no-cors&method=GET',
   [fetchResolved, hasContentLength, hasServerHeader, hasBody, typeBasic],
   [methodIsGET, authCheck1]],
  [BASE_URL + 'mode=no-cors&method=GET&headers={}',
   [fetchResolved, hasContentLength, hasServerHeader, hasBody, typeBasic],
   [methodIsGET]],
  [BASE_URL + 'mode=no-cors&method=GET&headers=CUSTOM',
   [fetchResolved, hasContentLength, hasServerHeader, hasBody, typeBasic],
   [methodIsGET, noCustomHeader]],
  [BASE_URL + 'mode=no-cors&method=POST&headers=CUSTOM',
   [fetchResolved, hasContentLength, hasServerHeader, hasBody, typeBasic],
   [methodIsPOST, noCustomHeader]],
  [BASE_URL + 'mode=no-cors&method=PUT',
   [fetchError]],
  [BASE_URL + 'mode=no-cors&method=XXX',
   [fetchError]],

  [OTHER_BASE_URL + 'mode=no-cors&method=GET&headers=CUSTOM',
   [fetchResolved, noContentLength, noServerHeader, noBody, typeOpaque],
   onlyOnServiceWorkerProxiedTest([methodIsGET, noCustomHeader, authCheck2])],
  [OTHER_BASE_URL + 'mode=no-cors&method=POST&headers=CUSTOM',
   [fetchResolved, noContentLength, noServerHeader, noBody, typeOpaque],
   onlyOnServiceWorkerProxiedTest([methodIsPOST, noCustomHeader])],
  [OTHER_BASE_URL + 'mode=no-cors&method=PUT&headers=CUSTOM',
   [fetchError]],
  [OTHER_BASE_URL + 'mode=no-cors&method=XXX&headers=CUSTOM',
   [fetchError]],
];

if (self.importScripts) {
  executeTests(TEST_TARGETS);
  done();
}
