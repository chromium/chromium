if (self.importScripts) {
  importScripts('../resources/fetch-test-helpers.js');
}

var {BASE_ORIGIN, OTHER_ORIGIN} = get_fetch_test_options();
var URL = 'https://www.example.com/test.html';

test(function() {
    var headers = new Headers;
    headers.set('User-Agent', 'Mozilla/5.0');
    headers.set('Accept', 'text/html');
    headers.set('X-Fetch-Test', 'request test field');

    var request = new Request(URL, {method: 'GET', headers: headers});

    assert_equals(request.url, URL, 'Request.url should match');
    assert_equals(request.method, 'GET', 'Request.method should match');
    assert_equals(request.referrer, 'about:client',
                  'Request.referrer should be about:client');
    assert_equals(request.referrerPolicy, '',
                  'Request.referrerPolicy should be empty');
    assert_true(request.headers instanceof Headers,
                'Request.headers should be Headers');

    // 'User-Agent' is a forbidden header.
    assert_equals(size(request.headers), 2,
                  'Request.headers size should match');
    // Note: detailed behavioral tests for Headers are in another test,
    // http/tests/fetch/*/headers.html.

    request.url = 'http://localhost/';
    assert_equals(request.url, 'https://www.example.com/test.html',
                  'Request.url should be readonly');

    // Unmatched lead surrogate.
    request = new Request('http://localhost/\uD800');
    assert_equals(request.url,
                  'http://localhost/' + encodeURIComponent('\uFFFD'),
                  'Request.url should have unmatched surrogates replaced.');

    request.method = 'POST';
    assert_equals(request.method, 'GET', 'Request.method should be readonly');
  }, 'Request basic test');

test(function() {
    [new Request(URL),
    new Request(URL, {method: undefined}),
    new Request(URL, {mode: undefined}),
    new Request(URL, {credentials: undefined})]
      .forEach(function(request) {
          assert_equals(request.url, URL,
                        'Request.url should match');
          assert_equals(request.method, 'GET',
                        'Default Request.method should be GET');
          assert_equals(request.mode, 'cors',
                        'Default Request.mode should be cors');
          assert_equals(request.credentials, 'same-origin',
                        'Default Request.credentials should be same-origin');
      });
}, "Request default value test");

test(function() {
     // All below values are invalid and thus should throw
     [{mode: null},
     {mode: 'sameorigin'},
     {mode: 'same origin'},
     {mode: 'same-origin\0'},
     {mode: ' same-origin'},
     {mode: 'same--origin'},
     {mode: 'SAME-ORIGIN'},
     {mode: 'nocors'},
     {mode: 'no cors'},
     {mode: 'no-cors\0'},
     {mode: ' no-cors'},
     {mode: 'no--cors'},
     {mode: 'NO-CORS'},
     {mode: 'cors\0'},
     {mode: ' cors'},
     {mode: 'co rs'},
     {mode: 'CORS'},
     {mode: 'navigate\0'},
     {mode: ' navigate'},
     {mode: 'navi gate'},
     {mode: 'NAVIGATE'},
     {mode: '\0'.repeat(100000)},
     {mode: 'x'.repeat(100000)},
     {credentials: null},
     {credentials: 'omit\0'},
     {credentials: ' omit'},
     {credentials: 'om it'},
     {credentials: 'OMIT'},
     {credentials: 'sameorigin'},
     {credentials: 'same origin'},
     {credentials: 'same-origin\0'},
     {credentials: ' same-origin'},
     {credentials: 'same--origin'},
     {credentials: 'SAME-ORIGIN'},
     {credentials: 'include\0'},
     {credentials: ' include'},
     {credentials: 'inc lude'},
     {credentials: 'INCLUDE'},
     {credentials: '\0'.repeat(100000)},
     {credentials: 'x'.repeat(100000)},
     {cache: null},
     {cache: 'default\0'},
     {cache: ' default'},
     {cache: 'def ault'},
     {cache: 'DEFAULT'},
     {cache: 'nostore'},
     {cache: 'no store'},
     {cache: 'no-store\0'},
     {cache: ' no-store'},
     {cache: 'no--store'},
     {cache: 'NO-STORE'},
     {cache: 'reload\0'},
     {cache: ' reload'},
     {cache: 're load'},
     {cache: 'RELOAD'},
     {cache: 'nocache'},
     {cache: 'no cache'},
     {cache: 'no-cache\0'},
     {cache: ' no-cache'},
     {cache: 'no--cache'},
     {cache: 'NO-CACHE'},
     {cache: 'forcecache'},
     {cache: 'force cache'},
     {cache: 'force-cache\0'},
     {cache: ' force-cache'},
     {cache: 'force--cache'},
     {cache: 'FORCE-CACHE'},
     {cache: 'onlyifcached'},
     {cache: 'only if cached'},
     {cache: 'only-if-cached\0'},
     {cache: ' only-if-cached'},
     {cache: 'only-if--cached'},
     {cache: 'only--if-cached'},
     {cache: 'ONLY-IF-CACHED'},
     {cache: '\0'.repeat(100000)},
     {cache: 'x'.repeat(100000)},
     {redirect: 'follow\0'},
     {redirect: ' follow'},
     {redirect: 'fol low'},
     {redirect: 'FOLLOW'},
     {redirect: 'error\0'},
     {redirect: ' error'},
     {redirect: 'er ror'},
     {redirect: 'ERROR'},
     {redirect: 'manual\0'},
     {redirect: ' manual'},
     {redirect: 'man ual'},
     {redirect: 'MANUAL'},
     {redirect: '\0'.repeat(100000)},
     {redirect: 'x'.repeat(100000)}]
      .concat(INVALID_TOKENS.map(
          function(name) { return {mode: name}; }))
      .concat(INVALID_TOKENS.map(
          function(name) { return {credentials: name}; }))
      .concat(INVALID_TOKENS.map(
          function(name) { return {cache: name}; }))
      .concat(INVALID_TOKENS.map(
          function(name) { return {redirect: name}; }))
      .forEach((init) => assert_throws_js(TypeError, () => new Request(URL, init)),
        'Invalid Request.{mode, credentials, cache, redirect} should throw a TypeError');
  }, 'Request invalid value test');

