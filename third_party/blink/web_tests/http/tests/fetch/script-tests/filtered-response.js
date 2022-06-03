// OPTIONS: ,-other-https,-base-https-other-https
if (self.importScripts) {
  importScripts('../resources/fetch-test-helpers.js');
}

// Spec: https://fetch.spec.whatwg.org/#concept-filtered-response

var {OTHER_ORIGIN} = get_fetch_test_options();
var base_url = '../resources/filtered-response.php';
var other_url = OTHER_ORIGIN + '/fetch/resources/filtered-response.php';

function check_headers(headers,
                       headers_must_exist,
                       headers_must_not_exist,
                       allow_other_headers) {
  headers_must_exist.forEach(function(header) {
      assert_equals(headers.get(header[0]), header[1],
                    header[0] + ' header must exist and match');
    });
  headers_must_not_exist.forEach(function(header) {
      assert_equals(headers.get(header), null,
                    header + ' header must not exist');
    });
  if (!allow_other_headers) {
    assert_equals(size(headers), headers_must_exist.length,
                  'Number of headers should be ' +
                  headers_must_exist.length);
  }
}

// Headers not filtered in basic/CORS filtered response
var headers_common = [
  ['cAche-cOntrol', 'private, no-store, no-cache, must-revalidate'],
  ['cOntent-lAnguage', 'test-content-language'],
  ['cOntent-lEngth', '8'],  // size of response body "Success."
  ['cOntent-tYpe', 'test-content-type'],
  ['eXpires', 'test-expires'],
  ['lAst-mOdified', 'test-last-modified'],
  ['pRagma', 'test-pragma']
];

var headers_basic = headers_common.concat([
  ['x-tEst', 'test-x-test'],
  ['x-tEst2', 'test-x-test2'],
  ['Access-Control-Allow-Origin', '*']
]);

// Headers to be filtered out in basic filtered response
var headers_cookies = ['sEt-cOokie', 'sEt-cOokie2'];

// basic filtered response
['same-origin', 'cors'].forEach(function(mode) {
    promise_test(function(t) {
        return fetch(base_url, {mode: mode})
          .then(function(response) {
              assert_equals(response.type, 'basic');
              check_headers(response.headers, headers_basic, headers_cookies,
                            true);
            });
      }, 'Basic filtered response with mode=' + mode);
  });

// CORS filtered response
promise_test(function() {
    return fetch(other_url, {mode: 'cors'})
      .then(function(response) {
          check_headers(response.headers, headers_common, [], false);
        });
  }, 'CORS filtered response');

promise_test(function() {
    // Access-Control-Expose-Headers with a single header name
    return fetch(other_url + '?ACEHeaders=x-teSt', {mode: 'cors'})
      .then(function(response) {
          assert_equals(response.type, 'cors');
          check_headers(response.headers,
                        headers_common.concat([['x-tEst', 'test-x-test']]),
                        [],
                        false);

          // Access-Control-Expose-Headers with multiple header names
          return fetch(other_url + '?ACEHeaders=x-teSt,x-teSt2',
                       {mode: 'cors'});
        })
      .then(function(response) {
          assert_equals(response.type, 'cors');
          check_headers(response.headers,
                        headers_common.concat([['x-tEst', 'test-x-test'],
                                               ['x-tEst2', 'test-x-test2']]),
                        [],
                        false);

          // Access-Control-Expose-Headers with an invalid header name
          return fetch(other_url + '?ACEHeaders=x-teSt x-teSt2',
                       {mode: 'cors'});
        })
      .then(function(response) {
          assert_equals(response.type, 'cors');
          check_headers(response.headers, headers_common, [], false);

          // Access-Control-Expose-Headers=Set-Cookie
          return fetch(other_url + '?ACEHeaders=sEt-cOokie', {mode: 'cors'});
        })
      .then(function(response) {
          // Set-Cookie header is omitted because Headers guard is response
          assert_equals(response.type, 'cors');
          check_headers(response.headers, headers_common, [], false);

          // Access-Control-Expose-Headers=Access-Control-Expose-Headers
          return fetch(other_url + '?ACEHeaders=acCess-coNtrol-exPose-heAders',
                       {mode: 'cors'});
        })
      .then(function(response) {
          assert_equals(response.type, 'cors');
          check_headers(response.headers,
                        headers_common.concat(
                          [['aCcess-cOntrol-eXpose-hEaders',
                            'acCess-coNtrol-exPose-heAders']]),
                        [],
                        false);
        });
  }, 'CORS filtered response with Access-Control-Expose-Headers');

// Opaque filtered response is tested in thorough tests.

done();
