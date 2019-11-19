if (self.importScripts) {
  importScripts('/fetch/resources/fetch-test-helpers.js');
  importScripts('/fetch/resources/thorough-util.js');
}

var TEST_TARGETS = [
  // Redirect: same origin -> same origin
  // Credential test
  [REDIRECT_URL + encodeURIComponent(BASE_URL) +
   '&mode=cors&credentials=omit&method=GET',
   [fetchResolved, hasContentLength, hasServerHeader, hasBody, typeBasic],
   [methodIsGET, authCheckNone]],
  [REDIRECT_URL + encodeURIComponent(BASE_URL) +
   '&mode=cors&credentials=include&method=GET',
   [fetchResolved, hasContentLength, hasServerHeader, hasBody, typeBasic],
   [methodIsGET, authCheck1]],
  [REDIRECT_URL + encodeURIComponent(BASE_URL) +
   '&mode=cors&credentials=same-origin&method=GET',
   [fetchResolved, hasContentLength, hasServerHeader, hasBody, typeBasic],
   [methodIsGET, authCheck1]],

  // Redirect: same origin -> other origin
  // Credential test
  [REDIRECT_URL + encodeURIComponent(OTHER_BASE_URL) +
   '&mode=cors&credentials=omit&method=GET',
   [fetchRejected]],
  [REDIRECT_URL + encodeURIComponent(OTHER_BASE_URL) +
   '&mode=cors&credentials=include&method=GET',
   [fetchRejected]],
  [REDIRECT_URL + encodeURIComponent(OTHER_BASE_URL) +
   '&mode=cors&credentials=same-origin&method=GET',
   [fetchRejected]],

  [REDIRECT_URL +
   encodeURIComponent(OTHER_BASE_URL + '&ACAOrigin=' + BASE_ORIGIN + '') +
   '&mode=cors&credentials=omit&method=GET',
   [fetchResolved, hasContentLength, noServerHeader, hasBody, typeCors],
   [methodIsGET, authCheckNone]],
  [REDIRECT_URL +
   encodeURIComponent(OTHER_BASE_URL +
                      '&ACAOrigin=' + BASE_ORIGIN + '&ACACredentials=true') +
   '&mode=cors&credentials=omit&method=GET',
   [fetchResolved, hasContentLength, noServerHeader, hasBody, typeCors],
   [methodIsGET, authCheckNone]],

  [REDIRECT_URL +
   encodeURIComponent(OTHER_BASE_URL + '&ACAOrigin=' + BASE_ORIGIN + '') +
   '&mode=cors&credentials=include&method=GET',
   [fetchRejected]],
  [REDIRECT_URL +
   encodeURIComponent(OTHER_BASE_URL +
                      '&ACAOrigin=' + BASE_ORIGIN + '&ACACredentials=true') +
   '&mode=cors&credentials=include&method=GET',
   [fetchResolved, hasContentLength, noServerHeader, hasBody, typeCors],
   [methodIsGET, onlyForCrossSiteCookieTest(authCheck2)]],

  [REDIRECT_URL +
   encodeURIComponent(OTHER_BASE_URL + '&ACAOrigin=' + BASE_ORIGIN + '') +
   '&mode=cors&credentials=same-origin&method=GET',
   [fetchResolved, hasContentLength, noServerHeader, hasBody, typeCors],
   [methodIsGET, authCheckNone]],
  [REDIRECT_URL +
   encodeURIComponent(OTHER_BASE_URL +
                      '&ACAOrigin=' + BASE_ORIGIN + '&ACACredentials=true') +
   '&mode=cors&credentials=same-origin&method=GET',
   [fetchResolved, hasContentLength, noServerHeader, hasBody, typeCors],
   [methodIsGET, authCheckNone]],

  // Redirect: other origin -> same origin
  // Credentials test
  [OTHER_REDIRECT_URL + encodeURIComponent(BASE_URL + 'ACAOrigin=*') +
   '&mode=cors&credentials=omit&method=GET&ACAOrigin=*',
   [fetchResolved, hasContentLength, noServerHeader, hasBody, typeCors],
   [methodIsGET, authCheckNone]],
  [OTHER_REDIRECT_URL + encodeURIComponent(BASE_URL + 'ACAOrigin=*') +
   '&mode=cors&credentials=include&method=GET&ACAOrigin=*',
   [fetchRejected]],
  [OTHER_REDIRECT_URL + encodeURIComponent(BASE_URL + 'ACAOrigin=*') +
   '&mode=cors&credentials=same-origin&method=GET&ACAOrigin=*',
   [fetchResolved, hasContentLength, noServerHeader, hasBody, typeCors],
   [methodIsGET, authCheckNone]],
  [OTHER_REDIRECT_URL +
   encodeURIComponent(BASE_URL + 'ACAOrigin=null&ACACredentials=true') +
   '&mode=cors&credentials=omit&method=GET' +
   '&ACAOrigin=' + BASE_ORIGIN + '&ACACredentials=true',
   [fetchResolved, hasContentLength, noServerHeader, hasBody, typeCors],
   [methodIsGET, authCheckNone]],
  [OTHER_REDIRECT_URL +
   encodeURIComponent(BASE_URL + 'ACAOrigin=null&ACACredentials=true') +
   '&mode=cors&credentials=include&method=GET' +
   '&ACAOrigin=' + BASE_ORIGIN + '&ACACredentials=true',
   [fetchResolved, hasContentLength, noServerHeader, hasBody, typeCors],
   [methodIsGET, authCheck1]],
  [OTHER_REDIRECT_URL +
   encodeURIComponent(BASE_URL + 'ACAOrigin=null&ACACredentials=true') +
   '&mode=cors&credentials=same-origin&method=GET' +
   '&ACAOrigin=' + BASE_ORIGIN + '&ACACredentials=true',
   [fetchResolved, hasContentLength, noServerHeader, hasBody, typeCors],
   [methodIsGET, authCheckNone]],
];

if (self.importScripts) {
  executeTests(TEST_TARGETS);
  done();
}
