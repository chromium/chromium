function get_thorough_test_options() {
  var options = get_fetch_test_options();
  var BASE_URL = options['BASE_ORIGIN'] +
      '/serviceworker/resources/fetch-access-control.php?';
  var OTHER_BASE_URL = options['OTHER_ORIGIN'] +
      '/serviceworker/resources/fetch-access-control.php?';
  var SCOPE = options['BASE_ORIGIN'] +
      '/fetch/resources/thorough-iframe.html?' + options['TEST_OPTIONS'];
  return Object.assign({
    BASE_URL: BASE_URL,
    OTHER_BASE_URL: OTHER_BASE_URL,
    SCOPE: SCOPE,
    IFRAME_ORIGIN: options['BASE_ORIGIN'],
    BASE_URL_WITH_USERNAME: BASE_URL.replace('://', '://user@'),
    OTHER_BASE_URL_WITH_USERNAME: OTHER_BASE_URL.replace('://', '://user@'),
    BASE_URL_WITH_PASSWORD: BASE_URL.replace('://', '://user:pass@'),
    OTHER_BASE_URL_WITH_PASSWORD: OTHER_BASE_URL.replace('://', '://user:pass@'),
    REDIRECT_URL: options['BASE_ORIGIN'] +
        '/serviceworker/resources/redirect.php?Redirect=',
    OTHER_REDIRECT_URL: options['OTHER_ORIGIN'] +
        '/serviceworker/resources/redirect.php?Redirect=',
    REDIRECT_LOOP_URL: options['BASE_ORIGIN'] +
        '/fetch/resources/redirect-loop.php?Redirect=',
    OTHER_REDIRECT_LOOP_URL: options['OTHER_ORIGIN'] +
        '/fetch/resources/redirect-loop.php?Redirect=',
    IFRAME_URL: SCOPE,
    WORKER_URL: options['BASE_ORIGIN'] +
        '/fetch/resources/thorough-worker.js?' + options['TEST_OPTIONS']
    }, options);
}

function onlyOnServiceWorkerProxiedTest(checkFuncs) {
  return [];
}

// Functions to check the result from the ServiceWorker.
var checkFetchResult = function(expected, url, data) {
  assert_equals(data.fetchResult, expected, url + ' should be ' + expected);
};
var checkFetchResponseBody = function(hasBody, url, data) {
  assert_equals(data.fetchResult,
                'resolved',
                'fetchResult must be resolved. url: ' + url);
  if (hasBody) {
    assert_not_equals(data.body, '',
                      'response must have body. url: ' + url);
  } else {
    assert_equals(data.body, '',
                  'response must not have body. url: ' + url);
  }
};
var checkFetchResponseHeader = function(name, expected, url, data) {
  assert_equals(data.fetchResult,
                'resolved',
                'fetchResult must be resolved. url: ' + url);
  var exist = false;
  for (var i = 0; i < data.headers.length; ++i) {
    if (data.headers[i][0] === name) {
      exist = true;
    }
  }
  assert_equals(exist,
                expected,
                'header check failed url: ' + url + ' name: ' + name);
};
var checkFetchResponseType = function(type, url, data) {
  assert_equals(data.fetchResult,
                'resolved',
                'fetchResult must be resolved. url = ' + url);
  assert_equals(data.type,
                type,
                'type must match. url: ' + url);
};
var checkFetchResponseRedirected = function(expected, url, data) {
  assert_equals(data.fetchResult,
                'resolved',
                'fetchResult must be resolved. url = ' + url);
  assert_equals(data.redirected,
                expected,
                url + ' redirected flag should match');
};
var checkURLList = function(redirectedURLList, url, data) {
  if (!self.internals)
    return;
  var expectedURLList = [url].concat(redirectedURLList);
  assert_equals(data.fetchResult,
                'resolved',
                'fetchResult must be resolved. url = ' + url);
  assert_array_equals(data.urlList,
                      expectedURLList,
                      url + ' URL list should match');
};

var showComment = function(url, data) {
  assert_true(!data.comment, 'Show comment: ' + data.comment + ' url: ' + url);
}

