if (self.importScripts) {
  importScripts('/fetch/resources/fetch-test-helpers.js');
  importScripts('/fetch/resources/thorough-util.js');
}

var {
  REDIRECT_URL,
  OTHER_REDIRECT_URL,
  BASE_URL_WITH_PASSWORD,
  BASE_URL_WITH_USERNAME,
  OTHER_BASE_URL_WITH_PASSWORD,
  OTHER_BASE_URL_WITH_USERNAME
} = get_thorough_test_options();

var TEST_TARGETS = [
  // Redirects to URLs with username/password; these requests are blocked.
  //
  // Spec: https://github.com/whatwg/fetch/pull/465
  // Step 5, redirect status, Step 10.1 and 10.2:
  // "If |request|'s mode is "cors", |request|'s origin is not same origin with
  //  |locationURL|'s origin, and |locationURL| includes credentials, return a
  //  network error."
  // "If the CORS flag is set and |locationURL| includes credentials, return
  //  a network error."

  // Origin A -[fetch]-> Origin A -[redirect]-> Origin A
  [REDIRECT_URL + encodeURIComponent(BASE_URL_WITH_USERNAME) +
   '&mode=same-origin&method=GET',
   [fetchRejected]],
  [REDIRECT_URL + encodeURIComponent(BASE_URL_WITH_PASSWORD) +
   '&mode=same-origin&method=GET',
   [fetchRejected]],
  [REDIRECT_URL + encodeURIComponent(BASE_URL_WITH_USERNAME) +
   '&mode=cors&method=GET',
   [fetchRejected]],
  [REDIRECT_URL + encodeURIComponent(BASE_URL_WITH_PASSWORD) +
   '&mode=cors&method=GET',
   [fetchRejected]],
  [REDIRECT_URL + encodeURIComponent(BASE_URL_WITH_USERNAME) +
   '&mode=no-cors&method=GET',
   [fetchRejected]],
  [REDIRECT_URL + encodeURIComponent(BASE_URL_WITH_PASSWORD) +
   '&mode=no-cors&method=GET',
   [fetchRejected]],

  // Origin A -[fetch]-> Origin A -[redirect]-> Origin B
  [REDIRECT_URL +
   encodeURIComponent(OTHER_BASE_URL_WITH_USERNAME + '&ACAOrigin=*') +
   '&mode=cors&method=GET',
   [fetchRejected]],
  [REDIRECT_URL +
   encodeURIComponent(OTHER_BASE_URL_WITH_PASSWORD + '&ACAOrigin=*') +
   '&mode=cors&method=GET',
   [fetchRejected]],
  [REDIRECT_URL +
   encodeURIComponent(OTHER_BASE_URL_WITH_USERNAME + '&ACAOrigin=*') +
   '&mode=no-cors&method=GET',
   [fetchRejected]],
  [REDIRECT_URL +
   encodeURIComponent(OTHER_BASE_URL_WITH_PASSWORD + '&ACAOrigin=*') +
   '&mode=no-cors&method=GET',
   [fetchRejected]],

  // Origin A -[fetch]-> Origin B -[redirect]-> Origin A
  [OTHER_REDIRECT_URL +
   encodeURIComponent(BASE_URL_WITH_USERNAME + 'ACAOrigin=*') +
   '&mode=cors&method=GET&ACAOrigin=*',
   [fetchRejected]],
  [OTHER_REDIRECT_URL +
   encodeURIComponent(BASE_URL_WITH_PASSWORD + 'ACAOrigin=*') +
   '&mode=cors&method=GET&ACAOrigin=*',
   [fetchRejected]],
  [OTHER_REDIRECT_URL +
   encodeURIComponent(BASE_URL_WITH_USERNAME + 'ACAOrigin=*') +
   '&mode=no-cors&method=GET&ACAOrigin=*',
   [fetchRejected]],
  [OTHER_REDIRECT_URL +
   encodeURIComponent(BASE_URL_WITH_PASSWORD + 'ACAOrigin=*') +
   '&mode=no-cors&method=GET&ACAOrigin=*',
   [fetchRejected]],

  // Origin A -[fetch]-> Origin B -[redirect]-> Origin B
  [OTHER_REDIRECT_URL +
   encodeURIComponent(OTHER_BASE_URL_WITH_USERNAME + 'ACAOrigin=*') +
   '&mode=cors&method=GET&ACAOrigin=*',
   [fetchRejected]],
  [OTHER_REDIRECT_URL +
   encodeURIComponent(OTHER_BASE_URL_WITH_PASSWORD + 'ACAOrigin=*') +
   '&mode=cors&method=GET&ACAOrigin=*',
   [fetchRejected]],
  [OTHER_REDIRECT_URL +
   encodeURIComponent(OTHER_BASE_URL_WITH_USERNAME + 'ACAOrigin=*') +
   '&mode=no-cors&method=GET&ACAOrigin=*',
   [fetchRejected]],
  [OTHER_REDIRECT_URL +
   encodeURIComponent(OTHER_BASE_URL_WITH_PASSWORD + 'ACAOrigin=*') +
   '&mode=no-cors&method=GET&ACAOrigin=*',
   [fetchRejected]],
];

if (self.importScripts) {
  executeTests(TEST_TARGETS);
  done();
}
