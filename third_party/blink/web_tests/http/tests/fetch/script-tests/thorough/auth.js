if (self.importScripts) {
  importScripts('/fetch/resources/fetch-test-helpers.js');
  importScripts('/fetch/resources/thorough-util.js');
}

var TEST_TARGETS = [
  // Auth check
  [BASE_URL + 'Auth&mode=same-origin&credentials=omit',
   [fetchResolved, hasBody], [checkJsonpError]],
  [BASE_URL + 'Auth&mode=same-origin&credentials=include',
   [fetchResolved, hasBody], [authCheck1]],
  [BASE_URL + 'Auth&mode=same-origin&credentials=same-origin',
   [fetchResolved, hasBody], [authCheck1]],

  [BASE_URL + 'Auth&mode=cors&credentials=omit',
   [fetchResolved, hasBody], [checkJsonpError]],
  [BASE_URL + 'Auth&mode=cors&credentials=include',
   [fetchResolved, hasBody], [authCheck1]],
  [BASE_URL + 'Auth&mode=cors&credentials=same-origin',
   [fetchResolved, hasBody], [authCheck1]],

  [OTHER_BASE_URL + 'Auth&mode=same-origin&credentials=omit',
   [fetchRejected]],
  [OTHER_BASE_URL + 'Auth&mode=same-origin&credentials=include',
   [fetchRejected]],
  [OTHER_BASE_URL + 'Auth&mode=same-origin&credentials=same-origin',
   [fetchRejected]],

  // CORS check tests
  // Spec: https://fetch.spec.whatwg.org/#concept-cors-check

  // If origin is null or failure, return failure.
  [OTHER_BASE_URL + 'Auth&mode=cors&credentials=omit',
   [fetchRejected]],
  [OTHER_BASE_URL + 'Auth&mode=cors&credentials=include',
   [fetchRejected]],
  [OTHER_BASE_URL + 'Auth&mode=cors&credentials=same-origin',
   [fetchRejected]],

  // If credentials mode is not include,
  // success if ACAOrigin is * or request's origin, or failure otherwise.
  [OTHER_BASE_URL + 'Auth&mode=cors&credentials=omit&ACAOrigin=*',
   [fetchResolved, hasBody], [checkJsonpError]],
  [OTHER_BASE_URL + 'Auth&mode=cors&credentials=omit&ACAOrigin=' +
   BASE_ORIGIN,
   [fetchResolved, hasBody], [checkJsonpError]],
  [OTHER_BASE_URL + 'Auth&mode=cors&credentials=omit&ACAOrigin=http://www.example.com',
   [fetchRejected]],

  // If credentials mode is include,
  // success if ACAOrigin is request's origin and ACACredentials=true,
  // or failure otherwise.
  [OTHER_BASE_URL + 'Auth&mode=cors&credentials=include&ACAOrigin=*',
   [fetchRejected]],
  [OTHER_BASE_URL + 'Auth&mode=cors&credentials=include&ACAOrigin=' +
   BASE_ORIGIN,
   [fetchRejected]],
  [OTHER_BASE_URL + 'Auth&mode=cors&credentials=include&ACAOrigin=http://www.example.com',
   [fetchRejected]],
  [OTHER_BASE_URL + 'Auth&mode=cors&credentials=include&ACAOrigin=*&ACACredentials=true',
   [fetchRejected]],
  [OTHER_BASE_URL + 'Auth&mode=cors&credentials=include&ACAOrigin=' +
   BASE_ORIGIN + '&ACACredentials=true',
   [fetchResolved, hasBody, typeCors], [onlyForCrossSiteCookieTest(authCheck2)]],
  [OTHER_BASE_URL + 'Auth&mode=cors&credentials=include&ACAOrigin=http://www.example.com&ACACredentials=true',
   [fetchRejected]],

  // Test that Access-Control-Allow-Credentials is case-sensitive.
  [OTHER_BASE_URL + 'Auth&mode=cors&credentials=include&ACAOrigin=' +
   BASE_ORIGIN + '&ACACredentials=True',
   [fetchRejected]],

  // If credentials mode is not include,
  // success if ACAOrigin is * or request's origin, or failure otherwise.
  [OTHER_BASE_URL + 'Auth&mode=cors&credentials=same-origin&ACAOrigin=*',
   [fetchResolved, hasBody], [checkJsonpError]],
  [OTHER_BASE_URL + 'Auth&mode=cors&credentials=same-origin&ACAOrigin=' +
   BASE_ORIGIN,
   [fetchResolved, hasBody], [checkJsonpError]],
  [OTHER_BASE_URL + 'Auth&mode=cors&credentials=same-origin&ACAOrigin=http://www.example.com',
   [fetchRejected]],

  // Credential check with CORS preflight.

  // Resolved because Authentication is not applied to CORS preflight.
  [OTHER_BASE_URL + 'Auth&mode=cors&credentials=omit&ACAOrigin=*&PACAOrigin=*&method=PUT&PACAMethods=PUT&PreflightTest=200',
   [fetchResolved, hasBody], [checkJsonpError]],
  [OTHER_BASE_URL + 'Auth&mode=cors&credentials=same-origin&ACAOrigin=*&PACAOrigin=*&method=PUT&PACAMethods=PUT&PreflightTest=200',
   [fetchResolved, hasBody], [checkJsonpError]],
  [OTHER_BASE_URL + 'Auth&mode=cors&credentials=include&ACAOrigin=' +
   BASE_ORIGIN + '&PACAOrigin=' + BASE_ORIGIN +
   '&ACACredentials=true&PACACredentials=true&method=PUT&PACAMethods=PUT&PreflightTest=200',
   [fetchResolved, hasBody, typeCors], [onlyForCrossSiteCookieTest(authCheck2)]],

  // Rejected because CORS preflight response returns 401.
  [OTHER_BASE_URL + 'PAuth&mode=cors&credentials=omit&ACAOrigin=*&PACAOrigin=*&method=PUT&PACAMethods=PUT&PreflightTest=200',
   [fetchRejected]],
  [OTHER_BASE_URL + 'PAuth&mode=cors&credentials=same-origin&ACAOrigin=*&pACAOrigin=*&method=PUT&PACAMethods=PUT&PreflightTest=200',
   [fetchRejected]],
  [OTHER_BASE_URL + 'PAuth&mode=cors&credentials=include&ACAOrigin=' +
   BASE_ORIGIN + '&PACAOrigin=' + BASE_ORIGIN +
   '&ACACredentials=true&PACACredentials=true&method=PUT&PACAMethods=PUT&PreflightTest=200',
   [fetchRejected]],

  // Check that Access-Control-Allow-Origin/Access-Control-Allow-Credentials
  // headers are checked in both CORS preflight and main fetch.
  [OTHER_BASE_URL + 'Auth&mode=cors&credentials=include&ACAOrigin=' +
   BASE_ORIGIN +
   '&ACACredentials=true&PACACredentials=true&method=PUT&PACAMethods=PUT&PreflightTest=200',
   [fetchRejected]],
  [OTHER_BASE_URL + 'Auth&mode=cors&credentials=include&PACAOrigin=' +
   BASE_ORIGIN +
   '&ACACredentials=true&PACACredentials=true&method=PUT&PACAMethods=PUT&PreflightTest=200',
   [fetchRejected]],
  [OTHER_BASE_URL +
   'Auth&mode=cors&credentials=include&ACAOrigin=*&PACAOrigin=' + BASE_ORIGIN +
   '&ACACredentials=true&PACACredentials=true&method=PUT&PACAMethods=PUT&PreflightTest=200',
   [fetchRejected]],
  [OTHER_BASE_URL + 'Auth&mode=cors&credentials=include&ACAOrigin=' +
   BASE_ORIGIN +
   '&PACAOrigin=*&ACACredentials=true&PACACredentials=true&method=PUT&PACAMethods=PUT&PreflightTest=200',
   [fetchRejected]],
  [OTHER_BASE_URL + 'Auth&mode=cors&credentials=include&ACAOrigin=' +
   BASE_ORIGIN + '&PACAOrigin=' + BASE_ORIGIN +
   '&ACACredentials=true&method=PUT&PACAMethods=PUT&PreflightTest=200',
   [fetchRejected]],
  [OTHER_BASE_URL + 'Auth&mode=cors&credentials=include&ACAOrigin=' +
   BASE_ORIGIN + '&PACAOrigin=' + BASE_ORIGIN +
   '&PACACredentials=true&method=PUT&PACAMethods=PUT&PreflightTest=200',
   [fetchRejected]],
];

if (self.importScripts) {
  executeTests(TEST_TARGETS);
  done();
}