test(function() {
    var request = new Request(URL);
    request.headers.append('X-Fetch-Foo', 'foo1');
    request.headers.append('X-Fetch-Foo', 'foo2');
    request.headers.append('X-Fetch-Bar', 'bar');
    var request2 = new Request(request);
    assert_equals(request2.url, URL, 'Request.url should match');
    assert_equals(request2.method, 'GET', 'Request.method should match');
    assert_equals(request2.mode, 'cors', 'Request.mode should match');
    assert_equals(request2.credentials, 'same-origin',
                  'Request.credentials should match');
    assert_equals(request2.headers.get('X-Fetch-Foo').split(', ')[0], 'foo1',
                  'Request.headers should match');
    assert_equals(request2.headers.get('X-Fetch-Foo').split(', ')[1], 'foo2',
                  'Request.headers should match');
    assert_equals(request2.headers.get('X-Fetch-Bar').split(', ')[0], 'bar',
                  'Request.headers should match');
    var request3 = new Request(URL,
                               {headers: [['X-Fetch-Foo', 'foo1'],
                                          ['X-Fetch-Foo', 'foo2'],
                                          ['X-Fetch-Bar', 'bar']]});
    assert_equals(request3.headers.get('X-Fetch-Foo').split(', ')[0], 'foo1',
                  'Request.headers should match');
    assert_equals(request3.headers.get('X-Fetch-Foo').split(', ')[1], 'foo2',
                  'Request.headers should match');
    assert_equals(request3.headers.get('X-Fetch-Bar').split(', ')[0], 'bar',
                  'Request.headers should match');
    var request4 = new Request(URL,
                               {headers: {'X-Fetch-Foo': 'foo1',
                                          'X-Fetch-Foo': 'foo2',
                                          'X-Fetch-Bar': 'bar'}});
    assert_equals(request4.headers.get('X-Fetch-Foo').split(', ')[0], 'foo2',
                  'Request.headers should match');
    assert_equals(request4.headers.get('X-Fetch-Bar').split(', ')[0], 'bar',
                  'Request.headers should match');
    // https://github.com/whatwg/fetch/issues/479
    var request5 = new Request(request, {headers: undefined});
    assert_equals(request5.headers.get('X-Fetch-Foo').split(', ')[0], 'foo1',
                  'Request.headers should match');
    assert_equals(request5.headers.get('X-Fetch-Foo').split(', ')[1], 'foo2',
                  'Request.headers should match');
    assert_equals(request5.headers.get('X-Fetch-Bar').split(', ')[0], 'bar',
                  'Request.headers should match');
    var request6 = new Request(request, {});
    assert_equals(request6.headers.get('X-Fetch-Foo').split(', ')[0], 'foo1',
                  'Request.headers should match');
    assert_equals(request6.headers.get('X-Fetch-Foo').split(', ')[1], 'foo2',
                  'Request.headers should match');
    assert_equals(request6.headers.get('X-Fetch-Bar').split(', ')[0], 'bar',
                  'Request.headers should match');
    assert_throws_js(TypeError,
                     () => { new Request(request, {headers: null}) },
                     'null cannot be converted to a HeaderInit');
  }, 'Request header test');

