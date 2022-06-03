'use strict';
if (self.importScripts) {
  importScripts('/fetch/resources/fetch-test-helpers.js');
}

const {BASE_ORIGIN, OTHER_ORIGIN} = get_fetch_test_options();
const BASE_URL = BASE_ORIGIN + '/fetch/resources/referrer.php';
const OTHER_URL = OTHER_ORIGIN + '/fetch/resources/referrer.php';
const REFERRER_SOURCE = location.href;

// There are more tests in referrer/ directory.
const TESTS = [
  [BASE_URL, 'about:client', '', REFERRER_SOURCE],
  [BASE_URL, 'about:client', 'no-referrer', '[no-referrer]'],
  [BASE_URL, 'about:client', 'no-referrer-when-downgrade', REFERRER_SOURCE],
  [BASE_URL, 'about:client', 'origin', BASE_ORIGIN + '/'],
  [BASE_URL, 'about:client', 'origin-when-cross-origin', REFERRER_SOURCE],
  [BASE_URL, 'about:client', 'unsafe-url', REFERRER_SOURCE],
  [OTHER_URL, 'about:client', '', BASE_ORIGIN + '/'],
  [OTHER_URL, 'about:client', 'no-referrer', '[no-referrer]'],
  [OTHER_URL, 'about:client', 'no-referrer-when-downgrade', REFERRER_SOURCE],
  [OTHER_URL, 'about:client', 'origin', BASE_ORIGIN + '/'],
  [OTHER_URL, 'about:client', 'origin-when-cross-origin', BASE_ORIGIN + '/'],
  [OTHER_URL, 'about:client', 'strict-origin-when-cross-origin', BASE_ORIGIN + '/'],
  [OTHER_URL, 'about:client', 'unsafe-url', REFERRER_SOURCE],

  [BASE_URL, '', '', '[no-referrer]'],
  [BASE_URL, '', 'no-referrer', '[no-referrer]'],
  [BASE_URL, '', 'no-referrer-when-downgrade', '[no-referrer]'],
  [BASE_URL, '', 'origin', '[no-referrer]'],
  [BASE_URL, '', 'origin-when-cross-origin', '[no-referrer]'],
  [BASE_URL, '', 'unsafe-url', '[no-referrer]'],
  [OTHER_URL, '', '', '[no-referrer]'],
  [OTHER_URL, '', 'no-referrer', '[no-referrer]'],
  [OTHER_URL, '', 'no-referrer-when-downgrade', '[no-referrer]'],
  [OTHER_URL, '', 'origin', '[no-referrer]'],
  [OTHER_URL, '', 'origin-when-cross-origin', '[no-referrer]'],
  [OTHER_URL, '', 'unsafe-url', '[no-referrer]'],

  [BASE_URL, '/foo', '', BASE_ORIGIN + '/foo'],
  [BASE_URL, '/foo', 'no-referrer', '[no-referrer]'],
  [BASE_URL, '/foo', 'no-referrer-when-downgrade', BASE_ORIGIN + '/foo'],
  [BASE_URL, '/foo', 'origin', BASE_ORIGIN + '/'],
  [BASE_URL, '/foo', 'origin-when-cross-origin', BASE_ORIGIN + '/foo'],
  [BASE_URL, '/foo', 'unsafe-url', BASE_ORIGIN + '/foo'],
  [OTHER_URL, '/foo', '', BASE_ORIGIN + '/'],
  [OTHER_URL, '/foo', 'no-referrer', '[no-referrer]'],
  [OTHER_URL, '/foo', 'no-referrer-when-downgrade', BASE_ORIGIN + '/foo'],
  [OTHER_URL, '/foo', 'origin', BASE_ORIGIN + '/'],
  [OTHER_URL, '/foo', 'origin-when-cross-origin', BASE_ORIGIN + '/'],
  [OTHER_URL, '/foo', 'strict-origin-when-cross-origin', BASE_ORIGIN + '/'],
  [OTHER_URL, '/foo', 'unsafe-url', BASE_ORIGIN + '/foo'],

  [BASE_URL,
    (BASE_URL + '/path#fragment?query#hash').replace('//', '//user:pass@'),
    'unsafe-url',
    BASE_URL + '/path'],
];

add_referrer_tests(TESTS);
done();