var fetchIgnored = checkFetchResult.bind(this, 'ignored');
var fetchResolved = checkFetchResult.bind(this, 'resolved');
var fetchRejected = checkFetchResult.bind(this, 'rejected');
var fetchError = checkFetchResult.bind(this, 'error');
var hasBody = checkFetchResponseBody.bind(this, true);
var noBody = checkFetchResponseBody.bind(this, false);
var hasContentLength =
  checkFetchResponseHeader.bind(this, 'content-length', true);
var noContentLength =
  checkFetchResponseHeader.bind(this, 'content-length', false);
var hasContentType =
  checkFetchResponseHeader.bind(this, 'content-type', true);
var noContentType =
  checkFetchResponseHeader.bind(this, 'content-type', false);
var hasServerHeader =
  checkFetchResponseHeader.bind(this, 'x-serviceworker-serverheader', true);
var noServerHeader =
  checkFetchResponseHeader.bind(this, 'x-serviceworker-serverheader', false);
var typeBasic = checkFetchResponseType.bind(this, 'basic');
var typeCors = checkFetchResponseType.bind(this, 'cors');
var typeOpaque = checkFetchResponseType.bind(this, 'opaque');
var typeOpaqueredirect = checkFetchResponseType.bind(this, 'opaqueredirect');
var responseRedirected = checkFetchResponseRedirected.bind(this, true);
var responseNotRedirected = checkFetchResponseRedirected.bind(this, false);

// Functions to check the result of JSONP which is evaluated in
// thorough-iframe.html by appending <script> element.
var checkJsonpResult = function(expected, url, data) {
  assert_equals(data.jsonpResult,
                expected,
                url + ' jsonpResult should match');
};
var checkJsonpHeader = function(name, value, url, data) {
  assert_equals(data.jsonpResult,
                'success',
                url + ' jsonpResult must be success');
  assert_equals(data.headers[name],
                value,
                'Request header check failed url:' + url + ' name:' + name);
};
var checkJsonpMethod = function(method, url, data) {
  assert_equals(data.jsonpResult,
                'success',
                url + ' jsonpResult must be success');
  assert_equals(data.method,
                method,
                'Method must match url:' + url);
};
var checkJsonpAuth = function(username, password, cookie, url, data) {
  assert_equals(data.jsonpResult,
                'success',
                url + ' jsonpResult must be success');
  assert_equals(data.username,
                username,
                'Username must match. url: ' + url);
  assert_equals(data.password,
                password,
                'Password must match. url: ' + url);
  assert_equals(data.cookie,
                cookie,
                'Cookie must match. url: ' + url);
};
var checkJsonpCookie = function(cookie, url, data) {
  assert_equals(data.jsonpResult,
                'success',
                url + ' jsonpResult must be success');
  assert_equals(data.cookie,
                cookie,
                'Cookie must match. url: ' + url);
};
var checkJsonpError = checkJsonpResult.bind(this, 'error');
var checkJsonpSuccess = checkJsonpResult.bind(this, 'success');
var checkJsonpNoRedirect = checkJsonpResult.bind(this, 'noredirect');
var hasCustomHeader =
  checkJsonpHeader.bind(this, 'X-ServiceWorker-Test', 'test');
var hasCustomHeader2 = function(url, data) {
  checkJsonpHeader('X-ServiceWorker-s', 'test1', url, data);
  checkJsonpHeader('X-ServiceWorker-Test', 'test2, test3', url, data);
  checkJsonpHeader('X-ServiceWorker-ua', 'test4', url, data);
  checkJsonpHeader('X-ServiceWorker-U', 'test5', url, data);
  checkJsonpHeader('X-ServiceWorker-V', 'test6', url, data);
};
var noCustomHeader =
  checkJsonpHeader.bind(this, 'X-ServiceWorker-Test', undefined);
var methodIsGET = checkJsonpMethod.bind(this, 'GET');
var methodIsPOST = checkJsonpMethod.bind(this, 'POST');
var methodIsPUT = checkJsonpMethod.bind(this, 'PUT');
var methodIsXXX = checkJsonpMethod.bind(this, 'XXX');
var authCheckNone =
  checkJsonpAuth.bind(this, 'undefined', 'undefined', 'undefined');
var authCheck1 = checkJsonpAuth.bind(this, 'username1', 'password1', 'cookie1');
var authCheck2 = checkJsonpAuth.bind(this, 'username2', 'password2', 'cookie2');

