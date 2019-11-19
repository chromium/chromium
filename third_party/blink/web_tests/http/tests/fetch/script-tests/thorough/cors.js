if (self.importScripts) {
  importScripts('/fetch/resources/fetch-test-helpers.js');
  importScripts('/fetch/resources/thorough-util.js');
}

// Tests for CORS check and CORS filtered response.

var TEST_TARGETS = [
  // CORS test

  [OTHER_BASE_URL + 'mode=same-origin&method=GET', [fetchRejected]],
  [OTHER_BASE_URL + 'mode=same-origin&method=POST', [fetchRejected]],
  [OTHER_BASE_URL + 'mode=same-origin&method=PUT', [fetchRejected]],
  [OTHER_BASE_URL + 'mode=same-origin&method=XXX', [fetchRejected]],

  // method=GET

  // CORS check
  // https://fetch.spec.whatwg.org/#concept-cors-check
  // Tests for Access-Control-Allow-Origin header.
  [OTHER_BASE_URL + 'mode=cors&credentials=same-origin&method=GET',
   [fetchRejected]],
  [OTHER_BASE_URL + 'mode=cors&credentials=same-origin&method=GET&ACAOrigin=*',
   [fetchResolved, hasContentLength, noServerHeader, hasBody, typeCors],
   [methodIsGET, authCheckNone]],
  [OTHER_BASE_URL + 'mode=cors&credentials=same-origin&method=GET' +
   '&ACAOrigin=' + BASE_ORIGIN,
   [fetchResolved, hasContentLength, noServerHeader, hasBody, typeCors],
   [methodIsGET]],
  [OTHER_BASE_URL + 'mode=cors&credentials=same-origin&method=GET&' +
   'ACAOrigin=' + BASE_ORIGIN + ',http://www.example.com',
   [fetchRejected]],
  [OTHER_BASE_URL + 'mode=cors&credentials=same-origin&method=GET' +
   '&ACAOrigin=http://www.example.com',
   [fetchRejected]],

  // CORS filtered response
  // https://fetch.spec.whatwg.org/#concept-filtered-response-cors
  // Tests for Access-Control-Expose-Headers header.
  [OTHER_BASE_URL + 'mode=cors&credentials=same-origin&method=GET&ACAOrigin=*' +
   '&ACEHeaders=X-ServiceWorker-ServerHeader',
   [fetchResolved, hasContentLength, hasServerHeader, hasBody, typeCors],
   [methodIsGET]],
  [OTHER_BASE_URL + 'mode=cors&credentials=same-origin&method=GET' +
   '&ACAOrigin=' + BASE_ORIGIN +
   '&ACEHeaders=X-ServiceWorker-ServerHeader',
   [fetchResolved, hasContentLength, hasServerHeader, hasBody, typeCors],
   [methodIsGET]],
  [OTHER_BASE_URL + 'mode=cors&credentials=same-origin&method=GET' +
   '&ACAOrigin=*&ACEHeaders=Content-Length, X-ServiceWorker-ServerHeader',
   [fetchResolved, hasContentLength, hasServerHeader, hasBody, typeCors],
   [methodIsGET]],
  [OTHER_BASE_URL + 'mode=cors&credentials=same-origin&method=GET' +
   '&ACAOrigin=' + BASE_ORIGIN +
   '&ACEHeaders=Content-Length, X-ServiceWorker-ServerHeader',
   [fetchResolved, hasContentLength, hasServerHeader, hasBody, typeCors],
   [methodIsGET]],

  // method=POST

  // CORS check
  // https://fetch.spec.whatwg.org/#concept-cors-check
  // Tests for Access-Control-Allow-Origin header.
  [OTHER_BASE_URL + 'mode=cors&credentials=same-origin&method=POST',
   [fetchRejected]],
  [OTHER_BASE_URL + 'mode=cors&credentials=same-origin&method=POST&ACAOrigin=*',
   [fetchResolved, hasContentLength, noServerHeader, hasBody, typeCors],
   [methodIsPOST]],
  [OTHER_BASE_URL + 'mode=cors&credentials=same-origin&method=POST' +
   '&ACAOrigin=' + BASE_ORIGIN,
   [fetchResolved, hasContentLength, noServerHeader, hasBody, typeCors],
   [methodIsPOST]],
  [OTHER_BASE_URL + 'mode=cors&credentials=same-origin&method=POST' +
   '&ACAOrigin=' + BASE_ORIGIN +
   ',http://www.example.com',
   [fetchRejected]],
  [OTHER_BASE_URL + 'mode=cors&credentials=same-origin&method=POST' +
   '&ACAOrigin=http://www.example.com',
   [fetchRejected]],

  // CORS filtered response
  // https://fetch.spec.whatwg.org/#concept-filtered-response-cors
  // Tests for Access-Control-Expose-Headers header.
  [OTHER_BASE_URL + 'mode=cors&credentials=same-origin&method=POST' +
   '&ACAOrigin=*&ACEHeaders=X-ServiceWorker-ServerHeader',
   [fetchResolved, hasContentLength, hasServerHeader, hasBody, typeCors],
   [methodIsPOST]],
  [OTHER_BASE_URL + 'mode=cors&credentials=same-origin&method=POST' +
   '&ACAOrigin=' + BASE_ORIGIN + '&ACEHeaders=X-ServiceWorker-ServerHeader',
   [fetchResolved, hasContentLength, hasServerHeader, hasBody, typeCors],
   [methodIsPOST]],
  [OTHER_BASE_URL + 'mode=cors&credentials=same-origin&method=POST' +
   '&ACAOrigin=*&ACEHeaders=Content-Length, X-ServiceWorker-ServerHeader',
   [fetchResolved, hasContentLength, hasServerHeader, hasBody, typeCors],
   [methodIsPOST]],
  [OTHER_BASE_URL + 'mode=cors&credentials=same-origin&method=POST' +
   '&ACAOrigin=' + BASE_ORIGIN +
   '&ACEHeaders=Content-Length, X-ServiceWorker-ServerHeader',
   [fetchResolved, hasContentLength, hasServerHeader, hasBody, typeCors],
   [methodIsPOST]],
];

if (self.importScripts) {
  executeTests(TEST_TARGETS);
  done();
}
