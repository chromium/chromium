if (self.importScripts) {
  importScripts('/fetch/resources/fetch-test-helpers.js');
  importScripts('/fetch/resources/thorough-util.js');
}

var url = "data:application/json,report({jsonpResult: 'success'});";
var {BASE_URL, REDIRECT_URL, OTHER_REDIRECT_URL} = get_thorough_test_options();

var TEST_TARGETS = [
// data: requests.
  [BASE_URL + 'url=' + encodeURIComponent(url) + '&mode=same-origin&method=GET',
   [fetchResolved, noContentLength, hasContentType, noServerHeader, hasBody,
    typeBasic],
   [checkJsonpSuccess]],
  [BASE_URL + 'url=' + encodeURIComponent(url) + '&mode=cors&method=GET',
   [fetchResolved, noContentLength, hasContentType, noServerHeader, hasBody,
    typeBasic],
   [checkJsonpSuccess]],
  [BASE_URL + 'url=' + encodeURIComponent(url) + '&mode=no-cors&method=GET',
   [fetchResolved, noContentLength, hasContentType, noServerHeader, hasBody,
    typeBasic],
   [checkJsonpSuccess]],

// data: requests with non-GET methods.
  [BASE_URL + 'url=' + encodeURIComponent(url) +
   '&mode=same-origin&method=POST',
   [fetchResolved, noContentLength, hasContentType, noServerHeader, hasBody,
    typeBasic],
   [checkJsonpSuccess]],

// data: requests with same-origin redirects.
  [REDIRECT_URL + encodeURIComponent(url) + '&mode=same-origin&method=GET',
   [fetchRejected]],
  [REDIRECT_URL + encodeURIComponent(url) + '&mode=cors&method=GET',
   [fetchRejected]],

// data: requests with cross-origin redirects.
  [OTHER_REDIRECT_URL + encodeURIComponent(url) +
   '&mode=same-origin&method=GET&ACAOrigin=*',
   [fetchRejected]],
  [OTHER_REDIRECT_URL + encodeURIComponent(url) +
   '&mode=cors&method=GET&ACAOrigin=*',
   [fetchRejected]],
];

if (self.importScripts) {
  executeTests(TEST_TARGETS);
  done();
}