test(function() {
    var request1 = {};
    var request2 = {};
    var METHODS = ['GET', 'HEAD', 'POST', 'PUT', 'DELETE', 'OPTIONS',
                   undefined];
    var MODES = ['same-origin', 'no-cors', 'cors', undefined];
    function isSimpleMethod(method) {
      return ['GET', 'HEAD', 'POST', undefined].indexOf(method) != -1;
    };
    function effectiveMethod(method1, method2) {
      return method2 ? method2 : (method1 ? method1 : 'GET');
    };
    function effectiveMode(mode1, mode2) {
      return mode2 ? mode2 : (mode1 ? mode1 : 'cors');
    };
    METHODS.forEach(function(method1) {
        MODES.forEach(function(mode1) {
            var init1 = {};
            if (method1 != undefined) { init1['method'] = method1; }
            if (mode1 != undefined) { init1['mode'] = mode1; }
            if (!isSimpleMethod(method1) && mode1 == 'no-cors') {
              assert_throws_js(
                TypeError,
                function() { request1 = new Request(URL, init1); },
                'new no-cors Request with non simple method (' + method1 +
                ') should throw');
              return;
            }
            request1 = new Request(URL, init1);
            assert_equals(request1.method, method1 ? method1 : 'GET',
                          'Request.method should match');
            assert_equals(request1.mode, mode1 ? mode1 : 'cors',
                          'Request.mode should match');
            request1 = new Request(request1);
            assert_equals(request1.method, method1 ? method1 : 'GET',
                          'Request.method should match');
            assert_equals(request1.mode, mode1 ? mode1 : 'cors',
                          'Request.mode should match');
            METHODS.forEach(function(method2) {
                MODES.forEach(function(mode2) {
                    // We need to construct a new request1 because as soon as it
                    // is used in a constructor it will be flagged as 'used',
                    // and we can no longer construct objects with it.
                    request1 = new Request(URL, init1);
                    var init2 = {};
                    if (method2 != undefined) { init2['method'] = method2; }
                    if (mode2 != undefined) { init2['mode'] = mode2; }
                    if (!isSimpleMethod(effectiveMethod(method1, method2)) &&
                        effectiveMode(mode1, mode2) == 'no-cors') {
                      assert_throws_js(
                        TypeError,
                        function() { request2 = new Request(request1, init2); },
                        'new no-cors Request with non simple method should ' +
                        'throw');
                      return;
                    }
                    request2 = new Request(request1, init2);
                    assert_equals(request2.method,
                                  method2 ? method2 : request1.method,
                                  'Request.method should be overridden');
                    assert_equals(request2.mode,
                                  mode2 ? mode2 : request1.mode,
                                  'Request.mode should be overridden');
                  });
              });
          });
      });
  }, 'Request method test');

test(function() {
    var request1 = {};
    var request2 = {};
    var CREDENTIALS = ['omit', 'same-origin', 'include', undefined];
    CREDENTIALS.forEach(function(credentials1) {
        var init1 = {};
        if (credentials1 != undefined) { init1['credentials'] = credentials1; }
        request1 = new Request(URL, init1);
        assert_equals(request1.credentials, credentials1 || 'same-origin',
                      'Request.credentials should match');
        request1 = new Request(request1);
        assert_equals(request1.credentials, credentials1 || 'same-origin',
                      'Request.credentials should match');
        CREDENTIALS.forEach(function(credentials2) {
            request1 = new Request(URL, init1);
            var init2 = {};
            if (credentials2 != undefined) {
              init2['credentials'] = credentials2;
            }
            request2 = new Request(request1, init2);
            assert_equals(request2.credentials,
                          credentials2 ? credentials2 : request1.credentials,
                          'Request.credentials should be overridden');
          });
      });
  }, 'Request credentials test');

test(function() {
    var request1 = {};
    var request2 = {};
    var REDIRECTS = ['follow', 'error', 'manual', undefined];
    REDIRECTS.forEach(function(redirect1) {
        var init1 = {};
        if (redirect1 != undefined) { init1['redirect'] = redirect1; }
        request1 = new Request(URL, init1);
        assert_equals(request1.redirect, redirect1 || 'follow',
                      'Request.redirect should match');
        request1 = new Request(request1);
        assert_equals(request1.redirect, redirect1 || 'follow',
                      'Request.redirect should match');
        REDIRECTS.forEach(function(redirect2) {
            request1 = new Request(URL, init1);
            var init2 = {};
            if (redirect2 != undefined) {
              init2['redirect'] = redirect2;
            }
            request2 = new Request(request1, init2);
            assert_equals(request2.redirect,
                          redirect2 ? redirect2 : request1.redirect,
                          'Request.redirect should be overridden');
          });
      });
  }, 'Request redirect test');

test(function() {
    var request1 = {};
    var request2 = {};
    var init = {};
    request1 = new Request(URL, init);
    assert_equals(request1.integrity, '',
                  'Request.integrity should be empty on init');
    init['integrity'] = 'sha256-deadbeef';
    request1 = new Request(URL, init);
    assert_equals(request1.integrity, 'sha256-deadbeef',
                  'Request.integrity match the integrity of init');
    request2 = new Request(request1);
    assert_equals(request2.integrity, 'sha256-deadbeef',
                  'Request.integrity should match');
}, 'Request integrity test');

test(function() {
    ['same-origin', 'cors', 'no-cors'].forEach(function(mode) {
        FORBIDDEN_METHODS.forEach(function(method) {
            assert_throws_js(
              TypeError,
              function() {
                var request = new Request(URL, {mode: mode, method: method});
              },
              'new Request with a forbidden method (' + method + ') should ' +
              'throw');
          });
        INVALID_METHOD_NAMES.forEach(function(name) {
            assert_throws_js(
              TypeError,
              function() {
                var request = new Request(URL, {mode: mode, method: name});
              },
              'new Request with an invalid method (' + name + ') should throw');
          });
      });
  }, 'Request method name throw test');

test(function() {
    assert_throws_js(
      TypeError,
      function() {
        var request = new Request(URL, {mode: 'navigate'});
      },
      'new Request with a navigate mode should throw');
  }, 'Request mode throw test');

