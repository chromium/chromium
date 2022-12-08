if (self.importScripts) {
  importScripts('/resources/get-host-info.js?pipe=sub');
  importScripts('/resources/testharness.js');
  importScripts('/serviceworker/resources/test-helpers.js');
  importScripts('/fetch/resources/fetch-test-options.js');
}

function getContentType(headers) {
  var content_type = '';
  for (var header of headers) {
    if (header[0] == 'content-type')
      content_type = header[1];
  }
  return content_type;
}

// token [RFC 2616]
// "token          = 1*<any CHAR except CTLs or separators>"
// All octets are tested except for those >= 0x80.
var INVALID_TOKENS = [
  '',
  // CTL
  '\x00', '\x01', '\x02', '\x03', '\x04', '\x05', '\x06', '\x07',
  '\x08', '\x09', '\x0a', '\x0b', '\x0c', '\x0d', '\x0e', '\x0f',
  '\x10', '\x11', '\x12', '\x13', '\x14', '\x15', '\x16', '\x17',
  '\x18', '\x19', '\x1a', '\x1b', '\x1c', '\x1d', '\x1e', '\x1f', '\x7f',
  // separators
  ' ', '"', '(', ')', ',', '/', ':', ';', '<', '=', '>', '?', '@', '[', '\\',
  ']', '{', '}',
  // non-CHAR
  '\x80', '\xff', '\u0100', '\u3042',
  // Strings that contain characters above.
  'a(b', 'invalid name', 'invalid \r name', 'invalid \n name',
  'invalid\r\n name', 'invalid \0 name',
  'test\r', 'test\n', 'test\r\n', 'test\0',
  '\0'.repeat(100000), '<'.repeat(100000), '\r\n'.repeat(50000),
  'x'.repeat(100000) + '\0'];

// Method names.

// A method name must match token in RFC 2616:
// Fetch API Spec: https://fetch.spec.whatwg.org/#concept-method
var INVALID_METHOD_NAMES = INVALID_TOKENS;

// Spec: https://fetch.spec.whatwg.org/#forbidden-method
// "A forbidden method is a method that is a byte case-insensitive match for
// one of `CONNECT`, `TRACE`, and `TRACK`."
var FORBIDDEN_METHODS = ['TRACE', 'TRACK', 'CONNECT',
                         'trace', 'track', 'connect'];

// Spec: https://fetch.spec.whatwg.org/#concept-method-normalize
// "To normalize a method, if it is a byte case-insensitive match for
// `DELETE`, `GET`, `HEAD`, `OPTIONS`, `POST`, or `PUT`, byte uppercase it"
var TO_BE_NORMALIZED_METHOD_NAMES = [
  'delete', 'get', 'head', 'options', 'post', 'put'];

var OTHER_VALID_METHOD_NAMES = [
  '!', '#', '$', '%', '&', '\'', '*', '+', '-', '.', '^', '_', '`', '|', '~',
  '0123456789', 'PATCH', 'MKCOL', 'CUSTOM', 'X-FILES', 'p0sT', 'AZaz',
  'x'.repeat(100000)];

var VALID_TOKENS = FORBIDDEN_METHODS
  .concat(TO_BE_NORMALIZED_METHOD_NAMES)
  .concat(OTHER_VALID_METHOD_NAMES);

// Header names and values.
// Header names are divided into the following mutually-exclusive categories:
// - not a name (INVALID_HEADER_NAMES)
// - forbidden header names (FORBIDDEN_HEADER_NAMES)
// - forbidden response header names (FORBIDDEN_RESPONSE_HEADER_NAMES)
// - names of simple headers, except for "Content-Type" (SIMPLE_HEADER_NAMES)
// - "Content-Type" (CONTENT_TYPE): This can be
//   a simple header if the header value is in SIMPLE_HEADER_CONTENT_TYPE_VALUES
//   or not if the header value is in NON_SIMPLE_HEADER_CONTENT_TYPE_VALUES.
// - others (NON_SIMPLE_HEADER_NAMES)

// A header name must match token in RFC 2616.
// Fetch API Spec: https://fetch.spec.whatwg.org/#concept-header-name
var INVALID_HEADER_NAMES = INVALID_TOKENS;

// A header value is a byte sequence that matches the following conditions:
// * Has no leading or trailing HTTP whitespace bytes (0x09, 0x0A, 0x0D and
//   0x20).
// * Contains no 0x00, 0x0A or 0x0D bytes.
// Note that header values are normalized (ie. stripped of leading and trailing
// HTTP whitespace bytes) when a new header/value pair is appended or set.
var INVALID_HEADER_VALUES = [
  'test \r data', 'test \n data', 'test \0 data',
  'test\r\n data',
  'test\0',
  '\0'.repeat(100000), 'x'.repeat(100000) + '\0'];

var FORBIDDEN_HEADER_NAMES = [
  'Accept-Charset',
  'Accept-Encoding',
  'Access-Control-Request-Headers',
  'Access-Control-Request-Method',
  'Access-Control-Request-Private-Network',
  'Connection',
  'Content-Length',
  'Cookie',
  'Cookie2',
  'Date',
  'DNT',
  'Expect',
  'Host',
  'Keep-Alive',
  'Origin',
  'Referer',
  'Set-Cookie',
  'TE',
  'Trailer',
  'Transfer-Encoding',
  'Upgrade',
  'User-Agent',
  'Via',
  'Proxy-',
  'Sec-',
  'Proxy-FooBar',
  'Sec-FooBar'
];
var FORBIDDEN_RESPONSE_HEADER_NAMES =
  ['Set-Cookie', 'Set-Cookie2',
   'set-cookie', 'set-cookie2',
   'set-cOokie', 'set-cOokie2',
   'sEt-cookie', 'sEt-cookie2'];
