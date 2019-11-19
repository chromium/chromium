if (self.importScripts) {
  importScripts('/fetch/resources/fetch-test-helpers.js');
  importScripts('/fetch/resources/thorough-util.js');
}

function createExpectedURLList(prefix, count, last) {
  var array = [];
  for (var i = count; i > 0; --i)
    array.push(prefix + i);
  array.push(last);
  return array;
}

var TEST_TARGETS = [
  // Redirect loop: same origin -> same origin
  [REDIRECT_LOOP_URL + encodeURIComponent(BASE_URL) + '&Count=20&mode=cors' +
   '&credentials=same-origin',
   [fetchResolved, hasContentLength, hasBody, typeBasic,
    responseRedirected,
    checkURLList.bind(
        self,
        createExpectedURLList(
            REDIRECT_LOOP_URL + encodeURIComponent(BASE_URL) + '&Count=',
            19, BASE_URL))],
   [methodIsGET, authCheck1]],
  [REDIRECT_LOOP_URL + encodeURIComponent(BASE_URL) + '&Count=21&mode=cors' +
   '&credentials=same-origin',
   [fetchRejected]],

  // Redirect loop: same origin -> other origin
  [REDIRECT_LOOP_URL + encodeURIComponent(OTHER_BASE_URL + '&ACAOrigin=*') +
   '&Count=20&mode=cors&credentials=same-origin&method=GET',
   [fetchResolved, hasContentLength, noServerHeader, hasBody, typeCors,
    responseRedirected,
    checkURLList.bind(
        self,
        createExpectedURLList(
            REDIRECT_LOOP_URL +
            encodeURIComponent(OTHER_BASE_URL + '&ACAOrigin=') + '%2A&Count=',
            19,
            OTHER_BASE_URL + '&ACAOrigin=*'))],
   [methodIsGET, authCheckNone]],
  [REDIRECT_LOOP_URL + encodeURIComponent(OTHER_BASE_URL + '&ACAOrigin=*') +
   '&Count=21&mode=cors&credentials=same-origin&method=GET',
   [fetchRejected]],

  // Redirect loop: other origin -> same origin
  [OTHER_REDIRECT_LOOP_URL + encodeURIComponent(BASE_URL + 'ACAOrigin=*') +
   '&Count=20&mode=cors&credentials=same-origin&method=GET&ACAOrigin=*',
   [fetchResolved, hasContentLength, noServerHeader, hasBody, typeCors,
    responseRedirected,
    checkURLList.bind(
        self,
        createExpectedURLList(
            OTHER_REDIRECT_LOOP_URL +
            encodeURIComponent(BASE_URL + 'ACAOrigin=') +
            '%2A&ACAOrigin=*&Count=',
            19,
            BASE_URL + 'ACAOrigin=*'))],
   [methodIsGET, authCheckNone]],
  [OTHER_REDIRECT_LOOP_URL + encodeURIComponent(BASE_URL + 'ACAOrigin=*') +
   '&Count=21&mode=cors&credentials=same-origin&method=GET&ACAOrigin=*',
   [fetchRejected]],

  // Redirect loop: other origin -> other origin
  [OTHER_REDIRECT_LOOP_URL +
   encodeURIComponent(OTHER_BASE_URL + 'ACAOrigin=*') +
   '&Count=20&mode=cors&credentials=same-origin&method=GET&ACAOrigin=*',
   [fetchResolved, hasContentLength, noServerHeader, hasBody, typeCors,
    responseRedirected,
    checkURLList.bind(
        self,
        createExpectedURLList(
            OTHER_REDIRECT_LOOP_URL +
            encodeURIComponent(OTHER_BASE_URL + 'ACAOrigin=') +
            '%2A&ACAOrigin=*&Count=',
            19,
            OTHER_BASE_URL + 'ACAOrigin=*'))],
   [methodIsGET, authCheckNone]],
  [OTHER_REDIRECT_LOOP_URL +
   encodeURIComponent(OTHER_BASE_URL + 'ACAOrigin=*') +
   '&Count=21&mode=cors&credentials=same-origin&method=GET&ACAOrigin=*',
   [fetchRejected]],
];

if (self.importScripts) {
  executeTests(TEST_TARGETS);
  done();
}