test(function() {
    var url = 'http://example.com';
    TO_BE_NORMALIZED_METHOD_NAMES.forEach(
      function(method) {
        assert_equals(new Request(url, {method: method.toUpperCase()}).method,
                      method.toUpperCase(),
                      'method must match: ' + method);
        assert_equals(new Request(url, {method: method}).method,
                      method.toUpperCase(),
                      'method should be normalized to uppercase: ' + method);
      });

    OTHER_VALID_METHOD_NAMES.forEach(
      function(method) {
        assert_equals(new Request(url, {method: method}).method, method,
                      'method should not be changed when normalized: ' +
                      method);
        method = method.toLowerCase();
        assert_equals(new Request(url, {method: method}).method, method,
                      'method should not be changed when normalized: ' +
                      method);
      });
  }, 'Request: valid method names and normalize test');

test(function() {
    assert_throws_js(TypeError,
                     function() { new Request('http://user@localhost/'); },
                     'Request with a URL with username must throw.');
    assert_throws_js(TypeError,
                     function() { new Request('http://user:pass@localhost/'); },
                     'Request with a URL with username and password must throw.');
  }, 'Request construction with URLs with credentials.');

test(function() {
    var req = new Request(URL);
    assert_false(req.bodyUsed,
                 'Request should not be flagged as used if it has not been ' +
                 'consumed.');
    var req2 = new Request(req);
    assert_false(req.bodyUsed,
                 'Request should not be flagged as used if it does not ' +
                 'have body.');
    assert_false(req2.bodyUsed,
                 'Request should not be flagged as used if it has not been ' +
                 'consumed.');
  }, 'Request construction without body behavior regardning "bodyUsed"');

test(function() {
    var req = new Request(URL, {method: 'POST', body: 'hello'});
    assert_false(req.bodyUsed,
                 'Request should not be flagged as used if it has not been ' +
                 'consumed.');
    var req2 = new Request(req);
    assert_true(req.bodyUsed,
                'Request should be flagged as used if it has been consumed.');
    assert_false(req2.bodyUsed,
                 'Request should not be flagged as used if it has not been ' +
                 'consumed.');
    // See https://crbug.com/501195.
    assert_throws_js(
      TypeError,
      function() { new Request(req); },
      'Request construction should throw if used.');
  }, 'POST Request construction without body behavior regardning "bodyUsed"');

test(function() {
    var req = new Request(URL, {method: 'POST', body: 'hello'});
    assert_false(req.bodyUsed,
                 'Request should not be flagged as used if it has not been ' +
                 'consumed.');
    assert_throws_js(
      TypeError,
      function() { new Request(req, {method: 'GET'}); },
      'A get request may not have body.');

    assert_false(req.bodyUsed, 'After the GET case');

    assert_throws_js(
      TypeError,
      function() { new Request(req, {method: 'CONNECT'}); },
      'Request() with a forbidden method must throw.');

    assert_false(req.bodyUsed, 'After the forbidden method case');

    var req2 = new Request(req);
    assert_true(req.bodyUsed,
                'Request should be flagged as used if it has been consumed.');
  }, 'Request construction failure should not set "bodyUsed"');

test(function() {
    assert_equals(new Request(URL).referrer, 'about:client');
    assert_equals(new Request(URL).referrerPolicy, '');
  }, 'Request without RequestInit.');

test(function() {
  assert_equals(new Request(URL, {referrer: undefined}).referrer,
               'about:client');
  assert_equals(new Request(URL).referrerPolicy, '');
}, 'Request with referrer equals to undefined.');

test(function() {
  var expected = location.href.slice(0, location.href.lastIndexOf('/')) +
    '/null';
  assert_equals(new Request(URL, {referrer: null}).referrer, expected);
  assert_equals(new Request(URL).referrerPolicy, '');
}, 'Request with referrer equals to null.');

test(function() {
    var req = new Request(URL, {referrer: 'about:client'});

    assert_equals(req.referrer, 'about:client',
                  'constructed with referrer=about:client');
    assert_equals(new Request(req.clone()).referrer, 'about:client',
                 'cloned from a request with referrer=about:client');
    assert_equals(new Request(req.clone(), {foo: null}).referrer,
                  'about:client',
                  'constructed from a request with referrer=about:client');
    assert_equals(new Request(req.clone(), {method: 'GET'}).referrer,
                  'about:client',
                  'constructed with method from a request with ' +
                  'referrer=about:client');
  }, 'Request with referrer=about:client.');

test(function() {
    var req = new Request(URL, {referrer: ''});

    assert_equals(req.referrer, '', 'constructed with no-referrer');
    assert_equals(new Request(req.clone()).referrer, '',
                  'cloned from a request with no-referrer');
    assert_equals(new Request(req.clone(), {foo: null}).referrer, '',
                  'constructed from a request with no-referrer');
    assert_equals(new Request(req.clone(), {method: 'GET'}).referrer,
                  'about:client',
                  'constructed with method from a request with no-referrer');
  }, 'Request with no-referrer.');

