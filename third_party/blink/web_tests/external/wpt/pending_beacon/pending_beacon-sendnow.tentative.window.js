// META: script=/resources/testharness.js
// META: script=/resources/testharnessreport.js
// META: script=/common/utils.js
// META: script=./resources/pending_beacon-helper.js

'use strict';

promise_test(async t => {
  const uuid = token();
  const url = generateSetBeaconCountURL(uuid);

  // Create and send a beacon.
  const beacon = new PendingGetBeacon(url);
  beacon.sendNow();

  await expectBeaconCount(uuid, 1);
}, 'sendNow() sends a beacon immediately.');

promise_test(async t => {
  const uuid = token();
  const url = generateSetBeaconCountURL(uuid);

  // Create and send a beacon.
  const beacon = new PendingGetBeacon(url);
  beacon.sendNow();
  await expectBeaconCount(uuid, 1);

  // Try to send the beacon again, and verify no beacon arrives.
  beacon.sendNow();
  await expectBeaconCount(uuid, 1);
}, 'sendNow() doesn\'t send the same beacon twice.');

promise_test(async t => {
  const uuid = token();
  const url = generateSetBeaconCountURL(uuid);

  // Create and send 1st beacon.
  const beacon1 = new PendingGetBeacon(url);
  beacon1.sendNow();

  // Create and send 2st beacon.
  const beacon2 = new PendingGetBeacon(url);
  beacon2.sendNow();

  await expectBeaconCount(uuid, 2);
}, 'sendNow() sends multiple beacons.');