var cookieCheck1 = checkJsonpCookie.bind(this, 'cookie1');
var cookieCheck2 = checkJsonpCookie.bind(this, 'cookie2');
var cookieCheckA = checkJsonpCookie.bind(this, 'cookieA');
var cookieCheckB = checkJsonpCookie.bind(this, 'cookieB');
var cookieCheckC = checkJsonpCookie.bind(this, 'cookieC');
var cookieCheckNone = checkJsonpCookie.bind(this, 'undefined');

if (location.href.indexOf('base-https') >= 0)
  authCheck1 = checkJsonpAuth.bind(this, 'username1s', 'password1s', 'cookie1');

if (location.href.indexOf('other-https') >= 0)
  authCheck2 = checkJsonpAuth.bind(this, 'username2s', 'password2s', 'cookie2');

function executeServiceWorkerProxiedTests(test_targets, thorough_options) {
  var test = async_test('Verify access control of fetch() in a Service Worker');
  var {WORKER_URL, IFRAME_ORIGIN, SCOPE} = thorough_options;
  test.step(function() {
      var worker = undefined;
      var frameWindow = {};
      var counter = 0;
      window.addEventListener('message', test.step_func(onMessage), false);

      Promise.resolve()
        .then(function() {
            return service_worker_unregister_and_register(test,
                                                          WORKER_URL,
                                                          SCOPE);
          })
        .then(function(registration) {
            worker = registration.installing;
            var messageChannel = new MessageChannel();
            messageChannel.port1.onmessage = test.step_func(onWorkerMessage);
            worker.postMessage(
              {port: messageChannel.port2}, [messageChannel.port2]);
            return wait_for_state(test, worker, 'activated');
          })
        .then(function() {
            return with_iframe(SCOPE);
          })
        .then(function(frame) {
            frameWindow = frame.contentWindow;
            // Start tests.
            loadNext();
          })
        .catch(unreached_rejection(test));

      var readyFromWorkerReceived = undefined;
      var resultFromWorkerReceived = undefined;
      var resultFromIframeReceived = undefined;

      function onMessage(e) {
        // The message is sent from thorough-iframe.html in report()
        // which is called by appending <script> element which source code is
        // generated by fetch-access-control.php.
        if (test_targets[counter][2]) {
          test_targets[counter][2].forEach(function(checkFunc) {
              checkFunc.call(this, test_targets[counter][0], e.data);
            });
        }
        resultFromIframeReceived();
      }

      function onWorkerMessage(e) {
        // The message is sent from the ServiceWorker.
        var message = e.data;
        if (message.msg === 'READY') {
          readyFromWorkerReceived();
          return;
        }
        var checks = test_targets[counter][1].concat(showComment);
        checks.forEach(function(checkFunc) {
            checkFunc.call(this, test_targets[counter][0], message);
          });
        resultFromWorkerReceived();
      }

      function loadNext() {
        var workerPromise = new Promise(function(resolve, reject) {
            resultFromWorkerReceived = resolve;
          });
        var iframePromise = new Promise(function(resolve, reject) {
            resultFromIframeReceived = resolve;
          });
        Promise.all([workerPromise, iframePromise])
          .then(test.step_func(function() {
              ++counter;
              if (counter === test_targets.length) {
                service_worker_unregister_and_done(test, SCOPE);
              } else {
                loadNext();
              }
            }));
        (new Promise(function(resolve, reject) {
            readyFromWorkerReceived = resolve;
            worker.postMessage({msg: 'START TEST CASE'});
          }))
          .then(test.step_func(function() {
              frameWindow.postMessage(
                {url: test_targets[counter][0]},
                IFRAME_ORIGIN);
            }));
      }
    });
}

function getQueryParams(url) {
  var search = (new URL(url)).search;
  if (!search) {
    return {};
  }
  var ret = {};
  var params = search.substring(1).split('&');
  params.forEach(function(param) {
      var element = param.split('=');
      ret[decodeURIComponent(element[0])] = decodeURIComponent(element[1]);
    });
  return ret;
}