test(function() {
    var referrer = BASE_ORIGIN + '/path?query';
    var req = new Request(URL, {referrer: referrer});

    assert_equals(req.referrer, referrer, 'constructed with a url referrer');
    assert_equals(req.clone().referrer, referrer,
                  'cloned from a request with a url referrer');
    assert_equals(new Request(req.clone(), {foo: null}).referrer, referrer,
                  'constructed from a request with a url referrer');
    assert_equals(new Request(req.clone(), {method: 'GET'}).referrer,
                  'about:client',
                  'constructed with method from a request with a url referrer');
  }, 'Request with a url referrer');

test(function() {
    var referrer =
        (BASE_ORIGIN + '/path/?query#hash').replace('//', '//user:pass@');
    var req = new Request(URL, {referrer: referrer});
    assert_equals(req.referrer, referrer, 'constructed with a url referrer');
  }, 'Request with a url referrer containing user, pass, and so on');

test(function() {
    var referrer = OTHER_ORIGIN + '/path?query';

    assert_equals(new Request(URL, {referrer: referrer}).referrer, 'about:client',
                      'constructed with cross-origin referrer');
  }, 'Request with a url with another origin');

test(function() {
    var referrer = 'https://example[invalid].com/';
    assert_throws_js(TypeError,
        () => new Request(URL, {referrer: referrer}));
  }, 'Request with an invalid referrer');

test(function() {
    var referrer = '/path?query';
    var expected = BASE_ORIGIN + '/path?query';

    assert_equals(new Request(URL, {referrer: referrer}).referrer, expected);
  }, 'Request with a relative referrer');

test(() => {
    assert_equals(new Request('/', {referrerPolicy: ''}).referrerPolicy, '');
    assert_equals(new Request('/', {referrerPolicy: 'no-referrer'})
        .referrerPolicy, 'no-referrer');
    assert_equals(new Request('/',
        {referrerPolicy: 'no-referrer-when-downgrade'}).referrerPolicy,
        'no-referrer-when-downgrade');
    assert_equals(new Request('/', {referrerPolicy: 'origin'})
        .referrerPolicy, 'origin');
    assert_equals(new Request('/', {referrerPolicy: 'origin-when-cross-origin'})
        .referrerPolicy, 'origin-when-cross-origin');
    assert_equals(new Request('/', {referrerPolicy: 'unsafe-url'})
        .referrerPolicy, 'unsafe-url');
    assert_throws_js(
        TypeError,
        () => new Request('/', {referrerPolicy: 'invalid'}),
        'Setting invalid referrer policy should be thrown.');
  }, 'Referrer policy settings');

test(() => {
     // All referrer policies below are invalid and should throw
     [{referrerPolicy: null},
     {referrerPolicy: 'noreferrer'},
     {referrerPolicy: 'no referrer'},
     {referrerPolicy: 'no-referrer\0'},
     {referrerPolicy: ' no-referrer'},
     {referrerPolicy: 'no--referrer'},
     {referrerPolicy: 'NO-REFERRER'},
     {referrerPolicy: 'noreferrerwhendowngrade'},
     {referrerPolicy: 'no referrer when downgrade'},
     {referrerPolicy: 'no-referrer-when-downgrade\0'},
     {referrerPolicy: ' no-referrer-when-downgrade'},
     {referrerPolicy: 'no--referrer--when--downgrade'},
     {referrerPolicy: 'NO-REFERRER-WHEN-DOWNGRADE'},
     {referrerPolicy: 'sameorigin'},
     {referrerPolicy: 'same origin'},
     {referrerPolicy: 'same-origin\0'},
     {referrerPolicy: ' same-origin'},
     {referrerPolicy: 'same--origin'},
     {referrerPolicy: 'SAME-ORIGIN'},
     {referrerPolicy: 'origin\0'},
     {referrerPolicy: ' origin'},
     {referrerPolicy: 'ori gin'},
     {referrerPolicy: 'ORIGIN'},
     {referrerPolicy: 'strictorigin'},
     {referrerPolicy: 'strict origin'},
     {referrerPolicy: 'strict-origin\0'},
     {referrerPolicy: ' strict-origin'},
     {referrerPolicy: 'strict--origin'},
     {referrerPolicy: 'STRICT-ORIGIN'},
     {referrerPolicy: 'originwhencrossorigin'},
     {referrerPolicy: 'origin when cross origin'},
     {referrerPolicy: 'origin-when-cross-origin\0'},
     {referrerPolicy: ' origin-when-cross-origin'},
     {referrerPolicy: 'origin--when--cross--origin'},
     {referrerPolicy: 'ORIGIN-WHEN-CROSS-ORIGIN'},
     {referrerPolicy: 'strictoriginwhencrossorigin'},
     {referrerPolicy: 'strict origin when cross origin'},
     {referrerPolicy: 'strict-origin-when-cross-origin\0'},
     {referrerPolicy: ' strict-origin-when-cross-origin'},
     {referrerPolicy: 'strict--origin--when--cross--origin'},
     {referrerPolicy: 'STRICT-ORIGIN-WHEN-CROSS-ORIGIN'},
     {referrerPolicy: 'unsafeurl'},
     {referrerPolicy: 'unsafe url'},
     {referrerPolicy: 'unsafe-url\0'},
     {referrerPolicy: ' unsafe-url'},
     {referrerPolicy: 'unsafe--url'},
     {referrerPolicy: 'UNSAFE-URL'},
     {referrerPolicy: '\0'.repeat(100000)},
     {referrerPolicy: 'x'.repeat(100000)}]
      .forEach((init) => assert_throws_js(TypeError, () => new Request(URL, init)),
        'Invalid Request.referrerPolicy should throw a TypeError');
  }, 'Request invalid referrer policy test');