var SIMPLE_HEADER_NAMES = ['Accept', 'Accept-Language', 'Content-Language'];
var CONTENT_TYPE = 'Content-Type';
var NON_SIMPLE_HEADER_NAMES = ['X-Fetch-Test', 'X-Fetch-Test2'];

var SIMPLE_HEADER_CONTENT_TYPE_VALUES =
  ['application/x-www-form-urlencoded',
   'multipart/form-data',
   // MIME types are case-insensitive.
   'multiPart/foRm-data',
   // MIME-type parameters are ignored when determining simple headers.
   'multiPart/foRm-data;charset=utf-8',
   'text/plain'];
var NON_SIMPLE_HEADER_CONTENT_TYPE_VALUES = ['foo/bar'];

// ResponseInit's statusText must match Reason-Phrase.
// https://fetch.spec.whatwg.org/#dom-response Step 2.
var INVALID_REASON_PHRASE = [
  // \x00-\x1F (except for \t) and \x7f are invalid.
  '\x00', '\x01', '\x02', '\x03', '\x04', '\x05', '\x06', '\x07',
  '\x08', '\x0a', '\x0b', '\x0c', '\x0d', '\x0e', '\x0f',
  '\x10', '\x11', '\x12', '\x13', '\x14', '\x15', '\x16', '\x17',
  '\x18', '\x19', '\x1a', '\x1b', '\x1c', '\x1d', '\x1e', '\x1f', '\x7f',
  // non-ByteString.
  '\u0100', '\u3042',
  // Strings that contain characters above.
  'invalid \r reason-phrase', 'invalid \n reason-phrase',
  'invalid \0 reason-phrase', 'invalid\r\n reason-phrase',
  'test\r', 'test\n', 'test\r\n', 'test\0',
  '\0'.repeat(100000), '\r\n'.repeat(50000),
  'x'.repeat(100000) + '\0'];

var VALID_REASON_PHRASE = [
  '\t', ' ', '"', '(', ')', ',', '/', ':', ';', '<', '=', '>', '?', '@', '[',
  '\\', ']', '{', '}',
  '!', '#', '$', '%', '&', '\'', '*', '+', '-', '.', '^', '_', '`', '|', '~',
  // non-CHAR
  '\x80', '\xff',
  // Valid strings.
  '', '0123456789', '404 Not Found', 'HTTP/1.1 404 Not Found', 'AZ\u00ffaz',
  'x'.repeat(100000)];

var INVALID_URLS =
  ['http://',
   'https://',
   'http://ex%00ample.com',
   'http://ex%0dample.com',
   'http://ex%0aample.com',
   'http://ex%08ample.com',
   'http://ex\x00ample.com'];

function size(headers) {
  var count = 0;
  for (var header of headers) {
    ++count;
  }
  return count;
}

function testBlockMixedContent(mode) {
  promise_test(t => {
      // Must fail: blocked as mixed content.
      return fetch(BASE_URL + 'test1-' + mode, {mode: mode})
       .then(unreached_fulfillment(t), () => {});
    }, `Mixed content fetch (${mode}, HTTPS->HTTP)`);

  promise_test(t => {
      // Must fail: original fetch is not blocked but redirect is blocked.
      return fetch(HTTPS_REDIRECT_URL +
                   encodeURIComponent(BASE_URL + 'test2-' + mode), {mode: mode})
        .then(unreached_fulfillment(t), () => {});
    }, `Mixed content redirect (${mode}, HTTPS->HTTPS->HTTP)`);

  promise_test(t => {
      // Must fail: original fetch is blocked.
      return fetch(REDIRECT_URL +
                   encodeURIComponent(HTTPS_BASE_URL + 'test3-' + mode),
                   {mode: mode}).then(unreached_fulfillment(t), () => {});
    }, `Mixed content redirect ${mode}, HTTPS->HTTP->HTTPS)`);

  promise_test(() => {
      // Must success.
      // Test that the mixed contents above are not rejected due to CORS check.
      return fetch(HTTPS_REDIRECT_URL +
                   encodeURIComponent(HTTPS_BASE_URL + 'test4-' + mode),
                   {mode: mode}).then(res => {
          assert_equals(res.status, mode === 'no-cors' ? 0 : 200);
        });
    }, `Same origin redirect (${mode}, HTTPS->HTTPS->HTTPS)`);

  promise_test(() => {
      // Must success if mode is not 'same-origin'.
      // Test that the mixed contents above are not rejected due to CORS check.
      return fetch(HTTPS_OTHER_REDIRECT_URL +
                   encodeURIComponent(HTTPS_BASE_URL + 'test5-' + mode),
                   {mode: mode})
        .then(res => {
            assert_not_equals(mode, 'same-origin');
          }, () => {
            assert_equals(mode, 'same-origin');
          });
    }, `Cross origin redirect (${mode}, HTTPS->HTTPS->HTTPS`);
}

function add_referrer_tests(tests, global) {
  global = global || self;
  for (let test of tests) {
    let url = test[0];
    let referrer = test[1];
    let policy = test[2];
    let expected = test[3];
    promise_test(t => {
        var request = new Request(url,
          {referrer: referrer, referrerPolicy: policy, mode: 'cors'});
        return global.fetch(new Request(url, request)).then(res => {
            return res.json();
          }).then(json => {
            assert_equals(json.referrer, expected, 'referrer');
          });
     },
     `referrer test: url = ${url}, referrer = ${referrer}, policy = ${policy}`);
  }
}
