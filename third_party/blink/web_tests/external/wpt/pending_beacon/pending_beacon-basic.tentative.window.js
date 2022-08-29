// META: script=/resources/testharness.js
// META: script=/resources/testharnessreport.js
// META: script=./resources/pending_beacon-helper.js

'use strict';

test(() => {
  assert_throws_js(TypeError, () => new PendingBeacon('/'));
}, `PendingBeacon cannot be constructed directly.`);

for (const beaconType of BeaconTypes) {
  test(() => {
    assert_throws_js(TypeError, () => new beaconType.type());
  }, `${beaconType.name}: constructor throws TypeError if url is missing`);

  test(() => {
    const beacon = new beaconType.type('/');
    assert_equals(beacon.url, '/');
    assert_equals(beacon.method, beaconType.expectedMethod);
    assert_equals(beacon.backgroundTimeout, -1);
    assert_equals(beacon.timeout, -1);
    assert_true(beacon.pending);
  }, `${beaconType.name}: constructor sets default properties if missing.`);

  test(() => {
    const beacon = new beaconType.type(
        'https://www.google.com', {backgroundTimeout: 200, timeout: 100});
    assert_equals(beacon.url, 'https://www.google.com');
    assert_equals(beacon.method, beaconType.expectedMethod);
    assert_equals(beacon.backgroundTimeout, 200);
    assert_equals(beacon.timeout, 100);
    assert_true(beacon.pending);
  }, `${beaconType.name}: constructor can set properties.`);

  test(() => {
    let beacon = new beaconType.type(
        'https://www.google.com',
        {method: 'GET', backgroundTimeout: 200, timeout: 100});

    beacon.backgroundTimeout = 400;
    assert_equals(beacon.backgroundTimeout, 400);

    beacon.timeout = 600;
    assert_equals(beacon.timeout, 600);
  }, `${beaconType.name}: 'backgroundTimeout' & 'timeout' can be mutated.`);

  test(
      () => {
        let beacon = new beaconType.type('https://www.google.com');

        assert_throws_js(TypeError, () => beacon.url = '/');
        assert_throws_js(TypeError, () => beacon.method = 'FOO');
        assert_throws_js(TypeError, () => beacon.pending = false);
      },
      `${beaconType.name}: throws TypeError when mutating ` +
          `'url', 'method', 'pending'.`);
}