test(() => {
    let r = new Request('/', {referrerPolicy: 'origin'});
    assert_equals(r.referrerPolicy, 'origin', 'original policy');

    assert_equals(new Request(r, {foo: 32}).referrerPolicy,
        'origin', 'kept original policy');
    assert_equals(new Request(r, {mode: 'cors'}).referrerPolicy,
        '', 'cleared policy');
    assert_equals(new Request(r, {referrerPolicy: 'unsafe-url'}).referrerPolicy,
        'unsafe-url', 'overriden policy');
  }, 'Referrer policy should be cleared when any members are set');

// Spec: https://fetch.spec.whatwg.org/#dom-request
// Step 21:
// If request's method is `GET` or `HEAD`, throw a TypeError.
test(function() {
    var headers = new Headers;
    headers.set('Content-Language', 'ja');
    ['GET', 'HEAD'].forEach(function(method) {
        assert_throws_js(
          TypeError,
          function() {
            new Request(URL,
                        {method: method,
                         body: new Blob(['Test Blob'], {type: 'test/type'})
                        });
          },
          'Request of GET/HEAD method cannot have RequestInit body.');
      });
  }, 'Request of GET/HEAD method cannot have RequestInit body.');

test(() => {
    var req = new Request(URL, {method: 'POST', body: 'hello'});
    req.text();
    assert_true(req.bodyUsed);
    assert_throws_js(TypeError, () => { req.clone(); });
  }, 'Used => clone');

test(() => {
  // We used to implement RequestInit manually so we needed to test this
  // functionality here. We now generate RequestInit with the IDL compiler,
  // but it's still good to keep these around.
  function undefined_notpresent(property_name) {
    assert_not_equals(property_name, 'referrer', 'property_name');
    const request = new Request('/', {referrer: '/'});
    let init = {};
    init[property_name] = undefined;
    assert_equals((new Request(request, init)).referrer, request.url,
                  property_name);
  }

  undefined_notpresent('method');
  undefined_notpresent('headers');
  undefined_notpresent('body');
  undefined_notpresent('referrerPolicy');
  undefined_notpresent('mode');
  undefined_notpresent('credentials');
  undefined_notpresent('cache');
  undefined_notpresent('redirect');
  undefined_notpresent('integrity');
  undefined_notpresent('keepalive');
  undefined_notpresent('signal');
  undefined_notpresent('window');

  // |undefined_notpresent| uses referrer for testing, so we need to test
  // the property manually.
  const request = new Request('/', {referrerPolicy: 'same-origin'});
  assert_equals(new Request(request, {referrer: undefined}).referrerPolicy,
                'same-origin', 'referrer');
}, 'An undefined member should be treated as not-present');

test(() => {
  // We used to implement RequestInit manually so we needed to test this
  // functionality here. We now generate RequestInit with the IDL compiler,
  // but it's still good to keep these around.
  const e = Error();
  assert_throws_exactly(e, () => {
    new Request('/', {get method() { throw e; }})}, 'method');
  assert_throws_exactly(e, () => {
    new Request('/', {get headers() { throw e; }})}, 'headers');
  assert_throws_exactly(e, () => {
    new Request('/', {get body() { throw e; }})}, 'body');
  assert_throws_exactly(e, () => {
    new Request('/', {get referrer() { throw e; }})}, 'referrer');
  assert_throws_exactly(e, () => {
    new Request('/', {get referrerPolicy() { throw e; }})}, 'referrerPolicy');
  assert_throws_exactly(e, () => {
    new Request('/', {get mode() { throw e; }})}, 'mode');
  assert_throws_exactly(e, () => {
    new Request('/', {get credentials() { throw e; }})}, 'credentials');
  assert_throws_exactly(e, () => {
    new Request('/', {get cache() { throw e; }})}, 'cache');
  assert_throws_exactly(e, () => {
    new Request('/', {get redirect() { throw e; }})}, 'redirect');
  assert_throws_exactly(e, () => {
    new Request('/', {get integrity() { throw e; }})}, 'integrity');
  assert_throws_exactly(e, () => {
    new Request('/', {get keepalive() { throw e; }})}, 'keepalive');
  assert_throws_exactly(e, () => {
    new Request('/', {get signal() { throw e; }})}, 'signal');

  // Not implemented
  // assert_throws_exactly(e, () => {
  //  new Request('/', {get window() { throw e; }})}, 'window');
}, 'Getter exceptions should not be silently ignored');


test(() => {
  // This is to test that a TypeError is thrown when RequestInit's signal
  // member does not implement the AbortSignal interface. We test this because
  // we used to use an `any` IDL type to represent RequestInit's signal member
  // instead of `AbortSignal` due to a bug in the IDL compiler, and performed
  // conversions manually. This test ensures that conversion were carried out
  // properly.
  assert_throws_js(TypeError, () => {
    new Request('/', {signal: {}})},
    'An empty object as RequestInit\'s signal member should fail type conversion');
  assert_throws_js(TypeError, () => {
    new Request('/', {signal: new Request('/')})},
    'A Request object as RequestInit\'s signal member should fail type conversion');
  assert_throws_js(TypeError, () => {
    new Request('/', {signal: new Response('/')})},
    'A Response object as RequestInit\'s signal member should fail type conversion');
}, 'TypeError should be thrown when RequestInit\'s signal member does not implement the AbortSignal interface');