function getRequestInit(params) {
  var init = {};
  if (params['method']) {
    init['method'] = params['method'];
  }
  if (params['mode']) {
    init['mode'] = params['mode'];
  }
  if (params['redirectmode']) {
    init['redirect'] = params['redirectmode'];
  }
  if (params['credentials']) {
    init['credentials'] = params['credentials'];
  }
  if (params['headers'] === 'CUSTOM') {
    init['headers'] = {'X-ServiceWorker-Test': 'test'};
  } else if (params['headers'] === 'CUSTOM2') {
    init['headers'] = [['X-ServiceWorker-Test', 'test2'],
                       ['X-ServiceWorker-ua', 'test4'],
                       ['X-ServiceWorker-V', 'test6'],
                       ['X-ServiceWorker-s', 'test1'],
                       ['X-ServiceWorker-Test', 'test3'],
                       ['X-ServiceWorker-U', 'test5']];
  } else if (params['headers'] === 'SAFE') {
    init['headers'] = [['Accept', '*/*'],
                       ['Accept-Language', 'en-us,de'],
                       ['Content-Language', 'en-us'],
                       ['Content-Type', 'text/plain'],
                       ['Save-data', 'on']];
  } else if (params['headers'] === '{}') {
    init['headers'] = {};
  }
  return init;
}

function headersToArray(headers) {
  var ret = [];

  // Workaround for Firefox. iterable is not implemented yet.
  // This is used only by checkFetchResponseHeader, and
  // checkFetchResponseHeader is used only for the header names listed below.
  // FIXME: Replace it with the original code below when Firefox supports
  // iterable.
  ['content-length', 'content-type', 'x-serviceworker-serverheader'].forEach(
    function(name) {
      for (var header of headers){
        ret.push(header);
      }
    });

  return ret;
}

function doFetch(request) {
  var originalURL = request.url;
  var params = getQueryParams(originalURL);
  var init = getRequestInit(params);
  var url = params['url'];
  try {
    if (url) {
      request = new Request(url, init);
    } else {
      request = new Request(request, init);
    }
    var response;
    return fetch(request)
      .then(function(res) {
          response = res;
          return res.clone().text()
            .then(function(body) {
                return Promise.resolve({
                  // Setting a comment will fail the test and show the
                  // comment in the result. Use this for debugging
                  // tests.
                  comment: undefined,

                  fetchResult: 'resolved',
                  body: body,
                  status: response.status,
                  headers: headersToArray(response.headers),
                  type: response.type,
                  redirected: response.redirected,
                  urlList: self.internals ?
                           self.internals.getInternalResponseURLList(response) :
                           [],
                  response: response,
                  originalURL: originalURL
                });
              })
            .catch(function(e) {
                return Promise.resolve({fetchResult: 'error'});
              });
        })
      .catch(function(e) {
          return Promise.resolve({fetchResult: 'rejected'});
        });
  } catch (e) {
    return Promise.resolve({fetchResult: 'error'});
  }
}

var report_data = {};
function report(data) {
  report_data = data;
}

// |test_target| is an array. The first element of |test_target| is the URL to
// be fetched. The second element of |test_target| is an array of test functions
// which will be called with the result of doFetch(). The third element of
// |test_target| is an array of test functions which will be called with
// |report_data| set by report() which is called while executing
// "eval(message.body)".
function executeTest(test_target) {
  if (test_target.length == 0) {
    return Promise.resolve();
  }
  return doFetch(new Request(test_target[0],
                             {credentials: 'same-origin', mode: 'no-cors'}))
    .then(function(message) {
        var checks = test_target[1].concat(showComment);
        checks.forEach(function(checkFunc) {
            checkFunc.call(this, test_target[0], message);
          });

        if (test_target[2]) {
          report_data = {};
          if (message.fetchResult !== 'resolved' ||
              message.body === '' ||
              400 <= message.status) {
            report({jsonpResult:'error'});
          } else {
            eval(message.body);
          }
          assert_not_equals(report_data, {}, 'data should be set');

          test_target[2].forEach(function(checkFunc) {
              checkFunc.call(this, test_target[0], report_data);
            });
        }
      });
}

function executeTests(test_targets) {
  for (var i = 0; i < test_targets.length; ++i) {
    promise_test(
      function(counter, t) {
        return executeTest(test_targets[counter]);
      }.bind(this, i),
      "executeTest-" + i);
  }
}