promise_test(function() {
    var headers = new Headers;
    headers.set('Content-Language', 'ja');
    var req = new Request(URL, {
        method: 'POST',
        headers: headers,
        body: new Blob(['Test Blob'], {type: 'test/type'})
      });
    var req2 = req.clone();
    assert_false(req.bodyUsed);
    assert_false(req2.bodyUsed);
    // Change headers and of original request.
    req.headers.set('Content-Language', 'en');
    assert_equals(req2.headers.get('Content-Language'), 'ja',
                  'Headers of cloned request should not change when ' +
                  'original request headers are changed.');

    return req.text().then(function(text) {
        assert_equals(text, 'Test Blob', 'Body of request should match.');
        return req2.text();
      }).then(function(text) {
        assert_equals(text, 'Test Blob', 'Cloned request body should match.');
      });
  }, 'Test clone behavior with loading content from Request.');

async_test(function(t) {
    var request =
      new Request(URL,
                  {
                    method: 'POST',
                    body: new Blob(['Test Blob'], {type: 'test/type'})
                  });
    assert_equals(
      getContentType(request.headers), 'test/type',
      'ContentType header of Request created with Blob body must be set.');
    assert_false(request.bodyUsed,
                 'bodyUsed must be true before calling text()');
    request.text()
      .then(function(result) {
          assert_equals(result, 'Test Blob',
                        'Creating a Request with Blob body should success.');

          request = new Request(URL, {method: 'POST', body: 'Test String'});
          assert_equals(
            getContentType(request.headers), 'text/plain;charset=UTF-8',
            'ContentType header of Request created with string must be set.');
          return request.text();
        })
      .then(function(result) {
          assert_equals(result, 'Test String',
                        'Creating a Request with string body should success.');

          var text = 'Test ArrayBuffer';
          var array = new Uint8Array(text.length);
          for (var i = 0; i < text.length; ++i)
            array[i] = text.charCodeAt(i);
          request = new Request(URL, {method: 'POST', body: array.buffer});
          return request.text();
        })
      .then(function(result) {
          assert_equals(
            result, 'Test ArrayBuffer',
            'Creating a Request with ArrayBuffer body should success.');

          var text = 'Test ArrayBufferView';
          var array = new Uint8Array(text.length);
          for (var i = 0; i < text.length; ++i)
            array[i] = text.charCodeAt(i);
          request = new Request(URL, {method: 'POST', body: array});
          return request.text();
        })
      .then(function(result) {
          assert_equals(
            result, 'Test ArrayBufferView',
            'Creating a Request with ArrayBuffer body should success.');

          var formData = new FormData();
          formData.append('sample string', '1234567890');
          formData.append('sample blob', new Blob(['blob content']));
          formData.append('sample file',
                          new File(['file content'], 'file.dat'));
          request = new Request(URL, {method: 'POST', body: formData});
          return request.text();
        })
      .then(function(result) {
          var reg = new RegExp('multipart\/form-data; boundary=(.*)');
          var regResult = reg.exec(getContentType(request.headers));
          var boundary = regResult[1];
          var expected_body =
            '--' + boundary + '\r\n' +
            'Content-Disposition: form-data; name="sample string"\r\n' +
            '\r\n' +
            '1234567890\r\n' +
            '--' + boundary + '\r\n' +
            'Content-Disposition: form-data; name="sample blob"; ' +
            'filename="blob"\r\n' +
            'Content-Type: application/octet-stream\r\n' +
            '\r\n' +
            'blob content\r\n' +
            '--' + boundary + '\r\n' +
            'Content-Disposition: form-data; name="sample file"; ' +
            'filename="file.dat"\r\n' +
            'Content-Type: application/octet-stream\r\n' +
            '\r\n' +
            'file content\r\n' +
            '--' + boundary + '--\r\n';
          assert_equals(
            result, expected_body,
            'Creating a Request with FormData body should success.');
        })
      .then(function() {
          var params = new URLSearchParams();
          params.append('sample string', '1234567890');
          request = new Request(URL, {method: 'POST', body: params});
          return request.text();
        })
      .then(function(result) {
          assert_equals(result, "sample+string=1234567890");
        })
      .then(function() {
          // Alphanumeric characters and *-._ shouldn't be percent-encoded.
          // The others must.
          var params = new URLSearchParams();
          params.append('\0\x1f!)*+,-./:?[^_{~\x7f\u0080',
                        '\0\x1f!)*+,-./:?[^_{~\x7f\u0080');
          request = new Request(URL, {method: 'POST', body: params});
          return request.text();
        })
      .then(function(result) {
          assert_equals(
              result,
              "%00%1F%21%29*%2B%2C-.%2F%3A%3F%5B%5E_%7B%7E%7F%C2%80=" +
                  "%00%1F%21%29*%2B%2C-.%2F%3A%3F%5B%5E_%7B%7E%7F%C2%80");
        })
      .then(function() {
          // CR and LF shouldn't be normalized into CRLF.
          var params = new URLSearchParams();
          params.append('\r \n \r\n', '\r \n \r\n');
          request = new Request(URL, {method: 'POST', body: params});
          return request.text();
        })
      .then(function(result) {
          assert_equals(result, "%0D+%0A+%0D%0A=%0D+%0A+%0D%0A");
        })
      .then(function() {
          t.done();
        })
      .catch(unreached_rejection(t));
    assert_true(request.bodyUsed,
                'bodyUsed must be true after calling text()');
  }, 'Request body test');

test(function() {
    // https://fetch.spec.whatwg.org/#dom-request
    // Step 32:
    // Fill r's Headers object with headers. Rethrow any exceptions.
    INVALID_HEADER_NAMES.forEach(function(name) {
        assert_throws_js(
          TypeError,
          function() {
            var obj = {};
            obj[name] = 'a';
            new Request('http://localhost/', {headers: obj});
          },
          'new Request with headers with an invalid name (' + name +
          ') should throw');
        assert_throws_js(
          TypeError,
          function() {
            new Request('http://localhost/', {headers: [[name, 'a']]});
          },
          'new Request with headers with an invalid name (' + name +
          ') should throw');
      });

    INVALID_HEADER_VALUES.forEach(function(value) {
        assert_throws_js(
          TypeError,
          function() {
            new Request('http://localhost/',
                         {headers: {'X-Fetch-Test': value}});
          },
          'new Request with headers with an invalid value should throw');
        assert_throws_js(
          TypeError,
          function() {
            new Request('http://localhost/',
                         {headers: [['X-Fetch-Test', value]]});
          },
          'new Request with headers with an invalid value should throw');
      });
  }, 'Request throw error test');

// Tests for MIME types.
promise_test(function(t) {
    var req = new Request('http://localhost/',
                          {method: 'POST', body: new Blob([''])});
    return req.blob()
      .then(function(blob) {
          assert_equals(blob.type, '');
          assert_equals(req.headers.get('Content-Type'), null);
        });
  }, 'MIME type for Blob');

promise_test(function(t) {
    var req = new Request('http://localhost/',
                          {method: 'POST',
                           body: new Blob([''], {type: 'Text/Plain'})});
    return req.blob()
      .then(function(blob) {
          assert_equals(blob.type, 'text/plain');
          assert_equals(req.headers.get('Content-Type'), 'text/plain');
        });
  }, 'MIME type for Blob with non-empty type');

promise_test(function(t) {
    var req = new Request('http://localhost/',
                          {method: 'POST', body: new FormData()});
    return req.blob()
      .then(function(blob) {
          assert_equals(blob.type.indexOf('multipart/form-data; boundary='),
                        0);
          assert_equals(req.headers.get('Content-Type')
                          .indexOf('multipart/form-data; boundary='),
                        0);
        });
  }, 'MIME type for FormData');

promise_test(function(t) {
    var req = new Request('http://localhost/',
                          {method: 'POST', body: ''});
    return req.blob()
      .then(function(blob) {
          assert_equals(blob.type, 'text/plain;charset=utf-8');
          assert_equals(req.headers.get('Content-Type'),
                        'text/plain;charset=UTF-8');
        });
  }, 'MIME type for USVString');

promise_test(function(t) {
    var req = new Request('http://localhost/',
                          {method: 'POST',
                           body: new Blob([''], {type: 'Text/Plain'}),
                           headers: [['Content-Type', 'Text/Html']]});
    var clone = req.clone();
    return Promise.all([req.blob(), clone.blob()])
      .then(function(blobs) {
          assert_equals(blobs[0].type, 'text/html');
          assert_equals(blobs[1].type, 'text/html');
          assert_equals(req.headers.get('Content-Type'), 'Text/Html');
          assert_equals(clone.headers.get('Content-Type'), 'Text/Html');
        });
  }, 'Extract a MIME type with clone');

promise_test(function(t) {
    var req = new Request('http://localhost/',
                          {method: 'POST',
                           body: new Blob([''], {type: 'Text/Plain'})});
    req.headers.set('Content-Type', 'Text/Html');
    return req.blob()
      .then(function(blob) {
          assert_equals(blob.type, 'text/plain');
          assert_equals(req.headers.get('Content-Type'), 'Text/Html');
        });
  },
  'MIME type unchanged if headers are modified after Request() constructor');

promise_test(function(t) {
    var req = new Request('http://localhost/',
                          {method: 'POST',
                           body: new Blob([''], {type: 'Text/Plain'}),
                           headers: [['Content-Type', 'Text/Html']]});
    return req.blob()
      .then(function(blob) {
          assert_equals(blob.type, 'text/html');
          assert_equals(req.headers.get('Content-Type'), 'Text/Html');
        });
  }, 'Extract a MIME type (1)');

promise_test(function(t) {
    var req = new Request('http://localhost/',
                          {method: 'POST',
                           credentials: 'include',
                           body: 'this is a body'});

    return req.text()
        .then(t => {
           assert_equals(t, 'this is a body');
        });
  }, 'Credentials and body can both be set.');

done();
